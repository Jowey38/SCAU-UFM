#include "coupling/driver/tri_coupling.hpp"

#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>
#include <utility>

namespace scau::coupling::driver {
namespace {

constexpr std::size_t kMaxNodeId = static_cast<std::size_t>(std::numeric_limits<int>::max());

void validate_config(
    const TriCouplingStepConfig& config,
    double dt_sub,
    std::size_t surface_cell_count) {
    if (!std::isfinite(dt_sub) || dt_sub <= 0.0) {
        throw std::invalid_argument("tri coupling dt_sub must be finite and positive");
    }
    if (config.river_lateral_discharge_variable.empty()) {
        throw std::invalid_argument("river_lateral_discharge_variable must not be empty");
    }
    if (config.river_water_level_variable.empty()) {
        throw std::invalid_argument("river_water_level_variable must not be empty");
    }

    std::map<int, int> drainage_node_use;
    for (const auto& link : config.surface_drainage) {
        if (link.cell_index >= surface_cell_count) {
            throw std::invalid_argument(
                "surface-drainage link cell_index is out of range");
        }
        if (link.node_id < 0) {
            throw std::invalid_argument("surface-drainage link node_id must be non-negative");
        }
        if (!std::isfinite(link.q_request) || link.q_request < 0.0) {
            throw std::invalid_argument(
                "surface-drainage link q_request must be finite and non-negative");
        }
        if (!std::isfinite(link.priority_weight) || link.priority_weight <= 0.0) {
            throw std::invalid_argument(
                "surface-drainage link priority_weight must be finite and positive");
        }
        if (link.geometry.has_value() && !std::isfinite(link.surface_water_level)) {
            throw std::invalid_argument(
                "surface-drainage link surface_water_level must be finite in head-driven mode");
        }
        if (++drainage_node_use[link.node_id] > 1) {
            throw std::invalid_argument(
                "surface-drainage links must not reuse the same SWMM node");
        }
    }

    std::map<int, int> river_location_use;
    for (const auto& link : config.surface_river) {
        if (link.cell_index >= surface_cell_count) {
            throw std::invalid_argument(
                "surface-river link cell_index is out of range");
        }
        if (link.location_id < 0) {
            throw std::invalid_argument("surface-river link location_id must be non-negative");
        }
        if (!std::isfinite(link.q_request) || link.q_request < 0.0) {
            throw std::invalid_argument(
                "surface-river link q_request must be finite and non-negative");
        }
        if (!std::isfinite(link.priority_weight) || link.priority_weight <= 0.0) {
            throw std::invalid_argument(
                "surface-river link priority_weight must be finite and positive");
        }
        if (!std::isfinite(link.q_return) || link.q_return < 0.0) {
            throw std::invalid_argument(
                "surface-river link q_return must be finite and non-negative");
        }
        if (link.geometry.has_value() && !std::isfinite(link.surface_water_level)) {
            throw std::invalid_argument(
                "surface-river link surface_water_level must be finite in head-driven mode");
        }
        if (++river_location_use[link.location_id] > 1) {
            throw std::invalid_argument(
                "surface-river links must not reuse the same river location");
        }
    }

    std::map<int, int> outfall_use;
    std::map<int, int> interface_river_location_use;
    for (const auto& link : config.drainage_river) {
        if (link.outfall_node_id < 0) {
            throw std::invalid_argument(
                "drainage-river link outfall_node_id must be non-negative");
        }
        if (link.river_location_id < 0) {
            throw std::invalid_argument(
                "drainage-river link river_location_id must be non-negative");
        }
        if (!std::isfinite(link.q_capacity) || link.q_capacity < 0.0) {
            throw std::invalid_argument(
                "drainage-river link q_capacity must be finite and non-negative");
        }
        if (++outfall_use[link.outfall_node_id] > 1) {
            throw std::invalid_argument(
                "drainage-river links must not reuse the same SWMM outfall");
        }
        if (++interface_river_location_use[link.river_location_id] > 1) {
            throw std::invalid_argument(
                "drainage-river links must not reuse the same river location");
        }
    }
}

}  // namespace

TriCouplingStepReport advance_tri_coupling_step(
    core::CouplingState& state,
    drainage::ISwmmEngine& swmm,
    river::IDFlowFMEngine& dflowfm,
    const TriCouplingStepConfig& config,
    double dt_sub,
    double h_wet) {
    validate_config(config, dt_sub, state.cells().size());

    TriCouplingStepReport report{};
    report.surface_mass_before = state.compute_system_mass(h_wet);

    // Phase 1: shared-cell arbitration of all surface -> engine drain intents.
    // Intents are grouped per 2D cell so competing engines on the same cell
    // are split by priority_weight against the cell's V_limit / Q_limit.
    // Head-driven links derive both direction and magnitude from the water
    // levels read here, before any engine state is written.
    std::vector<double> drainage_head_return(config.surface_drainage.size(), 0.0);
    std::vector<double> river_head_return(config.surface_river.size(), 0.0);

    std::map<std::size_t, std::vector<core::SharedExchangeIntent>> intents_per_cell;
    for (std::size_t i = 0; i < config.surface_drainage.size(); ++i) {
        const auto& link = config.surface_drainage[i];
        double q_request = link.q_request;
        if (link.geometry.has_value()) {
            const auto flow = core::compute_head_driven_exchange_flow(
                link.surface_water_level,
                swmm.get_node_head(link.node_id),
                *link.geometry);
            q_request = flow.q_surface_to_engine > 0.0 ? flow.q_surface_to_engine : 0.0;
            drainage_head_return[i] =
                flow.q_surface_to_engine < 0.0 ? -flow.q_surface_to_engine : 0.0;
        }
        intents_per_cell[link.cell_index].push_back(core::SharedExchangeIntent{
            .endpoint = {
                .engine = core::SharedExchangeEngine::drainage,
                .node_id = static_cast<std::size_t>(link.node_id),
            },
            .q_request = q_request,
            .priority_weight = link.priority_weight,
        });
    }
    for (std::size_t i = 0; i < config.surface_river.size(); ++i) {
        const auto& link = config.surface_river[i];
        double q_request = link.q_request;
        if (link.geometry.has_value()) {
            const auto flow = core::compute_head_driven_exchange_flow(
                link.surface_water_level,
                dflowfm.get_value(config.river_water_level_variable, link.location_id),
                *link.geometry);
            q_request = flow.q_surface_to_engine > 0.0 ? flow.q_surface_to_engine : 0.0;
            river_head_return[i] =
                flow.q_surface_to_engine < 0.0 ? -flow.q_surface_to_engine : 0.0;
        }
        intents_per_cell[link.cell_index].push_back(core::SharedExchangeIntent{
            .endpoint = {
                .engine = core::SharedExchangeEngine::river,
                .node_id = static_cast<std::size_t>(link.location_id),
            },
            .q_request = q_request,
            .priority_weight = link.priority_weight,
        });
    }

    std::map<int, double> drainage_lateral_inflow;
    std::map<int, double> river_lateral_discharge;
    for (const auto& [cell_index, intents] : intents_per_cell) {
        auto decisions = state.apply_shared_exchange(cell_index, intents, dt_sub);
        for (const auto& decision : decisions) {
            if (decision.endpoint.node_id > kMaxNodeId) {
                throw std::invalid_argument("shared exchange node_id is out of int range");
            }
            const int engine_node = static_cast<int>(decision.endpoint.node_id);
            const double q_accepted = decision.exchange.q_granted + decision.exchange.q_repay;
            if (decision.endpoint.engine == core::SharedExchangeEngine::drainage) {
                drainage_lateral_inflow[engine_node] += q_accepted;
            } else {
                river_lateral_discharge[engine_node] += q_accepted;
            }
            report.surface_decisions.push_back(decision);
        }
    }

    // Phase 2: 1D-1D interface exchange. The outfall discharge currently
    // arriving in the drainage engine is offered against the river acceptance
    // capacity; the core decision is the only thing either engine sees.
    for (const auto& link : config.drainage_river) {
        double q_outfall = swmm.get_node_inflow(link.outfall_node_id);
        if (!std::isfinite(q_outfall) || q_outfall < 0.0) {
            q_outfall = 0.0;
        }
        const auto decision = core::evaluate_engine_interface_exchange({
            .source = {
                .engine = core::SharedExchangeEngine::drainage,
                .node_id = static_cast<std::size_t>(link.outfall_node_id),
            },
            .target = {
                .engine = core::SharedExchangeEngine::river,
                .node_id = static_cast<std::size_t>(link.river_location_id),
            },
            .q_request = q_outfall,
            .q_capacity = link.q_capacity,
            .dt_sub = dt_sub,
        });
        river_lateral_discharge[link.river_location_id] += decision.q_granted;
        report.interface_decisions.push_back(decision);
    }

    // Phase 3: acceptance. Accumulated totals are written once per engine
    // endpoint; river water level drives the outfall boundary stage.
    for (const auto& [node_id, q_total] : drainage_lateral_inflow) {
        swmm.set_node_lateral_inflow(node_id, q_total);
    }
    for (const auto& [location_id, q_total] : river_lateral_discharge) {
        dflowfm.set_value(config.river_lateral_discharge_variable, location_id, q_total);
    }
    for (const auto& link : config.drainage_river) {
        if (link.drive_outfall_stage) {
            const double stage =
                dflowfm.get_value(config.river_water_level_variable, link.river_location_id);
            swmm.set_outfall_stage(link.outfall_node_id, stage);
        }
    }

    // Phase 4: engine stepping.
    if (config.step_engines) {
        swmm.step(dt_sub);
        dflowfm.update(dt_sub);
    }

    // Phase 5: return flows back onto the 2D surface (engine_to_surface),
    // then replay so the surface cells reflect this dt_sub. Pipe -> surface
    // combines node overflow (ponded spill) with the head-driven reverse
    // inlet flow computed in phase 1; river -> surface uses the head-driven
    // spill (or the explicit q_return in explicit mode).
    for (std::size_t i = 0; i < config.surface_drainage.size(); ++i) {
        const auto& link = config.surface_drainage[i];
        double q_overflow = swmm.get_node_overflow(link.node_id);
        if (!std::isfinite(q_overflow) || q_overflow < 0.0) {
            q_overflow = 0.0;
        }
        const double q_return_total = q_overflow + drainage_head_return[i];
        if (q_return_total > 0.0) {
            report.return_decisions.push_back(state.apply_return_exchange(
                link.cell_index,
                {
                    .source = {
                        .engine = core::SharedExchangeEngine::drainage,
                        .node_id = static_cast<std::size_t>(link.node_id),
                    },
                    .q_return = q_return_total,
                    .dt_sub = dt_sub,
                }));
        }
    }
    for (std::size_t i = 0; i < config.surface_river.size(); ++i) {
        const auto& link = config.surface_river[i];
        const double q_return =
            link.geometry.has_value() ? river_head_return[i] : link.q_return;
        if (q_return > 0.0) {
            report.return_decisions.push_back(state.apply_return_exchange(
                link.cell_index,
                {
                    .source = {
                        .engine = core::SharedExchangeEngine::river,
                        .node_id = static_cast<std::size_t>(link.location_id),
                    },
                    .q_return = q_return,
                    .dt_sub = dt_sub,
                }));
        }
    }

    state.replay_pending();
    report.surface_mass_after = state.compute_system_mass(h_wet);
    return report;
}

}  // namespace scau::coupling::driver

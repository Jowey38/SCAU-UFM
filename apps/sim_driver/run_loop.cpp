#include "run_loop.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "coupling/core/state.hpp"
#include "coupling/driver/surface2d_coupling_map.hpp"
#include "coupling/driver/tri_coupling.hpp"
#include "surface2d/boundary/conditions.hpp"
#include "surface2d/geometry/cache.hpp"
#include "surface2d/state/state.hpp"
#include "surface2d/stcf_bridge/load_case.hpp"
#include "surface2d/time_integration/step.hpp"

namespace scau::apps::sim_driver {

namespace {

namespace core = scau::coupling::core;
namespace driver_ns = scau::coupling::driver;
namespace s2d = scau::surface2d;

int resolve_node_name(const std::string& node_name, const RunLoopHooks& hooks) {
    if (hooks.resolve_swmm_node) {
        return hooks.resolve_swmm_node(node_name);
    }
    std::string digits = node_name;
    if (!digits.empty() && digits.front() == '-') {
        digits.erase(0, 1U);
    }
    if (digits.empty() || digits.find_first_not_of("0123456789") != std::string::npos) {
        throw std::invalid_argument(
            "mock mode requires integer SWMM node names, got '" + node_name + "'");
    }
    return std::stoi(node_name);
}

std::size_t epoch_count(const RuntimeConfig& config) {
    return static_cast<std::size_t>(
        std::llround((config.end_time - config.start_time) / config.dt_couple));
}

std::size_t surface_substep_count(const RuntimeConfig& config) {
    return static_cast<std::size_t>(std::llround(config.dt_couple / config.dt_surface));
}

double physical_surface_volume(const s2d::SurfaceState& state,
                               const s2d::DpmFields& dpm,
                               const s2d::GeometryCache& geometry) {
    double total = 0.0;
    for (std::size_t cell = 0U; cell < state.cells.size(); ++cell) {
        total += static_cast<double>(state.cells[cell].conserved.h) *
                 static_cast<double>(dpm.cells[cell].phi_t) *
                 static_cast<double>(geometry.cell_areas[cell]);
    }
    return total;
}

}  // namespace

RunLoopResult run_simulation(
    SimDriver& driver,
    coupling::drainage::ISwmmEngine& swmm,
    coupling::river::IDFlowFMEngine& dflowfm,
    const RunLoopHooks& hooks) {
    const RuntimeConfig& config = driver.config();
    if (!config.enable_swmm || !config.enable_dflowfm) {
        throw std::invalid_argument(
            "run_simulation is the tri-model loop; both engines must be enabled");
    }

    // Case loading and surface initialization (strict CF/UGRID path).
    const s2d::LoadedSurface2DCase loaded = s2d::load_surface2d_case(config.stcf_case_path);
    const s2d::GeometryCache geometry = s2d::GeometryCache::for_mesh(loaded.mesh);
    const s2d::BoundaryConditions boundary = s2d::BoundaryConditions::for_mesh(loaded.mesh);
    s2d::SurfaceState state = s2d::SurfaceState::for_mesh(loaded.mesh);
    for (std::size_t cell = 0U; cell < state.cells.size(); ++cell) {
        const double bed = static_cast<double>(loaded.bed_elevations[cell]);
        const double depth = std::max(0.0, config.initial_eta - bed);
        state.cells[cell].conserved.h = depth;
        state.cells[cell].conserved.hu = 0.0;
        state.cells[cell].conserved.hv = 0.0;
        state.cells[cell].eta = bed + depth;
    }

    // Exchange cell layout: all drainage links first, then all river links.
    driver_ns::Surface2DCouplingMap map{};
    std::vector<int> drainage_node_ids;
    drainage_node_ids.reserve(config.surface_drainage.size());
    for (const auto& link : config.surface_drainage) {
        map.surface_cells.push_back(link.cell);
        drainage_node_ids.push_back(resolve_node_name(link.node_name, hooks));
    }
    for (const auto& link : config.surface_river) {
        map.surface_cells.push_back(link.cell);
    }
    driver_ns::validate_surface2d_coupling_map(map, state, loaded.dpm_fields, geometry);

    std::vector<int> outfall_node_ids;
    outfall_node_ids.reserve(config.drainage_river.size());
    for (const auto& link : config.drainage_river) {
        outfall_node_ids.push_back(resolve_node_name(link.outfall_name, hooks));
    }

    driver.initialize();
    driver.start();

    RunLoopResult result{};
    RunSummary& summary = result.summary;
    const std::size_t n_epochs = epoch_count(config);
    const std::size_t n_surface = surface_substep_count(config);
    std::optional<core::CouplingState> previous_coupling{};

    const auto finish = [&](const char* outcome, const std::string& reason) {
        summary.outcome = outcome;
        summary.reason = reason;
        summary.committed_epochs = driver.completed_coupling_steps();
        summary.final_surface_physical_volume =
            physical_surface_volume(state, loaded.dpm_fields, geometry);
        summary.final_coupling_deficit_volume =
            previous_coupling.has_value()
                ? previous_coupling->compute_system_mass(config.h_wet).deficit_mass
                : 0.0;
        result.final_state = driver.state();
        result.committed_epochs = driver.completed_coupling_steps();
    };

    for (std::size_t epoch = 0U; epoch < n_epochs; ++epoch) {
        const double logical_time =
            config.start_time + static_cast<double>(epoch + 1U) * config.dt_couple;

        // 1. Surface substeps; engines advance only after all are accepted.
        double epoch_max_cfl = 0.0;
        for (std::size_t substep = 0U; substep < n_surface; ++substep) {
            s2d::StepConfig step_config{};
            step_config.dt = config.dt_surface;
            step_config.cfl_safety = config.cfl_safety;
            step_config.c_rollback = config.c_rollback;
            step_config.enable_cvc_spatial_phi_t_correction =
                config.enable_cvc_spatial_phi_t_correction;
            const s2d::StepDiagnostics diagnostics = s2d::advance_one_step_cpu(
                loaded.mesh, state, step_config, loaded.dpm_fields, boundary,
                loaded.source_fields, geometry);
            epoch_max_cfl = std::max(epoch_max_cfl,
                                     static_cast<double>(diagnostics.max_cell_cfl));
            summary.total_boundary_inflow_volume +=
                static_cast<double>(diagnostics.boundary_inflow_volume);
            if (diagnostics.rollback_required) {
                driver.require_review();
                finish("review_required",
                       "cfl_rollback at epoch " + std::to_string(epoch) +
                           " (max_cell_cfl exceeded c_rollback; surface state not advanced)");
                return result;
            }
        }

        // 2. Project the current surface into fresh exchange cells.
        const std::vector<core::ExchangeCellState> cells_before =
            driver_ns::build_exchange_cells(
                state, loaded.dpm_fields, geometry, map,
                previous_coupling.has_value() ? &*previous_coupling : nullptr);
        core::CouplingState coupling{cells_before};

        // 3. One coupled substep (head-driven links), dt_sub = dt_couple.
        driver_ns::TriCouplingStepConfig tri_config{};
        tri_config.river_water_level_variable = config.river_water_level_variable;
        tri_config.step_engines = true;
        for (std::size_t index = 0U; index < config.surface_drainage.size(); ++index) {
            const auto& link_config = config.surface_drainage[index];
            driver_ns::SurfaceDrainageLink link{};
            link.cell_index = index;
            link.node_id = drainage_node_ids[index];
            link.priority_weight = link_config.priority_weight;
            core::ExchangeFlowGeometry structure{};
            structure.crest_level = link_config.crest_level;
            structure.exchange_width = link_config.exchange_width;
            link.geometry = structure;
            link.surface_water_level =
                static_cast<double>(state.cells[link_config.cell].eta);
            tri_config.surface_drainage.push_back(link);
        }
        for (std::size_t index = 0U; index < config.surface_river.size(); ++index) {
            const auto& link_config = config.surface_river[index];
            driver_ns::SurfaceRiverLink link{};
            link.cell_index = config.surface_drainage.size() + index;
            link.location_id = link_config.location_id;
            link.priority_weight = link_config.priority_weight;
            core::ExchangeFlowGeometry structure{};
            structure.crest_level = link_config.crest_level;
            structure.exchange_width = link_config.exchange_width;
            link.geometry = structure;
            link.surface_water_level =
                static_cast<double>(state.cells[link_config.cell].eta);
            tri_config.surface_river.push_back(link);

            driver_ns::DFlowFMLateralIdMapping mapping{};
            mapping.location_id = link_config.location_id;
            mapping.native_lateral_id = link_config.native_lateral_id;
            tri_config.river_lateral_ids.push_back(mapping);
        }
        for (std::size_t index = 0U; index < config.drainage_river.size(); ++index) {
            const auto& link_config = config.drainage_river[index];
            driver_ns::DrainageRiverLink link{};
            link.outfall_node_id = outfall_node_ids[index];
            link.river_location_id = link_config.river_location_id;
            link.q_capacity = link_config.q_capacity;
            link.drive_outfall_stage = link_config.drive_outfall_stage;
            tri_config.drainage_river.push_back(link);
        }

        const driver_ns::TriCouplingStepReport report = driver_ns::advance_tri_coupling_step(
            coupling, swmm, dflowfm, tri_config, config.dt_couple, config.h_wet);

        // 4. Unconditional conservative write-back (post-replay storage update).
        const driver_ns::ExchangeWriteBackReport write_back =
            driver_ns::apply_exchange_write_back(
                state, loaded.dpm_fields, geometry, map, cells_before, coupling,
                loaded.bed_elevations);
        summary.total_drained_volume += write_back.drained_volume;
        summary.total_returned_volume += write_back.returned_volume;

        // 5. Commit the orchestration boundary.
        const core::SystemMassAudit audit_after = coupling.compute_system_mass(config.h_wet);
        previous_coupling.emplace(std::move(coupling));
        driver.record_committed_coupling_step();

        EpochRecord record{};
        record.epoch = static_cast<std::uint64_t>(epoch);
        record.logical_time = logical_time;
        record.coupling_surface_mass_before = report.surface_mass_before.surface_mass;
        record.coupling_surface_mass_after = report.surface_mass_after.surface_mass;
        record.coupling_deficit_mass_after = audit_after.deficit_mass;
        record.drained_volume = write_back.drained_volume;
        record.returned_volume = write_back.returned_volume;
        record.max_cell_cfl = epoch_max_cfl;
        record.wet_cell_count = report.surface_mass_after.wet_cell_count;
        summary.epochs.push_back(record);
        summary.final_time = logical_time;
    }

    driver.complete();
    finish("completed", "");
    return result;
}

}  // namespace scau::apps::sim_driver

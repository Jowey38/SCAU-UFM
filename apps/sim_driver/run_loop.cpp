#include "run_loop.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "coupling/core/state.hpp"
#include "coupling/driver/checkpoint_coordinator.hpp"
#include "coupling/driver/checkpoint_payloads.hpp"
#include "coupling/driver/dflowfm_checkpoint.hpp"
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

const char* rollback_action_name(driver_ns::DFlowFMRollbackAction action) {
    switch (action) {
        case driver_ns::DFlowFMRollbackAction::memory_only:
            return "memory_only";
        case driver_ns::DFlowFMRollbackAction::checkpoint_reload:
            return "checkpoint_reload";
        case driver_ns::DFlowFMRollbackAction::abort_no_checkpoint:
            return "abort_no_checkpoint";
        case driver_ns::DFlowFMRollbackAction::abort_replay_unavailable:
            return "abort_replay_unavailable";
    }
    return "unknown";
}

// The rolling in-memory window: exactly the last committed epoch boundary.
struct LastCommit {
    s2d::SurfaceState state{};
    std::optional<core::CouplingSnapshot> coupling{};
    double total_drained_volume{0.0};
    double total_returned_volume{0.0};
    double total_boundary_inflow_volume{0.0};
};

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

    // The initial state is the epoch-zero commit baseline.
    LastCommit last_commit{};
    last_commit.state = state;

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
        summary.final_surface_state_hash = driver_ns::hash_surface_state(state);
        result.final_state = driver.state();
        result.committed_epochs = driver.completed_coupling_steps();
    };

    // Failure BEFORE any engine advanced this epoch: restore exactly the last
    // committed boundary (surface state, coupling ledgers, cumulative totals).
    const auto restore_last_commit = [&]() {
        state = last_commit.state;
        if (last_commit.coupling.has_value()) {
            previous_coupling.emplace(
                core::CouplingState{last_commit.coupling->cells()});
        } else {
            previous_coupling.reset();
        }
        summary.total_drained_volume = last_commit.total_drained_volume;
        summary.total_returned_volume = last_commit.total_returned_volume;
        summary.total_boundary_inflow_volume = last_commit.total_boundary_inflow_volume;
        driver_ns::DFlowFMRollbackRequest request{};
        request.engine_advanced = false;
        request.deterministic_replay_available = true;
        summary.recovery_action = "restored_to_last_commit";
        summary.dflowfm_rollback_decision =
            rollback_action_name(driver_ns::decide_dflowfm_rollback_action(request));
    };

    // Failure AT or AFTER engine advancement: SWMM cannot rewind, so rollback
    // across the engine boundary is refused and the decision is evidence.
    const auto refuse_engine_rollback = [&]() {
        driver_ns::DFlowFMRollbackRequest request{};
        request.engine_advanced = true;
        request.deterministic_replay_available = false;
        summary.recovery_action = "refused_engine_rollback";
        summary.dflowfm_rollback_decision =
            rollback_action_name(driver_ns::decide_dflowfm_rollback_action(request));
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
                restore_last_commit();
                driver.require_review();
                finish("review_required",
                       "cfl_rollback at epoch " + std::to_string(epoch) +
                           " (max_cell_cfl exceeded c_rollback before engine advancement; "
                           "state restored to the last committed boundary)");
                return result;
            }
        }

        // 2. Project the current surface into fresh exchange cells. Failures
        // here happen BEFORE any engine write: restore and review.
        std::vector<core::ExchangeCellState> cells_before;
        std::optional<core::CouplingState> coupling_opt;
        try {
            cells_before = driver_ns::build_exchange_cells(
                state, loaded.dpm_fields, geometry, map,
                previous_coupling.has_value() ? &*previous_coupling : nullptr);
            coupling_opt.emplace(cells_before);
        } catch (const std::exception& error) {
            restore_last_commit();
            driver.require_review();
            finish("review_required",
                   "exchange projection failed at epoch " + std::to_string(epoch) +
                       " before engine advancement: " + error.what());
            return result;
        }
        core::CouplingState& coupling = *coupling_opt;

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

        // 3.-4. One coupled substep (dt_sub = dt_couple) and the unconditional
        // conservative write-back. Any failure in this window may have
        // advanced an engine: rollback is refused and the run stops.
        driver_ns::TriCouplingStepReport report{};
        driver_ns::ExchangeWriteBackReport write_back{};
        try {
            report = driver_ns::advance_tri_coupling_step(
                coupling, swmm, dflowfm, tri_config, config.dt_couple, config.h_wet);
            write_back = driver_ns::apply_exchange_write_back(
                state, loaded.dpm_fields, geometry, map, cells_before, coupling,
                loaded.bed_elevations);
        } catch (const std::exception& error) {
            refuse_engine_rollback();
            driver.require_review();
            finish("review_required",
                   "coupled substep failed at epoch " + std::to_string(epoch) +
                       " at or after engine advancement (rollback refused): " +
                       error.what());
            return result;
        }
        summary.total_drained_volume += write_back.drained_volume;
        summary.total_returned_volume += write_back.returned_volume;

        // 5. Epoch commit protocol: only a committed coordinator verdict
        // advances the committed-step counter and the rolling window.
        const std::uint64_t epoch_id = static_cast<std::uint64_t>(epoch) + 1U;
        const core::CouplingSnapshot coupling_snapshot = coupling.snapshot();
        const double swmm_elapsed =
            hooks.swmm_elapsed_time ? hooks.swmm_elapsed_time() : logical_time;
        const double dflowfm_elapsed =
            hooks.dflowfm_elapsed_time ? hooks.dflowfm_elapsed_time() : logical_time;
        std::vector<driver_ns::PreparedModuleCheckpoint> prepared{
            driver_ns::prepare_surface2d_checkpoint(state, epoch_id, logical_time),
            driver_ns::prepare_coupling_checkpoint(coupling_snapshot, epoch_id,
                                                   logical_time),
            driver_ns::prepare_sim_driver_checkpoint(
                driver.completed_coupling_steps() + 1U, epoch_id, logical_time),
            driver_ns::prepare_swmm_checkpoint(swmm_elapsed, epoch_id, logical_time),
            driver_ns::prepare_dflowfm_checkpoint_record(dflowfm_elapsed, nullptr,
                                                         epoch_id, logical_time),
        };
        driver_ns::CheckpointRequirements requirements{};
        requirements.require_swmm = true;
        requirements.require_dflowfm = true;
        const driver_ns::CheckpointCommitRecord commit_record =
            driver_ns::coordinate_checkpoint_commit(prepared, requirements);
        if (commit_record.status != driver_ns::CheckpointCommitStatus::committed) {
            refuse_engine_rollback();
            driver.require_review();
            finish("review_required",
                   "checkpoint commit aborted at epoch " + std::to_string(epoch) +
                       " after engine advancement (rollback refused): " +
                       commit_record.reason);
            return result;
        }

        const core::SystemMassAudit audit_after = coupling.compute_system_mass(config.h_wet);
        previous_coupling.emplace(std::move(coupling));
        driver.record_committed_coupling_step();

        last_commit.state = state;
        last_commit.coupling.emplace(coupling_snapshot);
        last_commit.total_drained_volume = summary.total_drained_volume;
        last_commit.total_returned_volume = summary.total_returned_volume;
        last_commit.total_boundary_inflow_volume = summary.total_boundary_inflow_volume;

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
        record.checkpoint_status = "committed";
        record.surface_content_hash = prepared[0].content_hash;
        record.coupling_content_hash = prepared[1].content_hash;
        summary.epochs.push_back(record);
        summary.final_time = logical_time;
    }

    driver.complete();
    finish("completed", "");
    return result;
}

}  // namespace scau::apps::sim_driver

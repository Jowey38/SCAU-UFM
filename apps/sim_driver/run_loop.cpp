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
#include "coupling/driver/dflowfm_volume_provider.hpp"
#include "coupling/driver/surface2d_coupling_map.hpp"
#include "coupling/driver/tri_coupling.hpp"
#include "coupling/driver/whole_system_mass_audit.hpp"
#include "surface2d/audit/mass.hpp"
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
                               const s2d::GeometryCache& geometry,
                               double h_wet) {
    return s2d::total_physical_surface_volume(state, dpm, geometry, h_wet);
}

std::vector<driver_ns::DeficitAgeObservation> observe_deficit_ages(
    const core::CouplingState& coupling) {
    std::vector<driver_ns::DeficitAgeObservation> observations;
    std::size_t account_index = 0U;
    for (const core::ExchangeCellState& cell : coupling.cells()) {
        if (cell.shared_deficit_accounts.empty()) {
            observations.push_back({
                .account_index = account_index++,
                .deficit_age_steps = cell.mass_deficit_account.age_steps,
                .volume = cell.mass_deficit_account.volume,
            });
        } else {
            for (const core::SharedExchangeEndpointDeficit& deficit :
                 cell.shared_deficit_accounts) {
                observations.push_back({
                    .account_index = account_index++,
                    .deficit_age_steps = deficit.mass_deficit_account.age_steps,
                    .volume = deficit.mass_deficit_account.volume,
                });
            }
        }
    }
    return observations;
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
    double cumulative_rainfall_volume{0.0};
    double cumulative_infiltration_volume{0.0};
    double cumulative_abstraction_volume{0.0};
    double cumulative_swmm_lateral_volume{0.0};
    double cumulative_depression_delta_volume{0.0};
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
    std::optional<driver_ns::WholeSystemMassSample> mass_baseline{};
    double cumulative_rainfall_volume = 0.0;
    double cumulative_infiltration_volume = 0.0;
    double cumulative_abstraction_volume = 0.0;
    double cumulative_swmm_lateral_volume = 0.0;
    double cumulative_depression_delta_volume = 0.0;
    summary.whole_system_mass_audit_enabled =
        config.enable_whole_system_mass_audit;

    if (config.enable_whole_system_mass_audit) {
        if (!hooks.swmm_storage_volume) {
            throw std::invalid_argument(
                "whole-system mass audit requires a complete SWMM storage provider");
        }
        driver_ns::WholeSystemMassSample initial_sample{};
        initial_sample.epoch = 0U;
        initial_sample.logical_time = config.start_time;
        // Whole-system physical storage counts all non-negative depths;
        // h_wet is a reference/tolerance diagnostic threshold, not permission
        // to discard near-dry physical water from a conservation equation.
        initial_sample.surface_volume = physical_surface_volume(
            state, loaded.dpm_fields, geometry, 0.0);
        initial_sample.surface_reference_volume = physical_surface_volume(
            state, loaded.dpm_fields, geometry, config.h_wet);
        initial_sample.swmm_storage_volume = hooks.swmm_storage_volume();
        if (hooks.dflowfm_storage_volume) {
            initial_sample.dflowfm_volume = hooks.dflowfm_storage_volume();
        } else {
            const driver_ns::DFlowFMVolumeObservation dflow_observation =
                driver_ns::observe_dflowfm_volume(dflowfm);
            if (!dflow_observation.scope_complete) {
                throw std::invalid_argument(
                    "whole-system mass audit requires complete D-Flow FM vol1 scope");
            }
            initial_sample.dflowfm_volume = dflow_observation.volume;
        }
        if (hooks.swmm_external_net_volume) {
            initial_sample.swmm_external_net_volume =
                hooks.swmm_external_net_volume();
        }
        if (hooks.dflowfm_external_net_volume) {
            initial_sample.dflowfm_external_net_volume =
                hooks.dflowfm_external_net_volume();
        }
        mass_baseline = initial_sample;
    }

    // The initial state is the epoch-zero commit baseline.
    LastCommit last_commit{};
    last_commit.state = state;

    const auto finish = [&](const char* outcome, const std::string& reason) {
        summary.outcome = outcome;
        summary.reason = reason;
        summary.committed_epochs = driver.completed_coupling_steps();
        summary.final_surface_physical_volume =
            physical_surface_volume(state, loaded.dpm_fields, geometry, 0.0);
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
            previous_coupling.emplace(core::CouplingState{
                last_commit.coupling->cells(),
                last_commit.coupling->runtime_counters()});
        } else {
            previous_coupling.reset();
        }
        summary.total_drained_volume = last_commit.total_drained_volume;
        summary.total_returned_volume = last_commit.total_returned_volume;
        summary.total_boundary_inflow_volume = last_commit.total_boundary_inflow_volume;
        cumulative_rainfall_volume = last_commit.cumulative_rainfall_volume;
        cumulative_infiltration_volume = last_commit.cumulative_infiltration_volume;
        cumulative_abstraction_volume = last_commit.cumulative_abstraction_volume;
        cumulative_swmm_lateral_volume = last_commit.cumulative_swmm_lateral_volume;
        cumulative_depression_delta_volume =
            last_commit.cumulative_depression_delta_volume;
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
            cumulative_rainfall_volume +=
                static_cast<double>(diagnostics.rainfall_volume);
            cumulative_infiltration_volume +=
                static_cast<double>(diagnostics.infiltration_volume);
            cumulative_abstraction_volume +=
                static_cast<double>(diagnostics.abstraction_volume);
            cumulative_depression_delta_volume +=
                static_cast<double>(diagnostics.depression_storage_delta_volume);
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
            coupling_opt.emplace(
                cells_before,
                previous_coupling.has_value()
                    ? previous_coupling->runtime_counters()
                    : core::RuntimeCounters{});
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
        for (const auto& decision : report.surface_decisions) {
            if (decision.endpoint.engine == core::SharedExchangeEngine::drainage) {
                cumulative_swmm_lateral_volume += decision.exchange.v_granted +
                    decision.exchange.v_repay;
            }
        }

        // M271 epoch-end obligation governance. replay_pending and write-back
        // already completed; age/write-off must become part of the atomic
        // checkpoint snapshot committed below.
        core::DeficitWriteoffConfig writeoff_config{};
        writeoff_config.writeoff_threshold_steps = config.n_writeoff_steps;
        const core::DeficitWriteoffReport writeoff_report =
            coupling.apply_deficit_writeoff(writeoff_config);

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

        // M270 physical whole-system storage audit at the post-replay,
        // post-write-back boundary. Deficit is parallel obligation evidence,
        // not physical storage (v_unmet remains on the surface).
        std::optional<driver_ns::WholeSystemMassAuditReport> whole_system_audit{};
        if (config.enable_whole_system_mass_audit) {
            driver_ns::WholeSystemMassSample current_sample{};
            current_sample.epoch = epoch_id;
            current_sample.logical_time = logical_time;
            current_sample.surface_volume = physical_surface_volume(
                state, loaded.dpm_fields, geometry, 0.0);
            current_sample.surface_reference_volume = physical_surface_volume(
                state, loaded.dpm_fields, geometry, config.h_wet);
            current_sample.coupling_deficit_volume = audit_after.deficit_mass;
            current_sample.swmm_storage_volume = hooks.swmm_storage_volume();
            if (hooks.dflowfm_storage_volume) {
                current_sample.dflowfm_volume = hooks.dflowfm_storage_volume();
            } else {
                const driver_ns::DFlowFMVolumeObservation dflow_observation =
                    driver_ns::observe_dflowfm_volume(dflowfm);
                if (!dflow_observation.scope_complete) {
                    refuse_engine_rollback();
                    driver.require_review();
                    finish("review_required",
                           "whole-system mass audit D-Flow FM volume scope incomplete "
                           "after engine advancement (rollback refused)");
                    return result;
                }
                current_sample.dflowfm_volume = dflow_observation.volume;
            }
            if (hooks.swmm_external_net_volume) {
                current_sample.swmm_external_net_volume =
                    hooks.swmm_external_net_volume();
            }
            if (hooks.dflowfm_external_net_volume) {
                current_sample.dflowfm_external_net_volume =
                    hooks.dflowfm_external_net_volume();
            }
            current_sample.cumulative_boundary_inflow_volume =
                summary.total_boundary_inflow_volume;
            current_sample.cumulative_rainfall_volume = cumulative_rainfall_volume;
            current_sample.cumulative_infiltration_volume =
                cumulative_infiltration_volume;
            current_sample.cumulative_abstraction_volume =
                cumulative_abstraction_volume;
            current_sample.cumulative_depression_storage_delta_volume =
                cumulative_depression_delta_volume;

            driver_ns::WholeSystemMassTolerance mass_tolerance{};
            mass_tolerance.strict = config.engine_mode == EngineMode::mock;
            mass_tolerance.engine_residual_absolute =
                config.mass_audit_engine_residual_absolute;
            mass_tolerance.engine_residual_relative =
                config.mass_audit_engine_residual_relative;
            whole_system_audit = driver_ns::audit_whole_system_mass(
                *mass_baseline, current_sample, mass_tolerance);
            summary.final_whole_system_mass_residual =
                whole_system_audit->residual;
            summary.max_abs_whole_system_mass_residual = std::max(
                summary.max_abs_whole_system_mass_residual,
                std::abs(whole_system_audit->residual));
            summary.whole_system_mass_tolerance =
                whole_system_audit->applied_tolerance;
            summary.whole_system_mass_verdict = whole_system_audit->conserved
                                                    ? "conserved"
                                                    : "review_required";
            if (!whole_system_audit->conserved) {
                refuse_engine_rollback();
                driver.require_review();
                const std::string failure_kind = whole_system_audit->scope_complete
                                                     ? "drift"
                                                     : "external flux scope incomplete";
                finish("review_required",
                       "whole-system mass audit " + failure_kind +
                           " after engine advancement (rollback refused): residual=" +
                           std::to_string(whole_system_audit->residual) +
                           ", tolerance=" +
                           std::to_string(whole_system_audit->applied_tolerance));
                return result;
            }
        }

        const std::vector<driver_ns::DeficitAgeObservation> deficit_ages =
            observe_deficit_ages(coupling);
        summary.count_writeoff_events += writeoff_report.event_count;
        summary.count_writeoff_volume_total += writeoff_report.volume_written_off_total;
        for (const core::DeficitWriteoffRecord& writeoff : writeoff_report.records) {
            if (writeoff.has_shared_endpoint) {
                summary.writeoff_endpoint_ids.push_back(
                    (writeoff.endpoint.engine == core::SharedExchangeEngine::drainage
                         ? "drainage:"
                         : "river:") +
                    std::to_string(writeoff.endpoint.node_id));
            } else {
                summary.writeoff_endpoint_ids.push_back(
                    "aggregate:cell=" + std::to_string(writeoff.cell_index));
            }
        }

        previous_coupling.emplace(std::move(coupling));
        driver.record_committed_coupling_step();

        last_commit.state = state;
        last_commit.coupling.emplace(coupling_snapshot);
        last_commit.total_drained_volume = summary.total_drained_volume;
        last_commit.total_returned_volume = summary.total_returned_volume;
        last_commit.total_boundary_inflow_volume = summary.total_boundary_inflow_volume;
        last_commit.cumulative_rainfall_volume = cumulative_rainfall_volume;
        last_commit.cumulative_infiltration_volume = cumulative_infiltration_volume;
        last_commit.cumulative_abstraction_volume = cumulative_abstraction_volume;
        last_commit.cumulative_swmm_lateral_volume = cumulative_swmm_lateral_volume;
        last_commit.cumulative_depression_delta_volume =
            cumulative_depression_delta_volume;

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
        record.whole_system_mass_audit_enabled =
            config.enable_whole_system_mass_audit;
        if (whole_system_audit.has_value()) {
            record.whole_system_storage_total =
                whole_system_audit->current_storage_total;
            record.whole_system_mass_residual = whole_system_audit->residual;
            record.whole_system_mass_tolerance =
                whole_system_audit->applied_tolerance;
            record.whole_system_mass_verdict = "conserved";
        }
        for (const driver_ns::DeficitAgeObservation& age : deficit_ages) {
            record.deficit_age_steps.push_back(age.deficit_age_steps);
            record.deficit_account_volumes.push_back(age.volume);
        }
        record.writeoff_event_count = writeoff_report.event_count;
        record.writeoff_volume_total = writeoff_report.volume_written_off_total;
        for (const core::DeficitWriteoffRecord& writeoff : writeoff_report.records) {
            record.writeoff_endpoint_ids.push_back(
                writeoff.has_shared_endpoint
                    ? ((writeoff.endpoint.engine == core::SharedExchangeEngine::drainage
                            ? "drainage:"
                            : "river:") +
                       std::to_string(writeoff.endpoint.node_id))
                    : ("aggregate:cell=" + std::to_string(writeoff.cell_index)));
        }
        summary.epochs.push_back(record);
        summary.final_time = logical_time;
    }

    driver.complete();
    finish("completed", "");
    return result;
}

}  // namespace scau::apps::sim_driver

#include "surface2d/source_terms/runoff/roof_step.hpp"

#include <stdexcept>

#include "surface2d/source_terms/runoff/runoff_generation.hpp"
#include "surface2d/time_integration/step.hpp"

namespace scau::surface2d {

RoofStepInputs RoofStepInputs::for_mesh(const mesh::Mesh& mesh) {
    RoofStepInputs inputs;
    inputs.map = RoofDrainageMap::for_mesh(mesh);
    // Accept-nothing default: every request is fully rejected (CapacityLimited).
    inputs.accept = [](const RoofDrainageIntent& intent) {
        return RoofDrainageAcceptance{
            .requested_volume = intent.requested_volume,
            .accepted_volume = 0.0,
            .rejected_volume = intent.requested_volume,
            .rejection_reason = RoofDrainageRejectionReason::CapacityLimited,
        };
    };
    return inputs;
}

void validate_roof_step_inputs_match_mesh(const RoofStepInputs& inputs, const mesh::Mesh& mesh) {
    validate_roof_drainage_map_match_mesh(inputs.map, mesh);
    if (!inputs.accept) {
        throw std::invalid_argument("roof step acceptance function must be set");
    }
}

void apply_roof_runoff_stage(
    SurfaceState& state,
    const StepConfig& config,
    const DpmFields& dpm_fields,
    const RunoffStepInputs& runoff,
    const RoofStepInputs& roof,
    RunoffState& runoff_state,
    const GeometryCache& geometry,
    StepDiagnostics& diagnostics) {
    for (std::size_t i = 0; i < state.cells.size(); ++i) {
        const core::Real area = geometry.cell_areas[i];
        if (area <= 0.0 || runoff.fields.roof_fraction[i] <= 0.0) {
            continue;
        }
        const core::Real phi_t = dpm_fields.cells[i].phi_t;

        RunoffCellInputs cell_inputs{
            .rainfall_rate = runoff.rainfall_rate[i],
            .phi_t = phi_t,
            .cell_area = area,
        };
        RunoffCellParams params{
            .pervious_fraction = runoff.fields.pervious_fraction[i],
            .impervious_fraction = runoff.fields.impervious_fraction[i],
            .roof_fraction = runoff.fields.roof_fraction[i],
            .initial_abstraction_capacity = runoff.fields.initial_abstraction_capacity[i],
            .depression_storage_capacity = runoff.fields.depression_storage_capacity[i],
            .roof_abstraction_capacity = runoff.fields.roof_abstraction_capacity[i],
            .roof_storage_capacity = runoff.fields.roof_storage_capacity[i],
            .roof_drain_capacity = roof.map.roof_drain_capacity[i],
            .soil = runoff.lut.at(runoff.fields.soil_type[i]),
        };
        RunoffCellState cell_state{
            .cumulative_infiltration = runoff_state.cumulative_infiltration[i],
            .ponding_time = runoff_state.ponding_time[i],
            .abstraction_filled = runoff_state.abstraction_filled[i],
            .depression_storage_filled = runoff_state.depression_storage_filled[i],
            .roof_abstraction_filled = runoff_state.roof_abstraction_filled[i],
            .roof_pending_volume = runoff_state.roof_pending_volume[i],
        };

        const RoofEmitResult emit = evaluate_roof_emit(cell_inputs, params, cell_state, config.dt);
        const RoofDrainageIntent intent{
            .source_cell_index = static_cast<int>(i),
            .target_swmm_node_index = roof.map.swmm_node_index[i],
            .requested_volume = emit.requested_volume,
            .source_roof_area = params.roof_fraction * area,
        };
        const RoofDrainageAcceptance acc = roof.accept(intent);
        const int overflow_cell = roof.map.roof_overflow_to_surface_cell_index[i];
        const bool overflow_valid = overflow_cell >= 0;
        const RoofAcceptanceResult ar =
            apply_roof_drainage_acceptance(cell_inputs, params, cell_state, acc, overflow_valid);

        if (ar.overflow_to_surface_volume > 0.0 && overflow_valid) {
            const std::size_t oc = static_cast<std::size_t>(overflow_cell);
            const core::Real oc_phi_t = dpm_fields.cells[oc].phi_t;
            const core::Real oc_area = geometry.cell_areas[oc];
            state.cells[oc].conserved.h += ar.overflow_to_surface_volume / (oc_phi_t * oc_area);
        }

        runoff_state.roof_abstraction_filled[i] = cell_state.roof_abstraction_filled;
        runoff_state.roof_pending_volume[i] = cell_state.roof_pending_volume;

        diagnostics.rainfall_volume += cell_inputs.rainfall_rate * config.dt * (params.roof_fraction * area);
        diagnostics.abstraction_volume += emit.roof_abstraction_volume;
        diagnostics.roof_to_swmm_requested_volume += emit.requested_volume;
        diagnostics.roof_to_swmm_accepted_volume += ar.accepted_volume;
        diagnostics.roof_to_swmm_rejected_volume += ar.rejected_volume;
        diagnostics.roof_pending_delta_volume += emit.roof_input_volume + ar.pending_delta_volume;
        diagnostics.roof_overflow_to_surface_volume += ar.overflow_to_surface_volume;
        if (ar.missing_overflow_target) {
            diagnostics.missing_roof_overflow_target = true;
        }
    }
}

}  // namespace scau::surface2d

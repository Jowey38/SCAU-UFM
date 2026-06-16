#include "surface2d/source_terms/runoff/runoff_generation.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace scau::surface2d {

namespace {

void validate_common_inputs(
    const RunoffCellInputs& inputs,
    const RunoffCellParams& params,
    core::Real dt) {
    if (!std::isfinite(inputs.rainfall_rate) || inputs.rainfall_rate < 0.0) {
        throw std::invalid_argument("runoff rainfall_rate must be finite and non-negative");
    }
    if (!std::isfinite(inputs.phi_t) || inputs.phi_t <= 0.0) {
        throw std::invalid_argument("runoff phi_t must be finite and positive");
    }
    if (!std::isfinite(inputs.cell_area) || inputs.cell_area <= 0.0) {
        throw std::invalid_argument("runoff cell_area must be finite and positive");
    }
    if (!std::isfinite(dt) || dt <= 0.0) {
        throw std::invalid_argument("runoff dt must be finite and positive");
    }
    const core::Real fp = params.pervious_fraction;
    const core::Real fi = params.impervious_fraction;
    const core::Real fr = params.roof_fraction;
    if (!std::isfinite(fp) || !std::isfinite(fi) || !std::isfinite(fr) ||
        fp < 0.0 || fi < 0.0 || fr < 0.0 || fp > 1.0 || fi > 1.0 || fr > 1.0) {
        throw std::invalid_argument("runoff area fractions must be finite and in [0,1]");
    }
    if (fp + fi + fr > 1.0 + 1.0e-6) {
        throw std::invalid_argument("runoff area fractions must sum to <= 1");
    }
}

}  // namespace

GroundRunoffResult evaluate_ground_runoff(
    const RunoffCellInputs& inputs,
    const RunoffCellParams& params,
    RunoffCellState& state,
    core::Real dt,
    core::Real f_inf_floor) {
    validate_common_inputs(inputs, params, dt);
    if (!std::isfinite(params.initial_abstraction_capacity) || params.initial_abstraction_capacity < 0.0) {
        throw std::invalid_argument("initial_abstraction_capacity must be finite and non-negative");
    }
    if (!std::isfinite(params.depression_storage_capacity) || params.depression_storage_capacity < 0.0) {
        throw std::invalid_argument("depression_storage_capacity must be finite and non-negative");
    }

    const core::Real area_pervious = params.pervious_fraction * inputs.cell_area;
    const core::Real area_impervious = params.impervious_fraction * inputs.cell_area;
    const core::Real area_ground = area_pervious + area_impervious;
    const core::Real rain_depth = inputs.rainfall_rate * dt;

    const core::Real abs_remaining =
        std::max(core::Real(0.0), params.initial_abstraction_capacity - state.abstraction_filled);
    const core::Real abs_fill_depth = std::min(abs_remaining, rain_depth);
    state.abstraction_filled += abs_fill_depth;
    const core::Real after_abstraction = rain_depth - abs_fill_depth;

    const core::Real dep_remaining =
        std::max(core::Real(0.0), params.depression_storage_capacity - state.depression_storage_filled);
    const core::Real dep_fill_depth = std::min(dep_remaining, after_abstraction);
    state.depression_storage_filled += dep_fill_depth;
    const core::Real available_depth = after_abstraction - dep_fill_depth;

    GroundRunoffResult result;
    result.abstraction_volume = abs_fill_depth * area_ground;
    result.depression_storage_delta_volume = dep_fill_depth * area_ground;

    core::Real infiltrated_depth = 0.0;
    if (area_pervious > 0.0) {
        const auto ga = green_ampt_infiltration_step(
            params.soil, state.cumulative_infiltration, available_depth, dt, f_inf_floor);
        infiltrated_depth = ga.infiltrated_depth;
        state.cumulative_infiltration = ga.cumulative_infiltration;
        result.ponding_started = ga.ponding_started;
    }

    const core::Real runoff_pervious_depth = available_depth - infiltrated_depth;
    result.infiltration_volume = infiltrated_depth * area_pervious;
    result.surface_added_volume =
        runoff_pervious_depth * area_pervious + available_depth * area_impervious;
    return result;
}

RoofEmitResult evaluate_roof_emit(
    const RunoffCellInputs& inputs,
    const RunoffCellParams& params,
    RunoffCellState& state,
    core::Real dt) {
    validate_common_inputs(inputs, params, dt);
    if (!std::isfinite(params.roof_abstraction_capacity) || params.roof_abstraction_capacity < 0.0) {
        throw std::invalid_argument("roof_abstraction_capacity must be finite and non-negative");
    }
    if (!std::isfinite(params.roof_drain_capacity) || params.roof_drain_capacity < 0.0) {
        throw std::invalid_argument("roof_drain_capacity must be finite and non-negative");
    }

    const core::Real area_roof = params.roof_fraction * inputs.cell_area;
    const core::Real rain_depth = inputs.rainfall_rate * dt;

    const core::Real abs_remaining =
        std::max(core::Real(0.0), params.roof_abstraction_capacity - state.roof_abstraction_filled);
    const core::Real abs_fill_depth = std::min(abs_remaining, rain_depth);
    state.roof_abstraction_filled += abs_fill_depth;

    RoofEmitResult result;
    result.roof_abstraction_volume = abs_fill_depth * area_roof;
    result.roof_input_volume = (rain_depth - abs_fill_depth) * area_roof;
    state.roof_pending_volume += result.roof_input_volume;

    const core::Real drain_capacity_volume = params.roof_drain_capacity * dt;
    result.requested_volume = std::min(state.roof_pending_volume, drain_capacity_volume);
    result.drain_capacity_limited = state.roof_pending_volume > drain_capacity_volume;
    return result;
}

RoofAcceptanceResult apply_roof_drainage_acceptance(
    const RunoffCellInputs& inputs,
    const RunoffCellParams& params,
    RunoffCellState& state,
    const RoofDrainageAcceptance& acceptance,
    bool overflow_target_valid) {
    validate_common_inputs(inputs, params, 1.0);  // dt unused here; 1.0 keeps the validator happy
    if (!std::isfinite(params.roof_storage_capacity) || params.roof_storage_capacity < 0.0) {
        throw std::invalid_argument("roof_storage_capacity must be finite and non-negative");
    }
    if (!std::isfinite(acceptance.accepted_volume) || acceptance.accepted_volume < 0.0 ||
        !std::isfinite(acceptance.requested_volume) || acceptance.requested_volume < 0.0) {
        throw std::invalid_argument("roof acceptance volumes must be finite and non-negative");
    }

    const core::Real pending_before = state.roof_pending_volume;

    RoofAcceptanceResult result;
    result.accepted_volume = std::min(acceptance.accepted_volume, state.roof_pending_volume);
    state.roof_pending_volume -= result.accepted_volume;
    result.rejected_volume = std::max(core::Real(0.0), acceptance.requested_volume - result.accepted_volume);
    result.node_rejected =
        acceptance.rejection_reason != RoofDrainageRejectionReason::None &&
        acceptance.accepted_volume < acceptance.requested_volume;

    const core::Real area_roof = params.roof_fraction * inputs.cell_area;
    const core::Real max_pending = params.roof_storage_capacity * area_roof;
    if (state.roof_pending_volume > max_pending) {
        result.pending_saturated = true;
        const core::Real excess = state.roof_pending_volume - max_pending;
        if (overflow_target_valid) {
            state.roof_pending_volume = max_pending;
            result.overflow_to_surface_volume = excess;
            result.overflowed = true;
        } else {
            result.missing_overflow_target = true;  // retain water; signal config error
        }
    }

    result.pending_delta_volume = state.roof_pending_volume - pending_before;
    return result;
}

RunoffGenerationOutput evaluate_runoff_generation(
    const RunoffCellInputs& inputs,
    const RunoffCellParams& params,
    RunoffCellState& state,
    core::Real dt,
    core::Real f_inf_floor,
    int cell_index,
    int target_swmm_node_index) {
    const GroundRunoffResult ground = evaluate_ground_runoff(inputs, params, state, dt, f_inf_floor);
    const RoofEmitResult roof = evaluate_roof_emit(inputs, params, state, dt);

    const core::Real area_roof = params.roof_fraction * inputs.cell_area;
    const core::Real area_ground =
        (params.pervious_fraction + params.impervious_fraction) * inputs.cell_area;
    const core::Real rain_depth = inputs.rainfall_rate * dt;

    RunoffGenerationOutput out;
    out.result.rainfall_volume = rain_depth * (area_ground + area_roof);
    out.result.surface_added_volume = ground.surface_added_volume;
    out.result.infiltration_volume = ground.infiltration_volume;
    out.result.abstraction_volume = ground.abstraction_volume + roof.roof_abstraction_volume;
    out.result.depression_storage_delta_volume = ground.depression_storage_delta_volume;
    out.result.roof_to_swmm_requested_volume = roof.requested_volume;
    out.result.roof_to_swmm_accepted_volume = 0.0;
    out.result.roof_to_swmm_rejected_volume = 0.0;
    out.result.roof_pending_delta_volume = roof.roof_input_volume;  // nothing drained yet
    out.result.roof_overflow_to_surface_volume = 0.0;
    out.result.rejected_fail_closed_volume = 0.0;
    out.result.flags.green_ampt_ponding_started = ground.ponding_started;
    out.result.flags.roof_drain_capacity_limited = roof.drain_capacity_limited;
    if (inputs.phi_t < 0.2 && out.result.surface_added_volume > 0.0) {
        out.result.flags.surface_storage_amplified_by_low_phi_t = true;
    }

    out.intent.source_cell_index = cell_index;
    out.intent.target_swmm_node_index = target_swmm_node_index;
    out.intent.requested_volume = roof.requested_volume;
    out.intent.source_roof_area = area_roof;
    return out;
}

}  // namespace scau::surface2d

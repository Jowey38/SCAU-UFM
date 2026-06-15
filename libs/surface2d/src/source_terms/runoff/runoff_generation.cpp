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

}  // namespace scau::surface2d

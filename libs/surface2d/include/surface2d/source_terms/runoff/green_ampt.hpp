#pragma once

#include "core/types.hpp"
#include "surface2d/portability.hpp"
#include "surface2d/source_terms/runoff/soil_params.hpp"

namespace scau::surface2d {

// Result of one Green-Ampt substep. infiltrated_depth is the depth (m) that
// left the surface into the soil this substep; cumulative_infiltration is the
// updated F_inf; ponding_started is true when supply exceeded capacity.
struct GreenAmptStep {
    core::Real infiltrated_depth{0.0};
    core::Real cumulative_infiltration{0.0};
    bool ponding_started{false};
};

// Forward-Euler Mein-Larson substep. available_depth is the water depth (m)
// offered to the soil this substep (rain excess plus any ponded water share).
// f_inf_floor (> 0) floors F for the 1/F capacity term. Fail-closed on
// non-finite or out-of-domain inputs; never returns negative infiltration and
// never infiltrates more than available_depth.
// Checked host entry point.
[[nodiscard]] GreenAmptStep green_ampt_infiltration_step(
    const SoilParams& params,
    core::Real cumulative_infiltration_before,
    core::Real available_depth,
    core::Real dt,
    core::Real f_inf_floor);

// Unchecked SCAU_HD core shared with the deterministic CUDA backend
// (M284/G9). Identical arithmetic to the checked wrapper; the CUDA path
// validates soil params and state on the host before launching.
[[nodiscard]] SCAU_HD inline GreenAmptStep green_ampt_infiltration_step_unchecked(
    const SoilParams& params,
    core::Real cumulative_infiltration_before,
    core::Real available_depth,
    core::Real dt,
    core::Real f_inf_floor) {
    const core::Real delta_theta = params.theta_s - params.theta_i;
    const core::Real f_eval = hd::max_(cumulative_infiltration_before, f_inf_floor);
    const core::Real capacity_rate = params.k_s * (1.0 + params.psi_f * delta_theta / f_eval);
    const core::Real potential_depth = capacity_rate * dt;

    GreenAmptStep step;
    step.infiltrated_depth = hd::min_(available_depth, potential_depth);
    if (step.infiltrated_depth < 0.0) {
        step.infiltrated_depth = 0.0;
    }
    step.cumulative_infiltration = cumulative_infiltration_before + step.infiltrated_depth;
    step.ponding_started = available_depth > potential_depth;
    return step;
}

}  // namespace scau::surface2d

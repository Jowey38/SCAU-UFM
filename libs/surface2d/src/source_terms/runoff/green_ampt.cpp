#include "surface2d/source_terms/runoff/green_ampt.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace scau::surface2d {

GreenAmptStep green_ampt_infiltration_step(
    const SoilParams& params,
    core::Real cumulative_infiltration_before,
    core::Real available_depth,
    core::Real dt,
    core::Real f_inf_floor) {
    validate_soil_params(params);
    if (!std::isfinite(cumulative_infiltration_before) || cumulative_infiltration_before < 0.0) {
        throw std::invalid_argument("green-ampt cumulative infiltration must be finite and non-negative");
    }
    if (!std::isfinite(available_depth) || available_depth < 0.0) {
        throw std::invalid_argument("green-ampt available depth must be finite and non-negative");
    }
    if (!std::isfinite(dt) || dt <= 0.0) {
        throw std::invalid_argument("green-ampt dt must be finite and positive");
    }
    if (!std::isfinite(f_inf_floor) || f_inf_floor <= 0.0) {
        throw std::invalid_argument("green-ampt f_inf_floor must be finite and positive");
    }

    const core::Real delta_theta = params.theta_s - params.theta_i;
    const core::Real f_eval = std::max(cumulative_infiltration_before, f_inf_floor);
    const core::Real capacity_rate = params.k_s * (1.0 + params.psi_f * delta_theta / f_eval);
    const core::Real potential_depth = capacity_rate * dt;

    GreenAmptStep step;
    step.infiltrated_depth = std::min(available_depth, potential_depth);
    if (step.infiltrated_depth < 0.0) {
        step.infiltrated_depth = 0.0;
    }
    step.cumulative_infiltration = cumulative_infiltration_before + step.infiltrated_depth;
    step.ponding_started = available_depth > potential_depth;
    return step;
}

}  // namespace scau::surface2d

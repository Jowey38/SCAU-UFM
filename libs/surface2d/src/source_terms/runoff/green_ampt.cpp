#include "surface2d/source_terms/runoff/green_ampt.hpp"

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
    return green_ampt_infiltration_step_unchecked(
        params, cumulative_infiltration_before, available_depth, dt, f_inf_floor);
}

}  // namespace scau::surface2d

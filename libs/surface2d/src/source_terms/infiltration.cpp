#include "surface2d/source_terms/infiltration.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace scau::surface2d {

core::Real infiltration_depth_decrement(
    core::Real capacity_rate,
    core::Real h,
    core::Real phi_t,
    core::Real dt) {
    if (!std::isfinite(capacity_rate) || capacity_rate < 0.0) {
        throw std::invalid_argument("infiltration capacity rate must be finite and non-negative");
    }
    if (!std::isfinite(h) || h < 0.0) {
        throw std::invalid_argument("infiltration depth must be finite and non-negative");
    }
    if (!std::isfinite(phi_t) || phi_t <= 0.0 || phi_t > 1.0) {
        throw std::invalid_argument("infiltration phi_t must be finite and in (0, 1]");
    }
    if (!std::isfinite(dt) || dt <= 0.0) {
        throw std::invalid_argument("infiltration dt must be finite and positive");
    }
    return std::min(capacity_rate * dt / phi_t, h);
}

}  // namespace scau::surface2d

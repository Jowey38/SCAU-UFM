#include "surface2d/source_terms/rainfall.hpp"

#include <cmath>
#include <stdexcept>

namespace scau::surface2d {

core::Real rainfall_depth_increment(core::Real rate, core::Real phi_t, core::Real dt) {
    if (!std::isfinite(rate) || rate < 0.0) {
        throw std::invalid_argument("rainfall rate must be finite and non-negative");
    }
    if (!std::isfinite(phi_t) || phi_t <= 0.0 || phi_t > 1.0) {
        throw std::invalid_argument("rainfall phi_t must be finite and in (0, 1]");
    }
    if (!std::isfinite(dt) || dt <= 0.0) {
        throw std::invalid_argument("rainfall dt must be finite and positive");
    }
    return rate * dt / phi_t;
}

}  // namespace scau::surface2d

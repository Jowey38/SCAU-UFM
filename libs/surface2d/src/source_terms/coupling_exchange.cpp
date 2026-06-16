#include "surface2d/source_terms/coupling_exchange.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace scau::surface2d {

ExchangeDepthResult exchange_depth_increment(
    core::Real volume,
    core::Real h,
    core::Real phi_t,
    core::Real area) {
    if (!std::isfinite(volume)) {
        throw std::invalid_argument("exchange volume must be finite");
    }
    if (!std::isfinite(h) || h < 0.0) {
        throw std::invalid_argument("exchange depth must be finite and non-negative");
    }
    if (!std::isfinite(phi_t) || phi_t <= 0.0 || phi_t > 1.0) {
        throw std::invalid_argument("exchange phi_t must be finite and in (0, 1]");
    }
    if (!std::isfinite(area) || area <= 0.0) {
        throw std::invalid_argument("exchange cell area must be finite and positive");
    }

    const core::Real raw_increment = volume / (phi_t * area);
    const core::Real depth_increment = std::max(raw_increment, -h);
    return ExchangeDepthResult{
        .depth_increment = depth_increment,
        .applied_volume = depth_increment * phi_t * area,
    };
}

}  // namespace scau::surface2d

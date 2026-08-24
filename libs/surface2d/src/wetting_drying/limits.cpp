#include "surface2d/wetting_drying/limits.hpp"

#include <cmath>
#include <stdexcept>

namespace scau::surface2d {

core::Real nonnegative_depth_after_increment(core::Real h, core::Real depth_increment) {
    if (!std::isfinite(h) || h < 0.0) {
        throw std::invalid_argument("wetting/drying depth must be finite and non-negative");
    }
    if (!std::isfinite(depth_increment)) {
        throw std::invalid_argument("wetting/drying depth increment must be finite");
    }
    return nonnegative_depth_after_increment_unchecked(h, depth_increment);
}

ConservedState apply_dry_cell_momentum_limit(const ConservedState& conserved, core::Real h_min) {
    if (!std::isfinite(h_min) || h_min < 0.0) {
        throw std::invalid_argument("wetting/drying h_min must be finite and non-negative");
    }
    if (!std::isfinite(conserved.h) || conserved.h < 0.0) {
        throw std::invalid_argument("wetting/drying depth must be finite and non-negative");
    }
    if (!std::isfinite(conserved.hu) || !std::isfinite(conserved.hv)) {
        throw std::invalid_argument("wetting/drying momentum must be finite");
    }
    return apply_dry_cell_momentum_limit_unchecked(conserved, h_min);
}

}  // namespace scau::surface2d

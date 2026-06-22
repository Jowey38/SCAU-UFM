#pragma once

#include "core/types.hpp"
#include "surface2d/state/state.hpp"

namespace scau::surface2d {

[[nodiscard]] core::Real nonnegative_depth_after_increment(
    core::Real h,
    core::Real depth_increment);

[[nodiscard]] ConservedState apply_dry_cell_momentum_limit(
    const ConservedState& conserved,
    core::Real h_min);

}  // namespace scau::surface2d

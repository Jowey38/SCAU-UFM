#pragma once

#include "core/types.hpp"
#include "surface2d/state/state.hpp"

namespace scau::surface2d {

// Semi-implicit Manning friction update on a single cell.
// denom = 1 + dt * g * n^2 * |u| / h^(4/3); momentum is divided by denom,
// which is unconditionally stable and preserves the velocity direction.
// Cells at or below h_min are dried: momentum is zeroed.
[[nodiscard]] ConservedState apply_manning_friction(
    const ConservedState& conserved,
    core::Real manning_n,
    core::Real dt,
    core::Real h_min,
    core::Real gravity = 9.81);

}  // namespace scau::surface2d

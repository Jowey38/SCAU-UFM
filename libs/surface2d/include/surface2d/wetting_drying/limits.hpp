#pragma once

#include "core/types.hpp"
#include "surface2d/portability.hpp"
#include "surface2d/state/state.hpp"

namespace scau::surface2d {

// Checked host entry points (throw on non-finite/out-of-domain inputs).
[[nodiscard]] core::Real nonnegative_depth_after_increment(
    core::Real h,
    core::Real depth_increment);

[[nodiscard]] ConservedState apply_dry_cell_momentum_limit(
    const ConservedState& conserved,
    core::Real h_min);

// Unchecked SCAU_HD cores shared with the deterministic CUDA backend
// (M284/G9). Identical arithmetic to the checked wrappers; the CUDA path
// re-creates the checked failure surface with a device error flag.
[[nodiscard]] SCAU_HD inline core::Real nonnegative_depth_after_increment_unchecked(
    core::Real h,
    core::Real depth_increment) {
    return hd::max_(0.0, h + depth_increment);
}

[[nodiscard]] SCAU_HD inline ConservedState apply_dry_cell_momentum_limit_unchecked(
    const ConservedState& conserved,
    core::Real h_min) {
    ConservedState updated = conserved;
    if (updated.h <= h_min) {
        updated.hu = 0.0;
        updated.hv = 0.0;
    }
    return updated;
}

}  // namespace scau::surface2d

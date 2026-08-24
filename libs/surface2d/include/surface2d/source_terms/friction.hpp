#pragma once

#include "core/types.hpp"
#include "surface2d/portability.hpp"
#include "surface2d/state/state.hpp"
#include "surface2d/wetting_drying/limits.hpp"

namespace scau::surface2d {

// Semi-implicit Manning friction update on a single cell.
// denom = 1 + dt * g * n^2 * |u| / h^(4/3); momentum is divided by denom,
// which is unconditionally stable and preserves the velocity direction.
// Cells at or below h_min are dried: momentum is zeroed.
// Checked host entry point (throws on non-finite/out-of-domain inputs).
[[nodiscard]] ConservedState apply_manning_friction(
    const ConservedState& conserved,
    core::Real manning_n,
    core::Real dt,
    core::Real h_min,
    core::Real gravity = 9.81);

// Unchecked SCAU_HD core shared with the deterministic CUDA backend
// (M284/G9). Identical arithmetic to the checked wrapper. NOTE: device pow
// may differ from the host libm by ulps; G9 locks momentum agreement at
// 1e-12 on friction-active fixtures (h is never touched by friction).
[[nodiscard]] SCAU_HD inline ConservedState apply_manning_friction_unchecked(
    const ConservedState& conserved,
    core::Real manning_n,
    core::Real dt,
    core::Real h_min,
    core::Real gravity) {
    ConservedState updated = conserved;
    if (conserved.h <= h_min) {
        updated.hu = 0.0;
        updated.hv = 0.0;
        return updated;
    }
    if (manning_n == 0.0) {
        return updated;
    }

    const core::Real u = conserved.hu / conserved.h;
    const core::Real v = conserved.hv / conserved.h;
    const core::Real speed = hd::sqrt_(u * u + v * v);
    if (speed == 0.0) {
        return updated;
    }

    const core::Real h_pow = hd::pow_(conserved.h, 4.0 / 3.0);
    const core::Real denom = 1.0 + dt * gravity * manning_n * manning_n * speed / h_pow;
    updated.hu /= denom;
    updated.hv /= denom;
    return updated;
}

}  // namespace scau::surface2d

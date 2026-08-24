#pragma once

#include "core/types.hpp"
#include "surface2d/portability.hpp"

namespace scau::surface2d {

// Engine-neutral surface-side application of a coupling-decided exchange
// volume (positive = into the surface, negative = out of the surface) under
// the DPM storage model V = phi_t * h * A. The surface layer never decides
// exchange amounts; arbitration and limits (Q_limit) live in CouplingLib.
// The returned increment is clamped so the cell depth cannot drop below
// zero; the caller reads the applied volume back from the clamped value.
struct ExchangeDepthResult {
    core::Real depth_increment{0.0};
    core::Real applied_volume{0.0};
};

// Checked host entry point (throws on non-finite/out-of-domain inputs).
[[nodiscard]] ExchangeDepthResult exchange_depth_increment(
    core::Real volume,
    core::Real h,
    core::Real phi_t,
    core::Real area);

// Unchecked SCAU_HD core shared with the deterministic CUDA backend
// (M284/G9). Identical arithmetic to the checked wrapper.
[[nodiscard]] SCAU_HD inline ExchangeDepthResult exchange_depth_increment_unchecked(
    core::Real volume,
    core::Real h,
    core::Real phi_t,
    core::Real area) {
    const core::Real raw_increment = volume / (phi_t * area);
    const core::Real depth_increment = hd::max_(raw_increment, -h);
    return ExchangeDepthResult{
        .depth_increment = depth_increment,
        .applied_volume = depth_increment * phi_t * area,
    };
}

}  // namespace scau::surface2d

#pragma once

#include "core/types.hpp"

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

[[nodiscard]] ExchangeDepthResult exchange_depth_increment(
    core::Real volume,
    core::Real h,
    core::Real phi_t,
    core::Real area);

}  // namespace scau::surface2d

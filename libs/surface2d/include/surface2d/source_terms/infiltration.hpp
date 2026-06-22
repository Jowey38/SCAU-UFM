#pragma once

#include "core/types.hpp"

namespace scau::surface2d {

// Depth decrement from infiltration over one substep under the DPM storage
// model V = phi_t * h * A. The soil takes up to capacity_rate * dt / phi_t
// of stored depth, clamped by the available depth h (never drives h < 0).
[[nodiscard]] core::Real infiltration_depth_decrement(
    core::Real capacity_rate,
    core::Real h,
    core::Real phi_t,
    core::Real dt);

}  // namespace scau::surface2d

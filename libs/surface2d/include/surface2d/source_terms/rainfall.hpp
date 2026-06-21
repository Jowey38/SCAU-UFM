#pragma once

#include "core/types.hpp"

namespace scau::surface2d {

// Depth increment from rainfall over one substep under the DPM storage
// model V = phi_t * h * A. Rain falls on the full plan area, so the stored
// depth rises by rate * dt / phi_t.
[[nodiscard]] core::Real rainfall_depth_increment(
    core::Real rate,
    core::Real phi_t,
    core::Real dt);

}  // namespace scau::surface2d

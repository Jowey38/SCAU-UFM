#pragma once

#include "surface2d/dpm/fields.hpp"
#include "surface2d/geometry/cache.hpp"
#include "surface2d/state/state.hpp"

namespace scau::surface2d {

// Whole-field physical surface water storage in m3:
//   sum(phi_t[i] * h[i] * area[i]) for cells with h[i] >= h_wet.
//
// phi_t owns storage; Phi_c must never enter this audit. The sum uses Neumaier
// compensation. Fail-closed: size mismatch, non-finite or negative h,
// non-finite/non-positive phi_t or area, or invalid h_wet throw
// std::invalid_argument.
[[nodiscard]] double total_physical_surface_volume(
    const SurfaceState& state,
    const DpmFields& dpm,
    const GeometryCache& geometry,
    double h_wet);

}  // namespace scau::surface2d

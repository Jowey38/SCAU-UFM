#pragma once

#include <cstddef>

#include "core/types.hpp"
#include "mesh/mesh.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/geometry/cache.hpp"
#include "surface2d/state/state.hpp"

namespace scau::surface2d {

struct PhiTRemapDiagnostics {
    core::Real volume_before{0.0};
    core::Real volume_after{0.0};
    core::Real momentum_x_before{0.0};
    core::Real momentum_x_after{0.0};
    core::Real momentum_y_before{0.0};
    core::Real momentum_y_after{0.0};
    std::size_t remapped_cells{0U};
};

[[nodiscard]] PhiTRemapDiagnostics remap_state_for_phi_t_change(
    const mesh::Mesh& mesh,
    SurfaceState& state,
    const DpmFields& old_dpm_fields,
    const DpmFields& new_dpm_fields,
    const GeometryCache& geometry);

}  // namespace scau::surface2d

#pragma once

#include "core/types.hpp"
#include "mesh/mesh.hpp"
#include "surface2d/boundary/conditions.hpp"
#include "surface2d/geometry/cache.hpp"
#include "surface2d/state/state.hpp"

namespace scau::surface2d {

struct CflDiagnostics {
    core::Real max_cell_cfl{0.0};
    bool rollback_required{false};
};

[[nodiscard]] core::Real raw_cell_cfl(
    const mesh::Mesh& mesh,
    const SurfaceState& state,
    const BoundaryConditions& boundary,
    const GeometryCache& geometry,
    core::Real dt);

[[nodiscard]] CflDiagnostics evaluate_cfl_diagnostics(
    const mesh::Mesh& mesh,
    const SurfaceState& state,
    const BoundaryConditions& boundary,
    const GeometryCache& geometry,
    core::Real dt,
    core::Real c_rollback);

}  // namespace scau::surface2d

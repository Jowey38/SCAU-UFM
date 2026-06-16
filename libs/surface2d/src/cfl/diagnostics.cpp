#include "surface2d/cfl/diagnostics.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

#include "surface2d/riemann/hllc.hpp"

namespace scau::surface2d {

core::Real raw_cell_cfl(
    const mesh::Mesh& mesh,
    const SurfaceState& state,
    const BoundaryConditions& boundary,
    const GeometryCache& geometry,
    core::Real dt) {
    if (!std::isfinite(dt) || dt <= 0.0) {
        throw std::invalid_argument("CFL dt must be finite and positive");
    }
    validate_state_matches_mesh(state, mesh);
    validate_boundary_conditions_match_mesh(boundary, mesh);
    validate_geometry_cache_matches_mesh(geometry, mesh);
    if (mesh.cells.empty()) {
        throw std::invalid_argument("CFL mesh must contain at least one cell");
    }

    std::vector<core::Real> cell_cfl(mesh.cells.size(), 0.0);
    if (mesh.edges.empty()) {
        return 0.0;
    }

    for (std::size_t edge_index = 0; edge_index < mesh.edges.size(); ++edge_index) {
        const auto& edge = mesh.edges[edge_index];
        const auto& adjacency = geometry.edge_cells[edge_index];
        if (!adjacency.is_internal() && boundary.edges[edge_index] == BoundaryKind::Wall) {
            continue;
        }

        const std::size_t left_index = adjacency.left.has_value() ? *adjacency.left : *adjacency.right;
        const std::size_t right_index = adjacency.right.has_value() ? *adjacency.right : *adjacency.left;
        const auto speeds = estimate_hllc_wave_speeds(
            state.cells[left_index],
            state.cells[right_index],
            Normal2{.x = edge.normal.x, .y = edge.normal.y});
        const core::Real spectral_radius = std::max(std::abs(speeds.s_l), std::abs(speeds.s_r));

        if (adjacency.left.has_value() && geometry.cell_areas[left_index] > 0.0) {
            cell_cfl[left_index] += dt * edge.length * spectral_radius / geometry.cell_areas[left_index];
        }
        if (adjacency.right.has_value() && geometry.cell_areas[right_index] > 0.0) {
            cell_cfl[right_index] += dt * edge.length * spectral_radius / geometry.cell_areas[right_index];
        }
    }

    return *std::max_element(cell_cfl.begin(), cell_cfl.end());
}

CflDiagnostics evaluate_cfl_diagnostics(
    const mesh::Mesh& mesh,
    const SurfaceState& state,
    const BoundaryConditions& boundary,
    const GeometryCache& geometry,
    core::Real dt,
    core::Real c_rollback) {
    if (!std::isfinite(c_rollback) || c_rollback <= 0.0) {
        throw std::invalid_argument("C_rollback must be finite and positive");
    }
    const core::Real max_cell_cfl = raw_cell_cfl(mesh, state, boundary, geometry, dt);
    return CflDiagnostics{
        .max_cell_cfl = max_cell_cfl,
        .rollback_required = max_cell_cfl > c_rollback,
    };
}

}  // namespace scau::surface2d

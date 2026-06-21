#include <cstddef>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/boundary/conditions.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/riemann/hllc.hpp"
#include "surface2d/state/state.hpp"
#include "surface2d/time_integration/step.hpp"

namespace {

std::size_t cell_index_by_id(const scau::mesh::Mesh& mesh, const std::string& cell_id) {
    for (std::size_t index = 0; index < mesh.cells.size(); ++index) {
        if (mesh.cells[index].id == cell_id) {
            return index;
        }
    }
    throw std::invalid_argument("mesh must contain referenced cell");
}

std::size_t first_internal_edge_index(const scau::mesh::Mesh& mesh) {
    for (std::size_t index = 0; index < mesh.edges.size(); ++index) {
        if (mesh.edges[index].left_cell.has_value() && mesh.edges[index].right_cell.has_value()) {
            return index;
        }
    }
    throw std::invalid_argument("mesh must contain an internal edge");
}

std::size_t first_boundary_edge_index(const scau::mesh::Mesh& mesh) {
    for (std::size_t index = 0; index < mesh.edges.size(); ++index) {
        const bool has_left = mesh.edges[index].left_cell.has_value();
        const bool has_right = mesh.edges[index].right_cell.has_value();
        if (has_left != has_right) {
            return index;
        }
    }
    throw std::invalid_argument("mesh must contain a boundary edge");
}

std::size_t edge_cell_index(const scau::mesh::Mesh& mesh, std::size_t edge_index) {
    const auto& edge = mesh.edges[edge_index];
    const auto& cell_id = edge.left_cell.has_value() ? *edge.left_cell : *edge.right_cell;
    for (std::size_t index = 0; index < mesh.cells.size(); ++index) {
        if (mesh.cells[index].id == cell_id) {
            return index;
        }
    }
    throw std::invalid_argument("mesh must contain boundary edge cell");
}

void isolate_edge(scau::surface2d::DpmFields& dpm_fields, std::size_t edge_index) {
    for (auto& edge_fields : dpm_fields.edges) {
        edge_fields.phi_e_n = 0.0;
        edge_fields.omega_edge = 0.0;
    }
    dpm_fields.edges[edge_index].phi_e_n = 1.0;
    dpm_fields.edges[edge_index].omega_edge = 1.0;
}

}  // namespace

// The internal edge carries the HLLC hydrostatic pressure momentum flux. We
// assert the per-edge vector flux (wall-independent); the equal-and-opposite
// application to the two cells is structural (accumulate left -= flux, right
// += flux) and the whole-cell balance is covered by the lake-at-rest test.
TEST(SurfacePressureMomentum, InternalEdgeCarriesHllcPressureMomentumFlux) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto edge_index = first_internal_edge_index(mesh);
    const auto left_index = cell_index_by_id(mesh, *mesh.edges[edge_index].left_cell);
    const auto right_index = cell_index_by_id(mesh, *mesh.edges[edge_index].right_cell);

    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    isolate_edge(dpm_fields, edge_index);

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 10.0};
    const auto normal = scau::surface2d::Normal2{
        .x = mesh.edges[edge_index].normal.x,
        .y = mesh.edges[edge_index].normal.y,
    };
    const auto expected = scau::surface2d::hllc_normal_flux(
        state.cells[left_index], state.cells[right_index], dpm_fields.edges[edge_index], normal, config.h_min);

    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields);

    EXPECT_NE(diagnostics.edges[edge_index].momentum_flux_n, 0.0);
    EXPECT_DOUBLE_EQ(diagnostics.edges[edge_index].momentum_x, expected.momentum_x);
    EXPECT_DOUBLE_EQ(diagnostics.edges[edge_index].momentum_y, expected.momentum_y);
}

TEST(SurfacePressureMomentum, DrySideEdgeContributesNoPressureResidual) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto edge_index = first_internal_edge_index(mesh);
    const auto left_index = cell_index_by_id(mesh, *mesh.edges[edge_index].left_cell);
    const auto right_index = cell_index_by_id(mesh, *mesh.edges[edge_index].right_cell);

    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    state.cells[left_index].conserved.h = 1.0e-8;
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    isolate_edge(dpm_fields, edge_index);

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 10.0, .h_min = 1.0e-6};
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields);

    // The dry-side internal edge carries no flux (asserted on the per-edge
    // diagnostic, which is independent of reflective wall pressure on the cells).
    static_cast<void>(left_index);
    static_cast<void>(right_index);
    EXPECT_EQ(diagnostics.edges[edge_index].mass_flux, 0.0);
    EXPECT_EQ(diagnostics.edges[edge_index].momentum_flux_n, 0.0);
    EXPECT_EQ(diagnostics.edges[edge_index].momentum_x, 0.0);
    EXPECT_EQ(diagnostics.edges[edge_index].momentum_y, 0.0);
}

// Reflective wall applies the inside cell's hydrostatic pressure (M249): zero
// mass flux, but a non-zero pressure momentum flux so a lake at rest balances.
TEST(SurfacePressureMomentum, WallBoundaryAppliesReflectiveHydrostaticPressure) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto edge_index = first_boundary_edge_index(mesh);
    const auto cell_index = edge_cell_index(mesh, edge_index);

    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    isolate_edge(dpm_fields, edge_index);
    auto boundary = scau::surface2d::BoundaryConditions::for_mesh(mesh);
    boundary.edges[edge_index] = scau::surface2d::BoundaryKind::Wall;

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 10.0};
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields, boundary);

    const double h = state.cells[cell_index].conserved.h;
    const double wall_pressure = 0.5 * 9.81 * h * h;
    EXPECT_EQ(diagnostics.edges[edge_index].mass_flux, 0.0);
    EXPECT_DOUBLE_EQ(diagnostics.edges[edge_index].momentum_flux_n, wall_pressure);
    // The wall pressure is delivered to the inside cell (non-zero momentum residual).
    const double residual_norm_squared =
        diagnostics.cells[cell_index].momentum_residual.x * diagnostics.cells[cell_index].momentum_residual.x +
        diagnostics.cells[cell_index].momentum_residual.y * diagnostics.cells[cell_index].momentum_residual.y;
    EXPECT_GT(residual_norm_squared, 0.0);
}

TEST(SurfacePressureMomentum, OpenBoundaryContributesPressureResidual) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto edge_index = first_boundary_edge_index(mesh);
    const auto cell_index = edge_cell_index(mesh, edge_index);

    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    state.cells[cell_index].conserved.hu = 1.0;
    state.cells[cell_index].conserved.hv = 1.0;
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    isolate_edge(dpm_fields, edge_index);
    auto boundary = scau::surface2d::BoundaryConditions::for_mesh(mesh);
    boundary.edges[edge_index] = scau::surface2d::BoundaryKind::Open;

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 10.0};
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields, boundary);

    EXPECT_NE(diagnostics.edges[edge_index].momentum_flux_n, 0.0);
    EXPECT_NE(diagnostics.cells[cell_index].momentum_residual.x, 0.0);
    EXPECT_NE(diagnostics.cells[cell_index].momentum_residual.y, 0.0);
}

TEST(SurfacePressureMomentum, MomentumTransportRegressionStillPasses) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto edge_index = first_internal_edge_index(mesh);
    const auto left_index = cell_index_by_id(mesh, *mesh.edges[edge_index].left_cell);
    const auto right_index = cell_index_by_id(mesh, *mesh.edges[edge_index].right_cell);

    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    state.cells[left_index].conserved.hu = 1.0;
    state.cells[left_index].conserved.hv = 0.5;
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    isolate_edge(dpm_fields, edge_index);

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 10.0};
    static_cast<void>(scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields));

    EXPECT_NE(state.cells[left_index].conserved.hu, 1.0);
    EXPECT_NE(state.cells[right_index].conserved.hu, 0.0);
}

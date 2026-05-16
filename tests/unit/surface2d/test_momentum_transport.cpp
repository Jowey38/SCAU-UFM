#include <cstddef>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/boundary/conditions.hpp"
#include "surface2d/dpm/fields.hpp"
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

TEST(SurfaceMomentumTransport, InternalEdgeMomentumResidualIsAntisymmetric) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto edge_index = first_internal_edge_index(mesh);
    const auto left_index = cell_index_by_id(mesh, *mesh.edges[edge_index].left_cell);
    const auto right_index = cell_index_by_id(mesh, *mesh.edges[edge_index].right_cell);

    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    state.cells[left_index].conserved.hu = 1.0;
    state.cells[left_index].conserved.hv = 0.5;
    state.cells[right_index].conserved.hu = 1.0;
    state.cells[right_index].conserved.hv = 0.5;
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    isolate_edge(dpm_fields, edge_index);

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 10.0};
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields);

    EXPECT_DOUBLE_EQ(
        diagnostics.cells[left_index].momentum_residual.x,
        -diagnostics.cells[right_index].momentum_residual.x);
    EXPECT_DOUBLE_EQ(
        diagnostics.cells[left_index].momentum_residual.y,
        -diagnostics.cells[right_index].momentum_residual.y);
}

TEST(SurfaceMomentumTransport, AdvancesHuHvFromInternalMassFlux) {
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

TEST(SurfaceMomentumTransport, DryCellMomentumIsZeroedAfterDepthClamp) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto edge_index = first_internal_edge_index(mesh);
    const auto left_index = cell_index_by_id(mesh, *mesh.edges[edge_index].left_cell);
    const auto right_index = cell_index_by_id(mesh, *mesh.edges[edge_index].right_cell);

    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    state.cells[left_index].conserved.h = 1.0e-4;
    state.cells[left_index].conserved.hu = 10.0;
    state.cells[left_index].conserved.hv = 10.0;
    state.cells[right_index].conserved.hu = 10.0;
    state.cells[right_index].conserved.hv = 10.0;
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    isolate_edge(dpm_fields, edge_index);

    const scau::surface2d::StepConfig config{.dt = 1.0, .cfl_safety = 0.45, .c_rollback = 100000.0, .h_min = 1.0e-8};
    static_cast<void>(scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields));

    if (state.cells[left_index].conserved.h <= config.h_min) {
        EXPECT_EQ(state.cells[left_index].conserved.hu, 0.0);
        EXPECT_EQ(state.cells[left_index].conserved.hv, 0.0);
    }
    if (state.cells[right_index].conserved.h <= config.h_min) {
        EXPECT_EQ(state.cells[right_index].conserved.hu, 0.0);
        EXPECT_EQ(state.cells[right_index].conserved.hv, 0.0);
    }
}

TEST(SurfaceMomentumTransport, OpenBoundaryContributesMomentumResidual) {
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

    EXPECT_NE(diagnostics.cells[cell_index].momentum_residual.x, 0.0);
    EXPECT_NE(diagnostics.cells[cell_index].momentum_residual.y, 0.0);
}

TEST(SurfaceMomentumTransport, WallBoundaryContributesNoMomentumResidual) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto edge_index = first_boundary_edge_index(mesh);
    const auto cell_index = edge_cell_index(mesh, edge_index);

    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    state.cells[cell_index].conserved.hu = 1.0;
    state.cells[cell_index].conserved.hv = 1.0;
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    isolate_edge(dpm_fields, edge_index);
    auto boundary = scau::surface2d::BoundaryConditions::for_mesh(mesh);
    boundary.edges[edge_index] = scau::surface2d::BoundaryKind::Wall;

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 10.0};
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields, boundary);

    EXPECT_EQ(diagnostics.cells[cell_index].momentum_residual.x, 0.0);
    EXPECT_EQ(diagnostics.cells[cell_index].momentum_residual.y, 0.0);
}

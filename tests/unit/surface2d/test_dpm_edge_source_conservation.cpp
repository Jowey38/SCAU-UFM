#include <cstddef>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/riemann/hllc.hpp"
#include "surface2d/state/state.hpp"
#include "surface2d/time_integration/step.hpp"

namespace {

std::size_t first_internal_edge_index(const scau::mesh::Mesh& mesh) {
    for (std::size_t index = 0; index < mesh.edges.size(); ++index) {
        if (mesh.edges[index].left_cell.has_value() && mesh.edges[index].right_cell.has_value()) {
            return index;
        }
    }
    throw std::invalid_argument("mesh must contain an internal edge");
}

std::size_t cell_index_by_id(const scau::mesh::Mesh& mesh, const std::string& cell_id) {
    for (std::size_t index = 0; index < mesh.cells.size(); ++index) {
        if (mesh.cells[index].id == cell_id) {
            return index;
        }
    }
    throw std::invalid_argument("mesh must contain referenced cell");
}

void isolate_edge(scau::surface2d::DpmFields& dpm_fields, std::size_t edge_index) {
    for (auto& edge_fields : dpm_fields.edges) {
        edge_fields.phi_e_n = 0.0;
        edge_fields.omega_edge = 0.0;
    }
    dpm_fields.edges[edge_index].phi_e_n = 1.0;
    dpm_fields.edges[edge_index].omega_edge = 1.0;
}

void set_edge_aligned_state(
    const scau::mesh::Mesh& mesh,
    std::size_t edge_index,
    scau::surface2d::SurfaceState& state,
    std::size_t left_index,
    std::size_t right_index) {
    const auto& normal = mesh.edges[edge_index].normal;
    const scau::surface2d::Normal2 tangent{.x = -normal.y, .y = normal.x};

    const double left_h = 1.20;
    const double right_h = 0.95;
    const double left_un = 0.70;
    const double right_un = -0.10;
    const double left_ut = 0.20;
    const double right_ut = -0.05;

    state.cells[left_index].conserved.h = left_h;
    state.cells[left_index].conserved.hu = left_h * (left_un * normal.x + left_ut * tangent.x);
    state.cells[left_index].conserved.hv = left_h * (left_un * normal.y + left_ut * tangent.y);
    state.cells[left_index].eta = 1.20;

    state.cells[right_index].conserved.h = right_h;
    state.cells[right_index].conserved.hu = right_h * (right_un * normal.x + right_ut * tangent.x);
    state.cells[right_index].conserved.hv = right_h * (right_un * normal.y + right_ut * tangent.y);
    state.cells[right_index].eta = 1.20;
}

void expect_zero_cell_residual(
    const scau::surface2d::StepDiagnostics& diagnostics,
    std::size_t cell_index) {
    EXPECT_EQ(diagnostics.cells[cell_index].mass_residual, 0.0);
    EXPECT_EQ(diagnostics.cells[cell_index].momentum_residual.x, 0.0);
    EXPECT_EQ(diagnostics.cells[cell_index].momentum_residual.y, 0.0);
}

}  // namespace

TEST(SurfaceDpmEdgeSourceConservation, ActiveInternalEdgeClosesMassMomentumAndPhiTSourcePairing) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto edge_index = first_internal_edge_index(mesh);
    const auto left_index = cell_index_by_id(mesh, *mesh.edges[edge_index].left_cell);
    const auto right_index = cell_index_by_id(mesh, *mesh.edges[edge_index].right_cell);

    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    set_edge_aligned_state(mesh, edge_index, state, left_index, right_index);

    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    isolate_edge(dpm_fields, edge_index);
    dpm_fields.cells[left_index].phi_t = 1.0;
    dpm_fields.cells[right_index].phi_t = 1.35;

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 20.0};
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields);

    ASSERT_EQ(diagnostics.edges.size(), mesh.edges.size());
    ASSERT_EQ(diagnostics.cells.size(), mesh.cells.size());

    EXPECT_NE(diagnostics.edges[edge_index].mass_flux, 0.0);
    EXPECT_NE(diagnostics.edges[edge_index].momentum_flux_n, 0.0);
    EXPECT_NE(diagnostics.edges[edge_index].pressure_pairing, 0.0);
    EXPECT_DOUBLE_EQ(
        diagnostics.edges[edge_index].pressure_pairing + diagnostics.edges[edge_index].s_phi_t,
        0.0);
    EXPECT_DOUBLE_EQ(diagnostics.edges[edge_index].residual, 0.0);

    EXPECT_NE(diagnostics.cells[left_index].mass_residual, 0.0);
    EXPECT_DOUBLE_EQ(
        diagnostics.cells[left_index].mass_residual,
        -diagnostics.cells[right_index].mass_residual);

    const double left_momentum_norm =
        diagnostics.cells[left_index].momentum_residual.x * diagnostics.cells[left_index].momentum_residual.x +
        diagnostics.cells[left_index].momentum_residual.y * diagnostics.cells[left_index].momentum_residual.y;
    EXPECT_NE(left_momentum_norm, 0.0);
    EXPECT_DOUBLE_EQ(
        diagnostics.cells[left_index].momentum_residual.x,
        -diagnostics.cells[right_index].momentum_residual.x);
    EXPECT_DOUBLE_EQ(
        diagnostics.cells[left_index].momentum_residual.y,
        -diagnostics.cells[right_index].momentum_residual.y);
}

TEST(SurfaceDpmEdgeSourceConservation, OmegaEdgeHardBlockZerosTransportResiduals) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto edge_index = first_internal_edge_index(mesh);
    const auto left_index = cell_index_by_id(mesh, *mesh.edges[edge_index].left_cell);
    const auto right_index = cell_index_by_id(mesh, *mesh.edges[edge_index].right_cell);

    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    set_edge_aligned_state(mesh, edge_index, state, left_index, right_index);

    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    isolate_edge(dpm_fields, edge_index);
    dpm_fields.edges[edge_index].omega_edge = 0.0;

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 20.0};
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields);

    ASSERT_EQ(diagnostics.edges.size(), mesh.edges.size());
    EXPECT_EQ(diagnostics.edges[edge_index].mass_flux, 0.0);
    EXPECT_EQ(diagnostics.edges[edge_index].momentum_flux_n, 0.0);
    expect_zero_cell_residual(diagnostics, left_index);
    expect_zero_cell_residual(diagnostics, right_index);
}

TEST(SurfaceDpmEdgeSourceConservation, PhiENSoftBlockZerosTransportResiduals) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto edge_index = first_internal_edge_index(mesh);
    const auto left_index = cell_index_by_id(mesh, *mesh.edges[edge_index].left_cell);
    const auto right_index = cell_index_by_id(mesh, *mesh.edges[edge_index].right_cell);

    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    set_edge_aligned_state(mesh, edge_index, state, left_index, right_index);

    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    isolate_edge(dpm_fields, edge_index);
    dpm_fields.edges[edge_index].phi_e_n = scau::surface2d::PhiEdgeMin * 0.5;

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 20.0};
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields);

    ASSERT_EQ(diagnostics.edges.size(), mesh.edges.size());
    EXPECT_EQ(diagnostics.edges[edge_index].mass_flux, 0.0);
    EXPECT_EQ(diagnostics.edges[edge_index].momentum_flux_n, 0.0);
    expect_zero_cell_residual(diagnostics, left_index);
    expect_zero_cell_residual(diagnostics, right_index);
}

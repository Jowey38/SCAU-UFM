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

std::size_t left_cell_index(const scau::mesh::Mesh& mesh, std::size_t edge_index) {
    return cell_index_by_id(mesh, *mesh.edges[edge_index].left_cell);
}

std::size_t right_cell_index(const scau::mesh::Mesh& mesh, std::size_t edge_index) {
    return cell_index_by_id(mesh, *mesh.edges[edge_index].right_cell);
}

}  // namespace

TEST(Surface2DGoldenMinimal, G1MixedMeshHydrostaticStateProducesZeroFluxes) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    const auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    const scau::surface2d::StepConfig config{.dt = 0.5, .cfl_safety = 0.45, .c_rollback = 1.0};

    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields);

    ASSERT_EQ(diagnostics.edges.size(), mesh.edges.size());
    for (const auto& edge : diagnostics.edges) {
        EXPECT_EQ(edge.mass_flux, 0.0);
        EXPECT_EQ(edge.momentum_flux_n, 0.0);
    }
    EXPECT_EQ(diagnostics.max_cell_cfl, 0.0);
    EXPECT_FALSE(diagnostics.rollback_required);
}

TEST(Surface2DGoldenMinimal, G2QuadOnlyMeshPhiTPressurePairingCancelsSource) {
    const auto mesh = scau::mesh::build_quad_only_control_mesh();
    const auto internal_edge_index = first_internal_edge_index(mesh);
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    dpm_fields.cells[left_cell_index(mesh, internal_edge_index)].phi_t = 1.0;
    dpm_fields.cells[right_cell_index(mesh, internal_edge_index)].phi_t = 1.25;
    const scau::surface2d::StepConfig config{.dt = 0.5, .cfl_safety = 0.45, .c_rollback = 1.0};

    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields);

    ASSERT_EQ(diagnostics.edges.size(), mesh.edges.size());
    EXPECT_DOUBLE_EQ(diagnostics.edges[internal_edge_index].pressure_pairing, 1.22625);
    EXPECT_DOUBLE_EQ(diagnostics.edges[internal_edge_index].s_phi_t, -1.22625);
    EXPECT_DOUBLE_EQ(diagnostics.edges[internal_edge_index].residual, 0.0);
}

TEST(Surface2DGoldenMinimal, G3TriOnlyMeshBlockedDpmEdgeZerosFlux) {
    const auto mesh = scau::mesh::build_tri_only_control_mesh();
    const auto internal_edge_index = first_internal_edge_index(mesh);
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    state.cells[left_cell_index(mesh, internal_edge_index)].conserved.hu = 1.0;
    state.cells[right_cell_index(mesh, internal_edge_index)].conserved.hu = 1.0;
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    dpm_fields.edges[internal_edge_index].omega_edge = 0.0;
    dpm_fields.edges[internal_edge_index].phi_e_n = scau::surface2d::PhiEdgeMin * 0.5;
    const scau::surface2d::StepConfig config{.dt = 0.5, .cfl_safety = 0.45, .c_rollback = 1.0};

    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields);

    ASSERT_EQ(diagnostics.edges.size(), mesh.edges.size());
    EXPECT_EQ(diagnostics.edges[internal_edge_index].mass_flux, 0.0);
    EXPECT_EQ(diagnostics.edges[internal_edge_index].momentum_flux_n, 0.0);
}

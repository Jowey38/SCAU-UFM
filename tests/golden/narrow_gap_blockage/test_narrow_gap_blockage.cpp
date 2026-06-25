#include <cstddef>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/state/state.hpp"
#include "surface2d/time_integration/step.hpp"

namespace {
std::size_t first_internal_edge_index(const scau::mesh::Mesh& mesh) {
    for (std::size_t i = 0; i < mesh.edges.size(); ++i) {
        if (mesh.edges[i].left_cell.has_value() && mesh.edges[i].right_cell.has_value()) return i;
    }
    throw std::invalid_argument("mesh must contain an internal edge");
}
std::size_t cell_index_by_id(const scau::mesh::Mesh& mesh, const std::string& id) {
    for (std::size_t i = 0; i < mesh.cells.size(); ++i) {
        if (mesh.cells[i].id == id) return i;
    }
    throw std::invalid_argument("mesh must contain referenced cell");
}
}

TEST(NarrowGapBlockageGolden, HardBlockHasExactZeroFlux) {
    const auto mesh = scau::mesh::build_tri_only_control_mesh();
    const auto edge_index = first_internal_edge_index(mesh);
    const auto left_index = cell_index_by_id(mesh, *mesh.edges[edge_index].left_cell);
    const auto right_index = cell_index_by_id(mesh, *mesh.edges[edge_index].right_cell);

    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    state.cells[left_index].conserved.hu = 1.0;
    state.cells[right_index].conserved.hu = 1.0;
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    dpm_fields.edges[edge_index].omega_edge = 0.0;
    dpm_fields.edges[edge_index].phi_e_n = 0.0;
    const scau::surface2d::StepConfig config{.dt = 0.5, .cfl_safety = 0.45, .c_rollback = 1.0};

    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields);

    EXPECT_EQ(diagnostics.edges[edge_index].mass_flux, 0.0);
    EXPECT_EQ(diagnostics.edges[edge_index].momentum_flux_n, 0.0);
    EXPECT_EQ(diagnostics.edges[edge_index].momentum_x, 0.0);
    EXPECT_EQ(diagnostics.edges[edge_index].momentum_y, 0.0);
}

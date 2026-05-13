#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/state/state.hpp"

TEST(SurfaceState, CreatesCellAlignedState) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto state = scau::surface2d::SurfaceState::for_mesh(mesh);

    ASSERT_EQ(state.cells.size(), mesh.cells.size());
    for (const auto& cell_state : state.cells) {
        EXPECT_EQ(cell_state.conserved.h, 0.0);
        EXPECT_EQ(cell_state.conserved.hu, 0.0);
        EXPECT_EQ(cell_state.conserved.hv, 0.0);
    }
}

TEST(SurfaceState, RejectsWrongCellCount) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    scau::surface2d::SurfaceState state;
    state.cells.resize(mesh.cells.size() - 1U);

    EXPECT_THROW(scau::surface2d::validate_state_matches_mesh(state, mesh), std::invalid_argument);
}

TEST(SurfaceState, CreatesHydrostaticCellAlignedState) {
    const auto mesh = scau::mesh::build_quad_only_control_mesh();
    const auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);

    ASSERT_EQ(state.cells.size(), mesh.cells.size());
    for (const auto& cell_state : state.cells) {
        EXPECT_EQ(cell_state.conserved.h, 1.0);
        EXPECT_EQ(cell_state.conserved.hu, 0.0);
        EXPECT_EQ(cell_state.conserved.hv, 0.0);
        EXPECT_EQ(cell_state.eta, 1.0);
    }
}

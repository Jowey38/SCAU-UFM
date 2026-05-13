#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/state/state.hpp"
#include "surface2d/time_integration/step.hpp"

TEST(SurfaceStep, CpuSkeletonPreservesStateAndReportsDiagnostics) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::for_mesh(mesh);
    state.cells[0].conserved.h = 1.25;
    state.cells[0].conserved.hu = 0.5;
    state.cells[0].conserved.hv = -0.25;

    const scau::surface2d::StepConfig config{.dt = 0.5, .cfl_safety = 0.45, .c_rollback = 1.0};
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config);

    EXPECT_EQ(state.cells[0].conserved.h, 1.25);
    EXPECT_EQ(state.cells[0].conserved.hu, 0.5);
    EXPECT_EQ(state.cells[0].conserved.hv, -0.25);
    EXPECT_EQ(diagnostics.cell_count, mesh.cells.size());
    EXPECT_EQ(diagnostics.edge_count, mesh.edges.size());
    EXPECT_EQ(diagnostics.max_cell_cfl, 0.0);
    EXPECT_FALSE(diagnostics.rollback_required);
    EXPECT_TRUE(diagnostics.edges.empty());
}

TEST(SurfaceStep, CpuStepReportsDpmEdgeDiagnostics) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    dpm_fields.cells[1].phi_t = 1.25;

    const scau::surface2d::StepConfig config{.dt = 0.5, .cfl_safety = 0.45, .c_rollback = 1.0};
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields);

    ASSERT_EQ(diagnostics.edges.size(), mesh.edges.size());
    EXPECT_EQ(diagnostics.edges[1].mass_flux, 0.0);
    EXPECT_DOUBLE_EQ(diagnostics.edges[1].pressure_pairing, 1.22625);
    EXPECT_DOUBLE_EQ(diagnostics.edges[1].s_phi_t, -1.22625);
    EXPECT_DOUBLE_EQ(diagnostics.edges[1].residual, 0.0);
    EXPECT_EQ(diagnostics.max_cell_cfl, 0.0);
    EXPECT_FALSE(diagnostics.rollback_required);
}

TEST(SurfaceStep, RejectsInvalidStepInputs) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::for_mesh(mesh);

    EXPECT_THROW(
        static_cast<void>(scau::surface2d::advance_one_step_cpu(
            mesh,
            state,
            scau::surface2d::StepConfig{.dt = 0.0, .cfl_safety = 0.45, .c_rollback = 1.0})),
        std::invalid_argument);

    state.cells.pop_back();
    EXPECT_THROW(
        static_cast<void>(scau::surface2d::advance_one_step_cpu(
            mesh,
            state,
            scau::surface2d::StepConfig{.dt = 0.5, .cfl_safety = 0.45, .c_rollback = 1.0})),
        std::invalid_argument);
}

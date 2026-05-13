#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/state/state.hpp"
#include "surface2d/time_integration/step.hpp"

TEST(SurfaceCflRollback, ReportsRawPhysicalCflWithoutSafetyScaling) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    state.cells[0].conserved.hu = 2.0;
    const auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);

    const scau::surface2d::StepConfig config{.dt = 0.5, .cfl_safety = 0.25, .c_rollback = 10.0};
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields);

    EXPECT_DOUBLE_EQ(diagnostics.max_cell_cfl, 1.0);
    EXPECT_FALSE(diagnostics.rollback_required);
}

TEST(SurfaceCflRollback, ComparesRollbackAgainstCRollbackOnly) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    state.cells[0].conserved.hu = 2.0;
    const auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);

    const scau::surface2d::StepConfig config{.dt = 0.5, .cfl_safety = 0.25, .c_rollback = 0.75};
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields);

    EXPECT_DOUBLE_EQ(diagnostics.max_cell_cfl, 1.0);
    EXPECT_TRUE(diagnostics.rollback_required);
}

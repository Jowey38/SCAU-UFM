#include <algorithm>
#include <cmath>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/state/state.hpp"
#include "surface2d/time_integration/step.hpp"
#include "tests/golden/golden_tolerances.hpp"

namespace {
double max_abs_velocity(const scau::surface2d::SurfaceState& state) {
    double worst = 0.0;
    for (const auto& cell : state.cells) {
        worst = std::max({worst, std::abs(cell.u()), std::abs(cell.v())});
    }
    return worst;
}
}

TEST(HydrostaticStepGolden, PreservesEtaAndVelocityAtRest) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    const auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    const scau::surface2d::StepConfig config{.dt = 0.5, .cfl_safety = 0.45, .c_rollback = 10.0};

    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields);

    ASSERT_FALSE(diagnostics.rollback_required);
    EXPECT_NEAR(max_abs_velocity(state), 0.0, scau::golden::u_hydro_tol);
    for (const auto& cell : state.cells) {
        EXPECT_NEAR(cell.eta, 1.0, scau::golden::eta_tol);
    }
}

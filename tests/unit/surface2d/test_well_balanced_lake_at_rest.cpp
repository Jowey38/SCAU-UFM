#include <algorithm>
#include <cmath>
#include <cstddef>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/state/state.hpp"
#include "surface2d/time_integration/step.hpp"

namespace {

double max_abs_momentum(const scau::surface2d::SurfaceState& state) {
    double worst = 0.0;
    for (const auto& cell : state.cells) {
        worst = std::max({worst, std::abs(cell.conserved.hu), std::abs(cell.conserved.hv)});
    }
    return worst;
}

}  // namespace

// Lake at rest: uniform depth, zero velocity, walled domain, uniform phi_t.
// A well-balanced scheme must keep momentum exactly zero (no spurious velocity).
// Locks the reflective-wall hydrostatic-pressure balance landed in M249.
TEST(WellBalancedLakeAtRest, UniformPhiTWalledDomainKeepsMomentumZero) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    const auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 100.0, .h_min = 1.0e-8};

    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields);

    ASSERT_FALSE(diagnostics.rollback_required);
    EXPECT_NEAR(max_abs_momentum(state), 0.0, 1.0e-12);
}

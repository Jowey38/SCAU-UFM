#include <algorithm>
#include <cmath>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/boundary/conditions.hpp"
#include "surface2d/dpm/edge_conveyance.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/dpm/tensor_projection.hpp"
#include "surface2d/geometry/cache.hpp"
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

TEST(PhiCEdgeZeroVelocityGolden, JumpDoesNotCreateSpuriousVelocity) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto geometry = scau::surface2d::GeometryCache::for_mesh(mesh);
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    for (std::size_t i = 0; i < dpm_fields.cells.size(); ++i) {
        dpm_fields.cells[i].Phi_c = (i % 2 == 0)
            ? scau::surface2d::Tensor2Symmetric{.xx = 1.0, .xy = 0.0, .yy = 1.0}
            : scau::surface2d::Tensor2Symmetric{.xx = 1.0, .xy = 0.0, .yy = 9.0};
    }
    static_cast<void>(scau::surface2d::assemble_edge_conveyance_from_tensors(mesh, geometry, dpm_fields));
    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 100.0, .h_min = 1.0e-8};

    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields);

    ASSERT_FALSE(diagnostics.rollback_required);
    EXPECT_NEAR(max_abs_velocity(state), 0.0, scau::golden::u_hydro_tol);
}

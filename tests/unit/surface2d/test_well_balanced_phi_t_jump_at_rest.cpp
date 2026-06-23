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

// G4: flat bed (eta = h uniform => z_b = 0), zero velocity, walled domain,
// phi_t jump across cells. With the phi_t-scaled WB pressure + S_phi_t pairing
// the lake stays exactly at rest.
TEST(WellBalancedPhiTJumpAtRest, FlatBedPhiTJumpKeepsMomentumZero) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    for (std::size_t i = 0; i < dpm_fields.cells.size(); ++i) {
        dpm_fields.cells[i].phi_t = (i % 2 == 0) ? 1.0 : 1.5;  // strong jump
    }
    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 100.0, .h_min = 1.0e-8};

    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields);

    ASSERT_FALSE(diagnostics.rollback_required);
    EXPECT_NEAR(max_abs_momentum(state), 0.0, 1.0e-12);
}

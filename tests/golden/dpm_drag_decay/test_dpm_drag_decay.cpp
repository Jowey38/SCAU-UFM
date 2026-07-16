#include <algorithm>
#include <cmath>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/boundary/conditions.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/geometry/cache.hpp"
#include "surface2d/source_terms/fields.hpp"
#include "surface2d/state/state.hpp"
#include "surface2d/time_integration/step.hpp"
#include "tests/golden/golden_tolerances.hpp"

namespace {
double system_mass(
    const scau::surface2d::SurfaceState& state,
    const scau::surface2d::DpmFields& dpm_fields,
    const scau::surface2d::GeometryCache& geometry) {
    double total = 0.0;
    for (std::size_t i = 0; i < state.cells.size(); ++i) {
        total += state.cells[i].conserved.h * dpm_fields.cells[i].phi_t * geometry.cell_areas[i];
    }
    return total;
}
}

TEST(DpmDragDecayGolden, DragDecaysMomentumWithoutCreatingMass) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto geometry = scau::surface2d::GeometryCache::for_mesh(mesh);
    auto state_drag = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    for (auto& cell : state_drag.cells) {
        cell.conserved.hu = 1.0;
    }
    auto state_control = state_drag;
    const auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    const auto boundary = scau::surface2d::BoundaryConditions::for_mesh(mesh);
    auto sources_drag = scau::surface2d::SourceTermFields::for_mesh(mesh);
    for (auto& n : sources_drag.manning_n) {
        n = 0.05;
    }
    const auto sources_control = scau::surface2d::SourceTermFields::for_mesh(mesh);
    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 100.0, .h_min = 1.0e-8};

    const double mass_before = system_mass(state_drag, dpm_fields, geometry);
    static_cast<void>(scau::surface2d::advance_one_step_cpu(mesh, state_drag, config, dpm_fields, boundary, sources_drag, geometry));
    static_cast<void>(scau::surface2d::advance_one_step_cpu(mesh, state_control, config, dpm_fields, boundary, sources_control, geometry));
    const double mass_after = system_mass(state_drag, dpm_fields, geometry);

    bool any_strictly_reduced = false;
    for (std::size_t i = 0; i < state_drag.cells.size(); ++i) {
        const double hu_drag = state_drag.cells[i].conserved.hu;
        const double hu_control = state_control.cells[i].conserved.hu;
        EXPECT_LE(std::abs(hu_drag), std::abs(hu_control));
        if (std::abs(hu_control) > 1.0e-12) {
            EXPECT_LT(std::abs(hu_drag), std::abs(hu_control));
            any_strictly_reduced = true;
        }
    }
    EXPECT_TRUE(any_strictly_reduced);
    EXPECT_NEAR(mass_after - mass_before, 0.0, scau::golden::conservation_near_tol);
}

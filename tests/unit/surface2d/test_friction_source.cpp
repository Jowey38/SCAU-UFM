#include <cmath>
#include <stdexcept>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/source_terms/fields.hpp"
#include "surface2d/source_terms/friction.hpp"
#include "surface2d/state/state.hpp"
#include "surface2d/time_integration/step.hpp"

namespace {

scau::surface2d::ConservedState wet_moving_cell() {
    return scau::surface2d::ConservedState{.h = 0.5, .hu = 1.0, .hv = -0.5};
}

}  // namespace

TEST(FrictionSource, ZeroManningLeavesMomentumUnchanged) {
    const auto before = wet_moving_cell();
    const auto after = scau::surface2d::apply_manning_friction(before, 0.0, 0.1, 1.0e-8);
    EXPECT_EQ(after.hu, before.hu);
    EXPECT_EQ(after.hv, before.hv);
    EXPECT_EQ(after.h, before.h);
}

TEST(FrictionSource, ReducesSpeedAndPreservesDirection) {
    const auto before = wet_moving_cell();
    const auto after = scau::surface2d::apply_manning_friction(before, 0.05, 0.1, 1.0e-8);
    EXPECT_LT(std::abs(after.hu), std::abs(before.hu));
    EXPECT_LT(std::abs(after.hv), std::abs(before.hv));
    EXPECT_GT(after.hu, 0.0);
    EXPECT_LT(after.hv, 0.0);
    EXPECT_NEAR(after.hu / after.hv, before.hu / before.hv, 1.0e-12);
    EXPECT_EQ(after.h, before.h);
}

TEST(FrictionSource, SemiImplicitMatchesClosedForm) {
    const scau::surface2d::ConservedState before{.h = 1.0, .hu = 2.0, .hv = 0.0};
    const double manning_n = 0.03;
    const double dt = 0.5;
    const double gravity = 9.81;
    const double speed = 2.0;
    const double denom = 1.0 + dt * gravity * manning_n * manning_n * speed / std::pow(1.0, 4.0 / 3.0);
    const auto after = scau::surface2d::apply_manning_friction(before, manning_n, dt, 1.0e-8, gravity);
    EXPECT_NEAR(after.hu, 2.0 / denom, 1.0e-12);
    EXPECT_EQ(after.hv, 0.0);
}

TEST(FrictionSource, DryCellMomentumIsZeroed) {
    const scau::surface2d::ConservedState before{.h = 1.0e-9, .hu = 1.0, .hv = 1.0};
    const auto after = scau::surface2d::apply_manning_friction(before, 0.03, 0.1, 1.0e-8);
    EXPECT_EQ(after.hu, 0.0);
    EXPECT_EQ(after.hv, 0.0);
}

TEST(FrictionSource, InvalidInputsFailClosed) {
    const auto cell = wet_moving_cell();
    EXPECT_THROW(
        static_cast<void>(scau::surface2d::apply_manning_friction(cell, -0.01, 0.1, 1.0e-8)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::surface2d::apply_manning_friction(cell, 0.03, 0.0, 1.0e-8)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::surface2d::apply_manning_friction(cell, 0.03, 0.1, -1.0)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::surface2d::apply_manning_friction(cell, 0.03, 0.1, 1.0e-8, 0.0)),
        std::invalid_argument);
}

TEST(FrictionSource, StepAppliesFrictionToMovingCells) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state_friction = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    for (auto& cell : state_friction.cells) {
        cell.conserved.hu = 1.0;
    }
    auto state_control = state_friction;

    const auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    const auto boundary = scau::surface2d::BoundaryConditions::for_mesh(mesh);
    auto sources_friction = scau::surface2d::SourceTermFields::for_mesh(mesh);
    for (auto& manning_n : sources_friction.manning_n) {
        manning_n = 0.05;
    }
    const auto sources_control = scau::surface2d::SourceTermFields::for_mesh(mesh);

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 100.0, .h_min = 1.0e-8};
    const auto diag_friction = scau::surface2d::advance_one_step_cpu(
        mesh, state_friction, config, dpm_fields, boundary, sources_friction);
    const auto diag_control = scau::surface2d::advance_one_step_cpu(
        mesh, state_control, config, dpm_fields, boundary, sources_control);

    ASSERT_FALSE(diag_friction.rollback_required);
    ASSERT_FALSE(diag_control.rollback_required);
    bool any_strictly_reduced = false;
    for (std::size_t index = 0; index < state_friction.cells.size(); ++index) {
        const double hu_friction = state_friction.cells[index].conserved.hu;
        const double hu_control = state_control.cells[index].conserved.hu;
        EXPECT_LE(std::abs(hu_friction), std::abs(hu_control));
        if (std::abs(hu_control) > 1.0e-12) {
            EXPECT_LT(std::abs(hu_friction), std::abs(hu_control));
            any_strictly_reduced = true;
        }
    }
    EXPECT_TRUE(any_strictly_reduced);
}

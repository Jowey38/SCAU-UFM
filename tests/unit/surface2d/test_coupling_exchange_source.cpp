#include <stdexcept>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/geometry/cache.hpp"
#include "surface2d/source_terms/coupling_exchange.hpp"
#include "surface2d/source_terms/fields.hpp"
#include "surface2d/state/state.hpp"
#include "surface2d/time_integration/step.hpp"

TEST(CouplingExchangeSource, PositiveVolumeRaisesDepth) {
    const auto result = scau::surface2d::exchange_depth_increment(2.0, 1.0, 0.5, 4.0);
    EXPECT_NEAR(result.depth_increment, 1.0, 1.0e-15);
    EXPECT_NEAR(result.applied_volume, 2.0, 1.0e-15);
}

TEST(CouplingExchangeSource, NegativeVolumeClampsAtZeroDepth) {
    const auto result = scau::surface2d::exchange_depth_increment(-10.0, 0.5, 1.0, 2.0);
    EXPECT_NEAR(result.depth_increment, -0.5, 1.0e-15);
    EXPECT_NEAR(result.applied_volume, -1.0, 1.0e-15);
}

TEST(CouplingExchangeSource, InvalidInputsFailClosed) {
    EXPECT_THROW(
        static_cast<void>(scau::surface2d::exchange_depth_increment(1.0, -1.0, 1.0, 1.0)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::surface2d::exchange_depth_increment(1.0, 1.0, 0.0, 1.0)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::surface2d::exchange_depth_increment(1.0, 1.0, 1.0, 0.0)),
        std::invalid_argument);
}

TEST(CouplingExchangeSource, StepReportsAppliedVolumes) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    const auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    const auto geometry = scau::surface2d::GeometryCache::for_mesh(mesh);
    auto sources = scau::surface2d::SourceTermFields::for_mesh(mesh);
    sources.exchange_volume[0] = 0.25;

    const double h_before = state.cells[0].conserved.h;

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 100.0, .h_min = 1.0e-8};
    const auto boundary = scau::surface2d::BoundaryConditions::for_mesh(mesh);
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(
        mesh, state, config, dpm_fields, boundary, sources, geometry);

    ASSERT_FALSE(diagnostics.rollback_required);
    EXPECT_NEAR(diagnostics.exchange_volume, 0.25, 1.0e-12);
    EXPECT_NEAR(
        state.cells[0].conserved.h - h_before,
        0.25 / geometry.cell_areas[0],
        1.0e-12);
}

TEST(CouplingExchangeSource, StepDrainClampsAtZeroAndReportsActualVolume) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 0.01);
    const auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    const auto geometry = scau::surface2d::GeometryCache::for_mesh(mesh);
    auto sources = scau::surface2d::SourceTermFields::for_mesh(mesh);
    sources.exchange_volume[0] = -100.0;

    const double available = 0.01 * geometry.cell_areas[0];

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 100.0, .h_min = 1.0e-8};
    const auto boundary = scau::surface2d::BoundaryConditions::for_mesh(mesh);
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(
        mesh, state, config, dpm_fields, boundary, sources, geometry);

    ASSERT_FALSE(diagnostics.rollback_required);
    EXPECT_GE(state.cells[0].conserved.h, 0.0);
    EXPECT_NEAR(state.cells[0].conserved.h, 0.0, 1.0e-12);
    EXPECT_NEAR(diagnostics.exchange_volume, -available, 1.0e-9);
}

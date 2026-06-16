#include <stdexcept>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/geometry/cache.hpp"
#include "surface2d/source_terms/fields.hpp"
#include "surface2d/source_terms/infiltration.hpp"
#include "surface2d/source_terms/rainfall.hpp"
#include "surface2d/state/state.hpp"
#include "surface2d/time_integration/step.hpp"

namespace {

double system_volume(
    const scau::surface2d::SurfaceState& state,
    const scau::surface2d::DpmFields& dpm_fields,
    const scau::surface2d::GeometryCache& geometry) {
    double volume = 0.0;
    for (std::size_t index = 0; index < state.cells.size(); ++index) {
        volume += state.cells[index].conserved.h * dpm_fields.cells[index].phi_t * geometry.cell_areas[index];
    }
    return volume;
}

}  // namespace

TEST(RainfallSource, DepthIncrementScalesInverselyWithPhiT) {
    EXPECT_NEAR(scau::surface2d::rainfall_depth_increment(1.0e-3, 1.0, 10.0), 1.0e-2, 1.0e-15);
    EXPECT_NEAR(scau::surface2d::rainfall_depth_increment(1.0e-3, 0.5, 10.0), 2.0e-2, 1.0e-15);
}

TEST(RainfallSource, InvalidInputsFailClosed) {
    EXPECT_THROW(
        static_cast<void>(scau::surface2d::rainfall_depth_increment(-1.0, 1.0, 1.0)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::surface2d::rainfall_depth_increment(1.0, 0.0, 1.0)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::surface2d::rainfall_depth_increment(1.0, 1.1, 1.0)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::surface2d::rainfall_depth_increment(1.0, 1.0, 0.0)),
        std::invalid_argument);
}

TEST(InfiltrationSource, ClampsAtAvailableDepth) {
    EXPECT_NEAR(scau::surface2d::infiltration_depth_decrement(1.0e-3, 1.0, 1.0, 10.0), 1.0e-2, 1.0e-15);
    EXPECT_NEAR(scau::surface2d::infiltration_depth_decrement(1.0, 5.0e-3, 1.0, 10.0), 5.0e-3, 1.0e-15);
    EXPECT_EQ(scau::surface2d::infiltration_depth_decrement(1.0, 0.0, 1.0, 10.0), 0.0);
}

TEST(InfiltrationSource, InvalidInputsFailClosed) {
    EXPECT_THROW(
        static_cast<void>(scau::surface2d::infiltration_depth_decrement(-1.0, 1.0, 1.0, 1.0)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::surface2d::infiltration_depth_decrement(1.0, -1.0, 1.0, 1.0)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::surface2d::infiltration_depth_decrement(1.0, 1.0, 0.0, 1.0)),
        std::invalid_argument);
}

TEST(RainfallSource, ClosedBoxStepConservesRainfallVolume) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    const auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    const auto geometry = scau::surface2d::GeometryCache::for_mesh(mesh);
    auto sources = scau::surface2d::SourceTermFields::for_mesh(mesh);
    for (auto& rate : sources.rainfall_rate) {
        rate = 2.0e-3;
    }

    const double volume_before = system_volume(state, dpm_fields, geometry);
    double expected_added = 0.0;
    for (const double area : geometry.cell_areas) {
        expected_added += 2.0e-3 * 0.5 * area;
    }

    const scau::surface2d::StepConfig config{.dt = 0.5, .cfl_safety = 0.45, .c_rollback = 100.0, .h_min = 1.0e-8};
    const auto boundary = scau::surface2d::BoundaryConditions::for_mesh(mesh);
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(
        mesh, state, config, dpm_fields, boundary, sources, geometry);

    ASSERT_FALSE(diagnostics.rollback_required);
    const double volume_after = system_volume(state, dpm_fields, geometry);
    EXPECT_NEAR(volume_after - volume_before, expected_added, 1.0e-9);
    EXPECT_NEAR(diagnostics.rainfall_volume, expected_added, 1.0e-9);
    EXPECT_EQ(diagnostics.infiltration_volume, 0.0);
    EXPECT_EQ(diagnostics.exchange_volume, 0.0);
}

TEST(InfiltrationSource, ClosedBoxStepRemovesClampedVolume) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0e-3);
    const auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    const auto geometry = scau::surface2d::GeometryCache::for_mesh(mesh);
    auto sources = scau::surface2d::SourceTermFields::for_mesh(mesh);
    for (auto& rate : sources.infiltration_rate) {
        rate = 1.0;
    }

    const double volume_before = system_volume(state, dpm_fields, geometry);

    const scau::surface2d::StepConfig config{.dt = 0.5, .cfl_safety = 0.45, .c_rollback = 100.0, .h_min = 1.0e-8};
    const auto boundary = scau::surface2d::BoundaryConditions::for_mesh(mesh);
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(
        mesh, state, config, dpm_fields, boundary, sources, geometry);

    ASSERT_FALSE(diagnostics.rollback_required);
    for (const auto& cell : state.cells) {
        EXPECT_GE(cell.conserved.h, 0.0);
        EXPECT_NEAR(cell.conserved.h, 0.0, 1.0e-12);
    }
    EXPECT_NEAR(diagnostics.infiltration_volume, volume_before, 1.0e-9);
}

TEST(SourceTermFields, MismatchedSizesFailClosed) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    const auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    auto sources = scau::surface2d::SourceTermFields::for_mesh(mesh);
    sources.rainfall_rate.pop_back();

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 100.0, .h_min = 1.0e-8};
    const auto boundary = scau::surface2d::BoundaryConditions::for_mesh(mesh);
    EXPECT_THROW(
        static_cast<void>(scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields, boundary, sources)),
        std::invalid_argument);
}

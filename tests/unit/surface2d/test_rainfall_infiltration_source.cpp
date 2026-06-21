#include <stdexcept>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/boundary/conditions.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/geometry/cache.hpp"
#include "surface2d/source_terms/fields.hpp"
#include "surface2d/source_terms/infiltration.hpp"
#include "surface2d/source_terms/rainfall.hpp"
#include "surface2d/source_terms/runoff/fields.hpp"
#include "surface2d/source_terms/runoff/step_inputs.hpp"
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

TEST(RainfallSource, ClosedBoxStepConservesRainfallVolumeViaRunoff) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    const auto dpm = scau::surface2d::DpmFields::for_mesh(mesh);
    const auto geom = scau::surface2d::GeometryCache::for_mesh(mesh);
    const auto boundary = scau::surface2d::BoundaryConditions::for_mesh(mesh);
    const auto sources = scau::surface2d::SourceTermFields::for_mesh(mesh);
    auto rs = scau::surface2d::RunoffState::for_mesh(mesh);
    auto in = scau::surface2d::RunoffStepInputs::for_mesh(mesh);
    for (std::size_t i = 0; i < mesh.cells.size(); ++i) {
        in.fields.pervious_fraction[i] = 0.0;
        in.fields.impervious_fraction[i] = 1.0;  // no infiltration -> all rain conserved into h
        in.rainfall_rate[i] = 2.0e-3;
    }
    const double before = system_volume(state, dpm, geom);
    double expected_added = 0.0;
    for (const double a : geom.cell_areas) expected_added += 2.0e-3 * 0.5 * a;
    const scau::surface2d::StepConfig config{.dt = 0.5, .cfl_safety = 0.45, .c_rollback = 100.0, .h_min = 1.0e-8};
    const auto diag = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm, boundary, sources, geom, in, rs);
    ASSERT_FALSE(diag.rollback_required);
    EXPECT_NEAR(system_volume(state, dpm, geom) - before, expected_added, 1.0e-9);
    EXPECT_NEAR(diag.surface_added_volume, expected_added, 1.0e-9);
}

TEST(SourceTermFields, MismatchedSizesFailClosed) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    const auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    auto sources = scau::surface2d::SourceTermFields::for_mesh(mesh);
    sources.manning_n.pop_back();

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 100.0, .h_min = 1.0e-8};
    const auto boundary = scau::surface2d::BoundaryConditions::for_mesh(mesh);
    EXPECT_THROW(
        static_cast<void>(scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields, boundary, sources)),
        std::invalid_argument);
}

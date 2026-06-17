#include <stdexcept>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/geometry/cache.hpp"
#include "surface2d/source_terms/runoff/fields.hpp"
#include "surface2d/source_terms/runoff/step_inputs.hpp"
#include "surface2d/state/state.hpp"
#include "surface2d/time_integration/step.hpp"

namespace {
using scau::surface2d::RunoffStepInputs;
}  // namespace

TEST(RunoffStepInputs, ForMeshHasNeutralDefaults) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto in = RunoffStepInputs::for_mesh(mesh);
    ASSERT_EQ(in.rainfall_rate.size(), mesh.cells.size());
    ASSERT_EQ(in.fields.pervious_fraction.size(), mesh.cells.size());
    ASSERT_FALSE(in.lut.entries.empty());
    EXPECT_GT(in.f_inf_floor, 0.0);
    for (std::size_t i = 0; i < mesh.cells.size(); ++i) {
        EXPECT_EQ(in.rainfall_rate[i], 0.0);
        EXPECT_EQ(in.fields.pervious_fraction[i], 1.0);
    }
}

TEST(RunoffStepInputs, MismatchedRainfallSizeFailsClosed) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto in = RunoffStepInputs::for_mesh(mesh);
    in.rainfall_rate.pop_back();
    EXPECT_THROW(scau::surface2d::validate_runoff_step_inputs_match_mesh(in, mesh), std::invalid_argument);
}

TEST(GroundRunoffStage, ImperviousRainAddsNetRunoffDepth) {
    // Fully impervious cells, no losses: all rain becomes surface runoff.
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    const auto dpm = scau::surface2d::DpmFields::for_mesh(mesh);          // phi_t defaults to 1.0
    const auto geom = scau::surface2d::GeometryCache::for_mesh(mesh);
    auto rs = scau::surface2d::RunoffState::for_mesh(mesh);
    auto in = scau::surface2d::RunoffStepInputs::for_mesh(mesh);
    for (std::size_t i = 0; i < mesh.cells.size(); ++i) {
        in.fields.pervious_fraction[i] = 0.0;
        in.fields.impervious_fraction[i] = 1.0;
        in.rainfall_rate[i] = 2.0e-3;
    }
    const scau::surface2d::StepConfig config{.dt = 0.5, .h_min = 1.0e-8};
    scau::surface2d::StepDiagnostics diag;
    diag.cell_count = mesh.cells.size();

    scau::surface2d::apply_ground_runoff_stage(state, config, dpm, in, rs, geom, diag);

    double expected_surface = 0.0;
    for (const double a : geom.cell_areas) expected_surface += 2.0e-3 * 0.5 * a;
    EXPECT_NEAR(diag.surface_added_volume, expected_surface, 1.0e-9);
    EXPECT_NEAR(diag.infiltration_volume, 0.0, 1.0e-12);
    for (const auto& c : state.cells) EXPECT_NEAR(c.conserved.h, 1.0 + 1.0e-3, 1.0e-9);
}

TEST(GroundRunoffStage, GroundClosureHolds) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    const auto dpm = scau::surface2d::DpmFields::for_mesh(mesh);
    const auto geom = scau::surface2d::GeometryCache::for_mesh(mesh);
    auto rs = scau::surface2d::RunoffState::for_mesh(mesh);
    auto in = scau::surface2d::RunoffStepInputs::for_mesh(mesh);
    for (std::size_t i = 0; i < mesh.cells.size(); ++i) {
        in.fields.pervious_fraction[i] = 0.5;
        in.fields.impervious_fraction[i] = 0.5;
        in.fields.initial_abstraction_capacity[i] = 1.0e-3;
        in.rainfall_rate[i] = 2.0e-3;
    }
    const scau::surface2d::StepConfig config{.dt = 1.0, .h_min = 1.0e-8};
    scau::surface2d::StepDiagnostics diag;
    diag.cell_count = mesh.cells.size();
    scau::surface2d::apply_ground_runoff_stage(state, config, dpm, in, rs, geom, diag);
    EXPECT_NEAR(diag.rainfall_volume,
                diag.surface_added_volume + diag.infiltration_volume +
                diag.abstraction_volume + diag.depression_storage_delta_volume,
                1.0e-9);
}

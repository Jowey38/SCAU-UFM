#include <stdexcept>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/boundary/conditions.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/geometry/cache.hpp"
#include "surface2d/source_terms/fields.hpp"
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

TEST(RunoffStep, ClosedBoxRainConservesThroughRunoffPath) {
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
        in.fields.impervious_fraction[i] = 1.0;
        in.rainfall_rate[i] = 2.0e-3;
    }
    const scau::surface2d::StepConfig config{.dt = 0.5, .cfl_safety = 0.45, .c_rollback = 100.0, .h_min = 1.0e-8};
    const auto diag = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm, boundary, sources, geom, in, rs);
    ASSERT_FALSE(diag.rollback_required);
    double expected = 0.0;
    for (const double a : geom.cell_areas) expected += 2.0e-3 * 0.5 * a;
    EXPECT_NEAR(diag.surface_added_volume, expected, 1.0e-9);
    EXPECT_NEAR(diag.rainfall_volume, expected, 1.0e-9);
}

TEST(RunoffStep, RollbackLeavesRunoffStateUntouched) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 10.0, 10.0);
    const auto dpm = scau::surface2d::DpmFields::for_mesh(mesh);
    const auto geom = scau::surface2d::GeometryCache::for_mesh(mesh);
    const auto boundary = scau::surface2d::BoundaryConditions::for_mesh(mesh);
    const auto sources = scau::surface2d::SourceTermFields::for_mesh(mesh);
    auto rs = scau::surface2d::RunoffState::for_mesh(mesh);
    for (auto& v : rs.cumulative_infiltration) v = 0.05;
    auto in = scau::surface2d::RunoffStepInputs::for_mesh(mesh);
    for (auto& r : in.rainfall_rate) r = 5.0e-3;
    const scau::surface2d::StepConfig config{.dt = 100.0, .cfl_safety = 0.45, .c_rollback = 1.0e-6, .h_min = 1.0e-8};
    const auto diag = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm, boundary, sources, geom, in, rs);
    ASSERT_TRUE(diag.rollback_required);
    EXPECT_EQ(diag.surface_added_volume, 0.0);
    for (const auto v : rs.cumulative_infiltration) EXPECT_EQ(v, 0.05);
}

TEST(GroundRunoffStage, PondedRemovalUsesPhiTScaledDepthDrop) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 0.1);
    auto dpm = scau::surface2d::DpmFields::for_mesh(mesh);
    dpm.cells[0].phi_t = 0.4;
    const auto geom = scau::surface2d::GeometryCache::for_mesh(mesh);
    auto rs = scau::surface2d::RunoffState::for_mesh(mesh);
    auto in = scau::surface2d::RunoffStepInputs::for_mesh(mesh);

    // only C0 active; 10% pervious, no rain, very high capacity soil
    for (std::size_t i = 0; i < mesh.cells.size(); ++i) {
        in.fields.pervious_fraction[i] = 0.0;
        in.fields.impervious_fraction[i] = 0.0;
        in.rainfall_rate[i] = 0.0;
    }
    in.fields.pervious_fraction[0] = 0.1;
    in.lut.entries[0] = scau::surface2d::SoilParams{.k_s = 1.0, .psi_f = 0.1, .theta_s = 0.4, .theta_i = 0.1};

    const scau::surface2d::StepConfig config{.dt = 1.0, .h_min = 1.0e-8};
    scau::surface2d::StepDiagnostics diag;
    diag.cell_count = mesh.cells.size();

    scau::surface2d::apply_ground_runoff_stage(state, config, dpm, in, rs, geom, diag);

    // For h=0.1, phi_t=0.4, perv=0.1: all ponded supply on the pervious footprint infiltrates.
    // ponded_infiltration_volume = (0.1*0.4) * (0.1*A) = 0.004*A
    const double area0 = geom.cell_areas[0];
    EXPECT_NEAR(diag.ponded_infiltration_volume, 0.004 * area0, 1.0e-9);
    EXPECT_NEAR(state.cells[0].conserved.h, 0.09, 1.0e-9);   // h drops by h*pervious_fraction = 0.01
    EXPECT_GE(state.cells[0].conserved.h, 0.0);
}

TEST(GroundRunoffStage, SurfaceStorageInvariantIncludesPondedInfiltration) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 0.02);
    auto dpm = scau::surface2d::DpmFields::for_mesh(mesh);
    dpm.cells[0].phi_t = 0.5;
    const auto geom = scau::surface2d::GeometryCache::for_mesh(mesh);
    auto rs = scau::surface2d::RunoffState::for_mesh(mesh);
    auto in = scau::surface2d::RunoffStepInputs::for_mesh(mesh);
    for (std::size_t i = 0; i < mesh.cells.size(); ++i) {
        in.fields.pervious_fraction[i] = 0.0;
        in.fields.impervious_fraction[i] = 0.0;
        in.rainfall_rate[i] = 0.0;
    }
    in.fields.pervious_fraction[0] = 1.0;
    in.lut.entries[0] = scau::surface2d::SoilParams{.k_s = 1.0, .psi_f = 0.1, .theta_s = 0.4, .theta_i = 0.1};

    const double before = 0.5 * 0.02 * geom.cell_areas[0];
    const scau::surface2d::StepConfig config{.dt = 1.0, .h_min = 1.0e-8};
    scau::surface2d::StepDiagnostics diag;
    diag.cell_count = mesh.cells.size();
    scau::surface2d::apply_ground_runoff_stage(state, config, dpm, in, rs, geom, diag);
    const double after = 0.5 * state.cells[0].conserved.h * geom.cell_areas[0];

    EXPECT_NEAR(after - before,
                diag.surface_added_volume - diag.ponded_infiltration_volume,
                1.0e-9);
}

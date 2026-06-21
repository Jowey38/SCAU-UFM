#include <stdexcept>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/boundary/conditions.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/geometry/cache.hpp"
#include "surface2d/source_terms/fields.hpp"
#include "surface2d/source_terms/runoff/fields.hpp"
#include "surface2d/source_terms/runoff/roof_step.hpp"
#include "surface2d/source_terms/runoff/step_inputs.hpp"
#include "surface2d/state/state.hpp"
#include "surface2d/time_integration/step.hpp"

namespace {
using scau::surface2d::RoofStepInputs;
}  // namespace

TEST(RoofStepInputs, ForMeshHasNeutralDefaults) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto in = RoofStepInputs::for_mesh(mesh);
    ASSERT_EQ(in.map.swmm_node_index.size(), mesh.cells.size());
    for (std::size_t i = 0; i < mesh.cells.size(); ++i) {
        EXPECT_EQ(in.map.swmm_node_index[i], -1);
    }
    ASSERT_TRUE(static_cast<bool>(in.accept));
}

TEST(RoofStepInputs, NullAcceptFailsClosed) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto in = RoofStepInputs::for_mesh(mesh);
    in.accept = nullptr;
    EXPECT_THROW(scau::surface2d::validate_roof_step_inputs_match_mesh(in, mesh), std::invalid_argument);
}

TEST(RoofRunoffStage, FullAcceptDrainsPendingNoSurface) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    const auto dpm = scau::surface2d::DpmFields::for_mesh(mesh);
    const auto geom = scau::surface2d::GeometryCache::for_mesh(mesh);
    auto rs = scau::surface2d::RunoffState::for_mesh(mesh);
    auto runoff = scau::surface2d::RunoffStepInputs::for_mesh(mesh);
    auto roof = scau::surface2d::RoofStepInputs::for_mesh(mesh);
    for (std::size_t i = 0; i < mesh.cells.size(); ++i) {
        runoff.fields.pervious_fraction[i] = 0.0;
        runoff.fields.roof_fraction[i] = 1.0;
        runoff.fields.roof_storage_capacity[i] = 1.0e-2;
        runoff.rainfall_rate[i] = 2.0e-3;
        roof.map.roof_drain_capacity[i] = 1.0e6;
        roof.map.swmm_node_index[i] = 0;
    }
    roof.accept = [](const scau::surface2d::RoofDrainageIntent& in) {
        return scau::surface2d::RoofDrainageAcceptance{
            .requested_volume = in.requested_volume, .accepted_volume = in.requested_volume,
            .rejected_volume = 0.0, .rejection_reason = scau::surface2d::RoofDrainageRejectionReason::None};
    };
    const scau::surface2d::StepConfig config{.dt = 0.5, .h_min = 1.0e-8};
    scau::surface2d::StepDiagnostics diag;
    diag.cell_count = mesh.cells.size();
    const double h_before = state.cells[0].conserved.h;

    scau::surface2d::apply_roof_runoff_stage(state, config, dpm, runoff, roof, rs, geom, diag);

    double roof_rain = 0.0;
    for (const double a : geom.cell_areas) roof_rain += 2.0e-3 * 0.5 * a;
    EXPECT_NEAR(diag.roof_to_swmm_accepted_volume, roof_rain, 1.0e-9);
    EXPECT_NEAR(diag.roof_overflow_to_surface_volume, 0.0, 1.0e-12);
    EXPECT_NEAR(state.cells[0].conserved.h, h_before, 1.0e-12);
    for (const auto v : rs.roof_pending_volume) EXPECT_NEAR(v, 0.0, 1.0e-12);
}

TEST(RoofRunoffStage, RejectZeroStorageOverflowsToSurface) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    const auto dpm = scau::surface2d::DpmFields::for_mesh(mesh);
    const auto geom = scau::surface2d::GeometryCache::for_mesh(mesh);
    auto rs = scau::surface2d::RunoffState::for_mesh(mesh);
    auto runoff = scau::surface2d::RunoffStepInputs::for_mesh(mesh);
    auto roof = scau::surface2d::RoofStepInputs::for_mesh(mesh);
    for (std::size_t i = 0; i < mesh.cells.size(); ++i) {
        runoff.fields.pervious_fraction[i] = 0.0;
        runoff.fields.roof_fraction[i] = 1.0;
        runoff.fields.roof_storage_capacity[i] = 0.0;          // no pending buffer -> overflow
        runoff.rainfall_rate[i] = 2.0e-3;
        roof.map.roof_drain_capacity[i] = 1.0e6;
        roof.map.swmm_node_index[i] = 0;
        roof.map.roof_overflow_to_surface_cell_index[i] = static_cast<int>(i);
    }
    roof.accept = [](const scau::surface2d::RoofDrainageIntent& in) {
        return scau::surface2d::RoofDrainageAcceptance{
            .requested_volume = in.requested_volume, .accepted_volume = 0.0,
            .rejected_volume = in.requested_volume, .rejection_reason = scau::surface2d::RoofDrainageRejectionReason::NodeSurcharged};
    };
    const scau::surface2d::StepConfig config{.dt = 0.5, .h_min = 1.0e-8};
    scau::surface2d::StepDiagnostics diag;
    diag.cell_count = mesh.cells.size();
    scau::surface2d::apply_roof_runoff_stage(state, config, dpm, runoff, roof, rs, geom, diag);

    double roof_rain = 0.0;
    for (const double a : geom.cell_areas) roof_rain += 2.0e-3 * 0.5 * a;
    EXPECT_NEAR(diag.roof_to_swmm_accepted_volume, 0.0, 1.0e-12);
    EXPECT_NEAR(diag.roof_overflow_to_surface_volume, roof_rain, 1.0e-9);
    for (const auto v : rs.roof_pending_volume) EXPECT_NEAR(v, 0.0, 1.0e-9);
}

TEST(RoofStep, FullStepRoofAcceptedConservesViaAccepted) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    const auto dpm = scau::surface2d::DpmFields::for_mesh(mesh);
    const auto geom = scau::surface2d::GeometryCache::for_mesh(mesh);
    const auto boundary = scau::surface2d::BoundaryConditions::for_mesh(mesh);
    const auto sources = scau::surface2d::SourceTermFields::for_mesh(mesh);
    auto rs = scau::surface2d::RunoffState::for_mesh(mesh);
    auto runoff = scau::surface2d::RunoffStepInputs::for_mesh(mesh);
    auto roof = scau::surface2d::RoofStepInputs::for_mesh(mesh);
    for (std::size_t i = 0; i < mesh.cells.size(); ++i) {
        runoff.fields.pervious_fraction[i] = 0.0;
        runoff.fields.roof_fraction[i] = 1.0;
        runoff.fields.roof_storage_capacity[i] = 1.0e-2;
        runoff.rainfall_rate[i] = 2.0e-3;
        roof.map.roof_drain_capacity[i] = 1.0e6;
        roof.map.swmm_node_index[i] = 0;
    }
    roof.accept = [](const scau::surface2d::RoofDrainageIntent& in) {
        return scau::surface2d::RoofDrainageAcceptance{
            .requested_volume = in.requested_volume, .accepted_volume = in.requested_volume,
            .rejected_volume = 0.0, .rejection_reason = scau::surface2d::RoofDrainageRejectionReason::None};
    };
    const scau::surface2d::StepConfig config{.dt = 0.5, .cfl_safety = 0.45, .c_rollback = 100.0, .h_min = 1.0e-8};
    const auto diag = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm, boundary, sources, geom, runoff, roof, rs);
    ASSERT_FALSE(diag.rollback_required);
    double roof_rain = 0.0;
    for (const double a : geom.cell_areas) roof_rain += 2.0e-3 * 0.5 * a;
    EXPECT_NEAR(diag.roof_to_swmm_accepted_volume, roof_rain, 1.0e-9);
    EXPECT_NEAR(diag.roof_overflow_to_surface_volume, 0.0, 1.0e-12);
}

TEST(RoofStep, RollbackLeavesRoofStateUntouched) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 10.0, 10.0);
    const auto dpm = scau::surface2d::DpmFields::for_mesh(mesh);
    const auto geom = scau::surface2d::GeometryCache::for_mesh(mesh);
    const auto boundary = scau::surface2d::BoundaryConditions::for_mesh(mesh);
    const auto sources = scau::surface2d::SourceTermFields::for_mesh(mesh);
    auto rs = scau::surface2d::RunoffState::for_mesh(mesh);
    for (auto& v : rs.roof_pending_volume) v = 0.07;
    auto runoff = scau::surface2d::RunoffStepInputs::for_mesh(mesh);
    auto roof = scau::surface2d::RoofStepInputs::for_mesh(mesh);
    for (std::size_t i = 0; i < mesh.cells.size(); ++i) {
        runoff.fields.roof_fraction[i] = 1.0;
        runoff.fields.pervious_fraction[i] = 0.0;
        runoff.rainfall_rate[i] = 5.0e-3;
        roof.map.swmm_node_index[i] = 0;
    }
    const scau::surface2d::StepConfig config{.dt = 100.0, .cfl_safety = 0.45, .c_rollback = 1.0e-6, .h_min = 1.0e-8};
    const auto diag = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm, boundary, sources, geom, runoff, roof, rs);
    ASSERT_TRUE(diag.rollback_required);
    EXPECT_EQ(diag.roof_to_swmm_accepted_volume, 0.0);
    for (const auto v : rs.roof_pending_volume) EXPECT_EQ(v, 0.07);  // untouched on rollback
}

#include <cstddef>
#include <stdexcept>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/boundary/conditions.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/geometry/cache.hpp"
#include "surface2d/state/state.hpp"
#include "surface2d/time_integration/step.hpp"

namespace {

using scau::mesh::build_mixed_minimal_mesh;
using scau::surface2d::BoundaryConditions;
using scau::surface2d::BoundaryKind;
using scau::surface2d::DpmFields;
using scau::surface2d::GeometryCache;
using scau::surface2d::StepConfig;
using scau::surface2d::SurfaceState;

std::size_t first_boundary_edge_index(const scau::mesh::Mesh& mesh) {
    for (std::size_t index = 0; index < mesh.edges.size(); ++index) {
        if (mesh.edges[index].left_cell.has_value() != mesh.edges[index].right_cell.has_value()) {
            return index;
        }
    }
    throw std::invalid_argument("mesh must contain a boundary edge");
}

double total_water_volume(const scau::mesh::Mesh& mesh, const SurfaceState& state) {
    const auto geometry = GeometryCache::for_mesh(mesh);
    double total = 0.0;
    for (std::size_t i = 0; i < state.cells.size(); ++i) {
        total += state.cells[i].conserved.h * geometry.cell_areas[i];
    }
    return total;
}

StepConfig quiet_config() {
    return StepConfig{.dt = 0.05, .cfl_safety = 0.45, .c_rollback = 10.0};
}

TEST(BoundaryDischargeInflow, ZeroDischargeKeepsLakeAtRestBitwise) {
    const auto mesh = build_mixed_minimal_mesh();
    const auto edge_index = first_boundary_edge_index(mesh);

    auto state = SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    const auto dpm_fields = DpmFields::for_mesh(mesh);
    auto boundary = BoundaryConditions::for_mesh(mesh);
    boundary.edges[edge_index] = BoundaryKind::DischargeInflow;

    const auto diagnostics =
        scau::surface2d::advance_one_step_cpu(mesh, state, quiet_config(), dpm_fields, boundary);

    EXPECT_FALSE(diagnostics.rollback_required);
    EXPECT_EQ(diagnostics.boundary_inflow_volume, 0.0);
    for (const auto& cell : state.cells) {
        EXPECT_EQ(cell.conserved.h, 1.0);
        EXPECT_EQ(cell.conserved.hu, 0.0);
        EXPECT_EQ(cell.conserved.hv, 0.0);
    }
}

TEST(BoundaryDischargeInflow, PrescribedDischargeAddsExactVolume) {
    const auto mesh = build_mixed_minimal_mesh();
    const auto edge_index = first_boundary_edge_index(mesh);

    auto state = SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    const auto dpm_fields = DpmFields::for_mesh(mesh);
    auto boundary = BoundaryConditions::for_mesh(mesh);
    boundary.edges[edge_index] = BoundaryKind::DischargeInflow;
    boundary.discharge_per_width.assign(mesh.edges.size(), 0.0);
    boundary.discharge_per_width[edge_index] = 0.2;

    const auto config = quiet_config();
    const double edge_length = mesh.edges[edge_index].length;
    const double before = total_water_volume(mesh, state);

    double expected_gain = 0.0;
    for (int step = 0; step < 5; ++step) {
        const auto diagnostics =
            scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields, boundary);
        ASSERT_FALSE(diagnostics.rollback_required);
        EXPECT_NEAR(diagnostics.boundary_inflow_volume, 0.2 * edge_length * config.dt, 1.0e-15);
        expected_gain += diagnostics.boundary_inflow_volume;
    }

    const double after = total_water_volume(mesh, state);
    EXPECT_NEAR(after - before, expected_gain, 1.0e-12);
    EXPECT_NEAR(after - before, 5.0 * 0.2 * edge_length * config.dt, 1.0e-12);
}

TEST(BoundaryDischargeInflow, RejectsNegativeOrNonFiniteDischarge) {
    const auto mesh = build_mixed_minimal_mesh();
    const auto edge_index = first_boundary_edge_index(mesh);

    auto state = SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    const auto dpm_fields = DpmFields::for_mesh(mesh);
    auto boundary = BoundaryConditions::for_mesh(mesh);
    boundary.edges[edge_index] = BoundaryKind::DischargeInflow;
    boundary.discharge_per_width.assign(mesh.edges.size(), 0.0);
    boundary.discharge_per_width[edge_index] = -0.1;

    EXPECT_THROW(
        static_cast<void>(scau::surface2d::advance_one_step_cpu(
            mesh, state, quiet_config(), dpm_fields, boundary)),
        std::invalid_argument);
}

TEST(BoundaryWaterLevel, MatchingStageKeepsLakeAtRest) {
    const auto mesh = build_mixed_minimal_mesh();
    const auto edge_index = first_boundary_edge_index(mesh);

    auto state = SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    const auto dpm_fields = DpmFields::for_mesh(mesh);
    auto boundary = BoundaryConditions::for_mesh(mesh);
    boundary.edges[edge_index] = BoundaryKind::WaterLevel;
    boundary.water_level.assign(mesh.edges.size(), 1.0);

    const auto diagnostics =
        scau::surface2d::advance_one_step_cpu(mesh, state, quiet_config(), dpm_fields, boundary);

    EXPECT_FALSE(diagnostics.rollback_required);
    EXPECT_EQ(diagnostics.edges[edge_index].mass_flux, 0.0);
    for (const auto& cell : state.cells) {
        EXPECT_NEAR(cell.conserved.h, 1.0, 1.0e-12);
        EXPECT_NEAR(cell.conserved.hu, 0.0, 1.0e-12);
        EXPECT_NEAR(cell.conserved.hv, 0.0, 1.0e-12);
    }
}

TEST(BoundaryWaterLevel, HigherStageDrivesInflow) {
    const auto mesh = build_mixed_minimal_mesh();
    const auto edge_index = first_boundary_edge_index(mesh);

    auto state = SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    const auto dpm_fields = DpmFields::for_mesh(mesh);
    auto boundary = BoundaryConditions::for_mesh(mesh);
    boundary.edges[edge_index] = BoundaryKind::WaterLevel;
    boundary.water_level.assign(mesh.edges.size(), 1.5);

    const double before = total_water_volume(mesh, state);
    const auto diagnostics =
        scau::surface2d::advance_one_step_cpu(mesh, state, quiet_config(), dpm_fields, boundary);
    const double after = total_water_volume(mesh, state);

    ASSERT_FALSE(diagnostics.rollback_required);
    EXPECT_GT(after, before);
}

TEST(BoundaryWaterLevel, LowerStageDrivesOutflow) {
    const auto mesh = build_mixed_minimal_mesh();
    const auto edge_index = first_boundary_edge_index(mesh);

    auto state = SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    const auto dpm_fields = DpmFields::for_mesh(mesh);
    auto boundary = BoundaryConditions::for_mesh(mesh);
    boundary.edges[edge_index] = BoundaryKind::WaterLevel;
    boundary.water_level.assign(mesh.edges.size(), 0.5);

    const double before = total_water_volume(mesh, state);
    const auto diagnostics =
        scau::surface2d::advance_one_step_cpu(mesh, state, quiet_config(), dpm_fields, boundary);
    const double after = total_water_volume(mesh, state);

    ASSERT_FALSE(diagnostics.rollback_required);
    EXPECT_LT(after, before);
}

TEST(BoundaryWaterLevel, RequiresStageVectorAndFiniteValues) {
    const auto mesh = build_mixed_minimal_mesh();
    const auto edge_index = first_boundary_edge_index(mesh);

    auto state = SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    const auto dpm_fields = DpmFields::for_mesh(mesh);
    auto boundary = BoundaryConditions::for_mesh(mesh);
    boundary.edges[edge_index] = BoundaryKind::WaterLevel;

    EXPECT_THROW(
        static_cast<void>(scau::surface2d::advance_one_step_cpu(
            mesh, state, quiet_config(), dpm_fields, boundary)),
        std::invalid_argument);
}

TEST(BoundaryConditionsValidation, RejectsMismatchedPrescribedVectors) {
    const auto mesh = build_mixed_minimal_mesh();
    auto boundary = BoundaryConditions::for_mesh(mesh);
    boundary.discharge_per_width.assign(mesh.edges.size() + 1, 0.0);
    EXPECT_THROW(
        scau::surface2d::validate_boundary_conditions_match_mesh(boundary, mesh),
        std::invalid_argument);

    auto stage_mismatch = BoundaryConditions::for_mesh(mesh);
    stage_mismatch.water_level.assign(1, 1.0);
    EXPECT_THROW(
        scau::surface2d::validate_boundary_conditions_match_mesh(stage_mismatch, mesh),
        std::invalid_argument);
}

}  // namespace

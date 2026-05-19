#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/boundary/conditions.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/riemann/hllc.hpp"
#include "surface2d/state/state.hpp"
#include "surface2d/time_integration/step.hpp"

namespace {

std::unordered_map<std::string, std::size_t> cell_indices_by_id(const scau::mesh::Mesh& mesh) {
    std::unordered_map<std::string, std::size_t> indices;
    for (std::size_t index = 0; index < mesh.cells.size(); ++index) {
        indices.emplace(mesh.cells[index].id, index);
    }
    return indices;
}

std::size_t edge_cell_index(
    const std::unordered_map<std::string, std::size_t>& cell_indices,
    const std::optional<std::string>& primary_cell,
    const std::optional<std::string>& fallback_cell) {
    const auto& cell_id = primary_cell.has_value() ? *primary_cell : *fallback_cell;
    return cell_indices.at(cell_id);
}

std::vector<double> cell_areas(const scau::mesh::Mesh& mesh) {
    const auto nodes = scau::mesh::node_lookup(mesh.nodes);
    std::vector<double> areas(mesh.cells.size(), 0.0);
    for (std::size_t cell_index = 0; cell_index < mesh.cells.size(); ++cell_index) {
        areas[cell_index] = scau::mesh::cell_area(mesh.cells[cell_index], nodes);
    }
    return areas;
}

double expected_edge_hllc_cfl(
    const scau::mesh::Mesh& mesh,
    const scau::surface2d::SurfaceState& state,
    const scau::surface2d::StepConfig& config,
    const scau::surface2d::BoundaryConditions& boundary) {
    const auto cell_indices = cell_indices_by_id(mesh);
    const auto areas = cell_areas(mesh);
    std::vector<double> cell_cfl(mesh.cells.size(), 0.0);

    for (std::size_t edge_index = 0; edge_index < mesh.edges.size(); ++edge_index) {
        const auto& edge = mesh.edges[edge_index];
        const bool is_internal = edge.left_cell.has_value() && edge.right_cell.has_value();
        if (!is_internal && boundary.edges[edge_index] == scau::surface2d::BoundaryKind::Wall) {
            continue;
        }

        const std::size_t left_index = edge_cell_index(cell_indices, edge.left_cell, edge.right_cell);
        const std::size_t right_index = edge_cell_index(cell_indices, edge.right_cell, edge.left_cell);
        const auto speeds = scau::surface2d::estimate_hllc_wave_speeds(
            state.cells[left_index],
            state.cells[right_index],
            scau::surface2d::Normal2{.x = edge.normal.x, .y = edge.normal.y});
        const double spectral_radius = std::max(std::abs(speeds.s_l), std::abs(speeds.s_r));

        if (edge.left_cell.has_value() && areas[left_index] > 0.0) {
            cell_cfl[left_index] += config.dt * edge.length * spectral_radius / areas[left_index];
        }
        if (edge.right_cell.has_value() && areas[right_index] > 0.0) {
            cell_cfl[right_index] += config.dt * edge.length * spectral_radius / areas[right_index];
        }
    }

    return *std::max_element(cell_cfl.begin(), cell_cfl.end());
}

}  // namespace

TEST(SurfaceCflRollback, ReportsRawPhysicalCflWithoutSafetyScaling) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    state.cells[0].conserved.hu = 2.0;
    const auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    const auto boundary = scau::surface2d::BoundaryConditions::for_mesh(mesh);

    const scau::surface2d::StepConfig config{.dt = 0.5, .cfl_safety = 0.25, .c_rollback = 10.0};
    const double expected_cfl = expected_edge_hllc_cfl(mesh, state, config, boundary);
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields, boundary);

    EXPECT_DOUBLE_EQ(diagnostics.max_cell_cfl, expected_cfl);
    EXPECT_FALSE(diagnostics.rollback_required);
}

TEST(SurfaceCflRollback, ComparesRollbackAgainstCRollbackOnly) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    state.cells[0].conserved.hu = 2.0;
    const auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    const auto boundary = scau::surface2d::BoundaryConditions::for_mesh(mesh);

    const scau::surface2d::StepConfig config{.dt = 0.5, .cfl_safety = 0.25, .c_rollback = 0.75};
    const double expected_cfl = expected_edge_hllc_cfl(mesh, state, config, boundary);
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields, boundary);

    EXPECT_DOUBLE_EQ(diagnostics.max_cell_cfl, expected_cfl);
    EXPECT_TRUE(diagnostics.rollback_required);
}

TEST(SurfaceCflRollback, StaticWaterReportsGravityWaveCfl) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    const auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    const auto boundary = scau::surface2d::BoundaryConditions::for_mesh(mesh);

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 10.0};
    const double expected_cfl = expected_edge_hllc_cfl(mesh, state, config, boundary);
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields, boundary);

    EXPECT_GT(diagnostics.max_cell_cfl, 0.0);
    EXPECT_DOUBLE_EQ(diagnostics.max_cell_cfl, expected_cfl);
    EXPECT_FALSE(diagnostics.rollback_required);
}

TEST(SurfaceCflRollback, EdgeLengthContributesToRawCfl) {
    auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto longer_edge_mesh = mesh;
    longer_edge_mesh.edges[1].length *= 2.0;

    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    auto longer_edge_state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(longer_edge_mesh, 1.0, 1.0);
    const auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    const auto longer_edge_dpm_fields = scau::surface2d::DpmFields::for_mesh(longer_edge_mesh);
    const auto boundary = scau::surface2d::BoundaryConditions::for_mesh(mesh);
    const auto longer_edge_boundary = scau::surface2d::BoundaryConditions::for_mesh(longer_edge_mesh);

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 10.0};
    const double expected_longer_cfl =
        expected_edge_hllc_cfl(longer_edge_mesh, longer_edge_state, config, longer_edge_boundary);
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields, boundary);
    const auto longer_edge_diagnostics = scau::surface2d::advance_one_step_cpu(
        longer_edge_mesh,
        longer_edge_state,
        config,
        longer_edge_dpm_fields,
        longer_edge_boundary);

    EXPECT_GT(longer_edge_diagnostics.max_cell_cfl, diagnostics.max_cell_cfl);
    EXPECT_DOUBLE_EQ(longer_edge_diagnostics.max_cell_cfl, expected_longer_cfl);
}

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <string>

#include <gtest/gtest.h>

#include "stcf/io_netcdf.hpp"
#include "surface2d/boundary/conditions.hpp"
#include "surface2d/geometry/cache.hpp"
#include "surface2d/state/state.hpp"
#include "surface2d/stcf_bridge/load_case.hpp"
#include "surface2d/time_integration/step.hpp"

namespace {

std::string environment_value(const char* name) {
#ifdef _WIN32
    char* value = nullptr;
    std::size_t size = 0;
    if (::_dupenv_s(&value, &size, name) != 0 || value == nullptr) {
        return {};
    }
    std::string result(value);
    std::free(value);
    return result;
#else
    const char* value = std::getenv(name);
    return value == nullptr ? std::string{} : std::string(value);
#endif
}

std::filesystem::path case_path(const char* variable) {
    const std::string value = environment_value(variable);
    if (value.empty()) {
        throw std::runtime_error(std::string("missing environment variable: ") + variable);
    }
    return value;
}

scau::surface2d::SurfaceState lake_at_rest(
    const scau::surface2d::LoadedSurface2DCase& loaded,
    double eta) {
    auto state = scau::surface2d::SurfaceState::for_mesh(loaded.mesh);
    for (std::size_t cell = 0; cell < state.cells.size(); ++cell) {
        state.cells[cell].conserved.h = eta - loaded.bed_elevations[cell];
        state.cells[cell].eta = eta;
    }
    return state;
}

double total_water_volume(
    const scau::surface2d::GeometryCache& geometry,
    const scau::surface2d::SurfaceState& state) {
    double total = 0.0;
    for (std::size_t cell = 0; cell < state.cells.size(); ++cell) {
        total += state.cells[cell].conserved.h * geometry.cell_areas[cell];
    }
    return total;
}

std::size_t first_boundary_edge(const scau::mesh::Mesh& mesh) {
    for (std::size_t edge = 0; edge < mesh.edges.size(); ++edge) {
        if (!mesh.edges[edge].right_cell.has_value()) {
            return edge;
        }
    }
    throw std::runtime_error("generated case has no boundary edge");
}

TEST(GoldenPreProcMixedMeshCase, CliGeneratedCasesAreLogicallyDeterministic) {
    const auto first = scau::stcf::read_stcf_case(case_path("SCAU_PREPROC_CASE_A"));
    const auto second = scau::stcf::read_stcf_case(case_path("SCAU_PREPROC_CASE_B"));

    EXPECT_EQ(first.topology.node_x, second.topology.node_x);
    EXPECT_EQ(first.topology.node_y, second.topology.node_y);
    EXPECT_EQ(first.topology.face_nodes, second.topology.face_nodes);
    EXPECT_EQ(first.topology.edge_nodes, second.topology.edge_nodes);
    EXPECT_EQ(first.topology.edge_faces, second.topology.edge_faces);
    EXPECT_EQ(first.topology.face_edges, second.topology.face_edges);
    EXPECT_EQ(first.fields.cells.phi_t, second.fields.cells.phi_t);
    EXPECT_EQ(first.fields.cells.phi_xx, second.fields.cells.phi_xx);
    EXPECT_EQ(first.fields.cells.phi_xy, second.fields.cells.phi_xy);
    EXPECT_EQ(first.fields.cells.phi_yy, second.fields.cells.phi_yy);
    EXPECT_EQ(first.fields.cells.manning_n, second.fields.cells.manning_n);
    EXPECT_EQ(first.fields.cells.z_b, second.fields.cells.z_b);
    EXPECT_EQ(first.fields.cells.soil_type, second.fields.cells.soil_type);
    EXPECT_EQ(first.fields.edges.omega_edge, second.fields.edges.omega_edge);
    EXPECT_EQ(first.fields.edges.phi_e_n, second.fields.edges.phi_e_n);
    EXPECT_EQ(first.fields.edges.phi_et, second.fields.edges.phi_et);
}

TEST(GoldenPreProcMixedMeshCase, StrictFileLoadsMixedMeshAndProfileFields) {
    const auto loaded = scau::surface2d::load_surface2d_case(
        case_path("SCAU_PREPROC_CASE_A"));

    ASSERT_EQ(loaded.mesh.nodes.size(), 5U);
    ASSERT_EQ(loaded.mesh.cells.size(), 2U);
    ASSERT_EQ(loaded.mesh.edges.size(), 6U);
    EXPECT_EQ(loaded.mesh.cells[0].cell_type, scau::mesh::CellType::Quadrilateral);
    EXPECT_EQ(loaded.mesh.cells[1].cell_type, scau::mesh::CellType::Triangle);
    EXPECT_EQ(loaded.dpm_fields.cells[0].phi_t, 1.0);
    EXPECT_EQ(loaded.dpm_fields.cells[1].phi_t, 0.8);
    EXPECT_EQ(loaded.source_fields.manning_n[0], 0.02);
    EXPECT_EQ(loaded.source_fields.manning_n[1], 0.03);
    EXPECT_EQ(loaded.bed_elevations[0], 0.0);
    EXPECT_EQ(loaded.bed_elevations[1], 0.1);
}

TEST(GoldenPreProcMixedMeshCase, FileDrivenSlopingBedLakeAtRestStaysBalanced) {
    const auto loaded = scau::surface2d::load_surface2d_case(
        case_path("SCAU_PREPROC_CASE_A"));
    auto state = lake_at_rest(loaded, 1.0);
    const auto initial = state;
    const auto boundary = scau::surface2d::BoundaryConditions::for_mesh(loaded.mesh);
    const auto geometry = scau::surface2d::GeometryCache::for_mesh(loaded.mesh);
    const scau::surface2d::StepConfig config{
        .dt = 0.01,
        .cfl_safety = 0.45,
        .c_rollback = 10.0,
    };

    for (int step = 0; step < 20; ++step) {
        const auto diagnostics = scau::surface2d::advance_one_step_cpu(
            loaded.mesh,
            state,
            config,
            loaded.dpm_fields,
            boundary,
            loaded.source_fields,
            geometry);
        ASSERT_FALSE(diagnostics.rollback_required);
    }

    for (std::size_t cell = 0; cell < state.cells.size(); ++cell) {
        EXPECT_NEAR(state.cells[cell].conserved.h, initial.cells[cell].conserved.h, 1.0e-12);
        EXPECT_NEAR(state.cells[cell].conserved.hu, 0.0, 1.0e-12);
        EXPECT_NEAR(state.cells[cell].conserved.hv, 0.0, 1.0e-12);
    }
}

TEST(GoldenPreProcMixedMeshCase, FileDrivenBoundaryInflowAuditIsExact) {
    const auto loaded = scau::surface2d::load_surface2d_case(
        case_path("SCAU_PREPROC_CASE_A"));
    auto state = lake_at_rest(loaded, 1.0);
    const auto geometry = scau::surface2d::GeometryCache::for_mesh(loaded.mesh);
    auto boundary = scau::surface2d::BoundaryConditions::for_mesh(loaded.mesh);
    const std::size_t edge = first_boundary_edge(loaded.mesh);
    boundary.edges[edge] = scau::surface2d::BoundaryKind::DischargeInflow;
    boundary.discharge_per_width.assign(loaded.mesh.edges.size(), 0.0);
    boundary.discharge_per_width[edge] = 0.15;
    const scau::surface2d::StepConfig config{
        .dt = 0.01,
        .cfl_safety = 0.45,
        .c_rollback = 10.0,
    };
    const double before = total_water_volume(geometry, state);
    double audited = 0.0;

    constexpr int kSteps = 10;
    for (int step = 0; step < kSteps; ++step) {
        const auto diagnostics = scau::surface2d::advance_one_step_cpu(
            loaded.mesh,
            state,
            config,
            loaded.dpm_fields,
            boundary,
            loaded.source_fields,
            geometry);
        ASSERT_FALSE(diagnostics.rollback_required);
        audited += diagnostics.boundary_inflow_volume;
    }

    const double expected = 0.15 * loaded.mesh.edges[edge].length * config.dt * kSteps;
    const double after = total_water_volume(geometry, state);
    EXPECT_NEAR(audited, expected, 1.0e-12);
    EXPECT_NEAR(after - before, expected, 1.0e-12);
}

}  // namespace

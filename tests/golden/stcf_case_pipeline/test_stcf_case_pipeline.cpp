// G21 stcf_case_pipeline: locks the full STCF data path
//   authored dataset -> write_stcf -> read_stcf -> stcf_bridge assembly ->
//   advance_one_step_cpu
// on the mixed minimal mesh with a sloping bed, spatially varying phi_t and
// Manning roughness carried by the file (not by programmatic fixtures).
//
// Assertions:
//   1. Sloping-bed, varying-phi_t lake at rest stays at rest (1e-12) when all
//      fields arrive through the NetCDF file (Audusse + WB pairing survive
//      the I/O round trip).
//   2. DischargeInflow boundary volume audit is exact across steps: total
//      water volume gain == sum(boundary_inflow_volume) == q * L * dt * N.

#include <cmath>
#include <cstddef>
#include <filesystem>
#include <stdexcept>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "stcf/io_netcdf.hpp"
#include "stcf/schema.hpp"
#include "surface2d/boundary/conditions.hpp"
#include "surface2d/geometry/cache.hpp"
#include "surface2d/state/state.hpp"
#include "surface2d/stcf_bridge/assemble.hpp"
#include "surface2d/time_integration/step.hpp"

namespace {

using scau::mesh::build_mixed_minimal_mesh;
using scau::stcf::make_uniform_dataset;
using scau::stcf::read_stcf;
using scau::stcf::StcfDataset;
using scau::stcf::write_stcf;
using scau::surface2d::assemble_bed_elevations;
using scau::surface2d::assemble_dpm_fields;
using scau::surface2d::assemble_source_term_fields;
using scau::surface2d::BoundaryConditions;
using scau::surface2d::BoundaryKind;
using scau::surface2d::GeometryCache;
using scau::surface2d::SourceTermFields;
using scau::surface2d::StepConfig;
using scau::surface2d::SurfaceState;

constexpr double kRestTolerance = 1.0e-12;

// Authored deterministic case: sloping bed, varying phi_t (with the SPD /
// phi_t >= max-diag closure respected), non-zero Manning roughness.
StcfDataset authored_case(const scau::mesh::Mesh& mesh) {
    auto dataset = make_uniform_dataset(mesh.cells.size(), mesh.edges.size());
    for (std::size_t i = 0; i < mesh.cells.size(); ++i) {
        const double f = static_cast<double>(i);
        dataset.cells.z_b[i] = 0.05 + 0.1 * f;
        dataset.cells.phi_t[i] = 1.0 - 0.08 * f;
        dataset.cells.phi_xx[i] = dataset.cells.phi_t[i] - 0.05;
        dataset.cells.phi_xy[i] = 0.0;
        dataset.cells.phi_yy[i] = dataset.cells.phi_t[i] - 0.1;
        dataset.cells.manning_n[i] = 0.02;
    }
    return dataset;
}

std::filesystem::path write_authored_case(const scau::mesh::Mesh& mesh, const char* file_name) {
    const auto path = std::filesystem::path(::testing::TempDir()) / file_name;
    write_stcf(path, authored_case(mesh));
    return path;
}

SurfaceState lake_at_rest_state(
    const scau::mesh::Mesh& mesh,
    const std::vector<scau::core::Real>& z_b,
    double eta) {
    auto state = SurfaceState::for_mesh(mesh);
    for (std::size_t i = 0; i < mesh.cells.size(); ++i) {
        const double depth = eta - z_b[i];
        if (depth <= 0.0) {
            throw std::invalid_argument("authored case must keep every cell wet");
        }
        state.cells[i].conserved.h = depth;
        state.cells[i].eta = eta;
    }
    return state;
}

double total_water_volume(const GeometryCache& geometry, const SurfaceState& state) {
    double total = 0.0;
    for (std::size_t i = 0; i < state.cells.size(); ++i) {
        total += state.cells[i].conserved.h * geometry.cell_areas[i];
    }
    return total;
}

std::size_t first_boundary_edge_index(const scau::mesh::Mesh& mesh) {
    for (std::size_t index = 0; index < mesh.edges.size(); ++index) {
        if (mesh.edges[index].left_cell.has_value() != mesh.edges[index].right_cell.has_value()) {
            return index;
        }
    }
    throw std::invalid_argument("mesh must contain a boundary edge");
}

TEST(GoldenStcfCasePipeline, FileCarriedSlopingBedLakeAtRestStaysAtRest) {
    const auto mesh = build_mixed_minimal_mesh();
    const auto path = write_authored_case(mesh, "g21_lake_at_rest.stcf.nc");

    const auto dataset = read_stcf(path);
    const auto dpm_fields = assemble_dpm_fields(mesh, dataset);
    const auto sources = assemble_source_term_fields(mesh, dataset);
    const auto z_b = assemble_bed_elevations(mesh, dataset);
    const auto geometry = GeometryCache::for_mesh(mesh);
    const auto boundary = BoundaryConditions::for_mesh(mesh);

    auto state = lake_at_rest_state(mesh, z_b, 1.0);
    const auto initial = state;

    const StepConfig config{.dt = 0.05, .cfl_safety = 0.45, .c_rollback = 10.0};
    for (int step = 0; step < 20; ++step) {
        const auto diagnostics = scau::surface2d::advance_one_step_cpu(
            mesh, state, config, dpm_fields, boundary, sources, geometry);
        ASSERT_FALSE(diagnostics.rollback_required);
    }

    for (std::size_t i = 0; i < state.cells.size(); ++i) {
        EXPECT_NEAR(state.cells[i].conserved.h, initial.cells[i].conserved.h, kRestTolerance);
        EXPECT_NEAR(state.cells[i].conserved.hu, 0.0, kRestTolerance);
        EXPECT_NEAR(state.cells[i].conserved.hv, 0.0, kRestTolerance);
    }
}

TEST(GoldenStcfCasePipeline, FileCarriedInflowVolumeAuditIsExact) {
    const auto mesh = build_mixed_minimal_mesh();
    const auto path = write_authored_case(mesh, "g21_inflow.stcf.nc");

    const auto dataset = read_stcf(path);
    const auto dpm_fields = assemble_dpm_fields(mesh, dataset);
    const auto sources = assemble_source_term_fields(mesh, dataset);
    const auto z_b = assemble_bed_elevations(mesh, dataset);
    const auto geometry = GeometryCache::for_mesh(mesh);

    const auto edge_index = first_boundary_edge_index(mesh);
    auto boundary = BoundaryConditions::for_mesh(mesh);
    boundary.edges[edge_index] = BoundaryKind::DischargeInflow;
    boundary.discharge_per_width.assign(mesh.edges.size(), 0.0);
    boundary.discharge_per_width[edge_index] = 0.15;

    auto state = lake_at_rest_state(mesh, z_b, 1.0);
    const double before = total_water_volume(geometry, state);

    const StepConfig config{.dt = 0.05, .cfl_safety = 0.45, .c_rollback = 10.0};
    const int steps = 10;
    double audited = 0.0;
    for (int step = 0; step < steps; ++step) {
        const auto diagnostics = scau::surface2d::advance_one_step_cpu(
            mesh, state, config, dpm_fields, boundary, sources, geometry);
        ASSERT_FALSE(diagnostics.rollback_required);
        audited += diagnostics.boundary_inflow_volume;
    }

    const double after = total_water_volume(geometry, state);
    const double expected = 0.15 * mesh.edges[edge_index].length * config.dt * steps;
    EXPECT_NEAR(audited, expected, 1.0e-12);
    EXPECT_NEAR(after - before, audited, 1.0e-12);
}

TEST(GoldenStcfCasePipeline, RoundTripPreservesAuthoredFieldsBitwise) {
    const auto mesh = build_mixed_minimal_mesh();
    const auto authored = authored_case(mesh);
    const auto path = write_authored_case(mesh, "g21_round_trip.stcf.nc");

    const auto loaded = read_stcf(path);
    for (std::size_t i = 0; i < mesh.cells.size(); ++i) {
        EXPECT_EQ(loaded.cells.phi_t[i], authored.cells.phi_t[i]);
        EXPECT_EQ(loaded.cells.z_b[i], authored.cells.z_b[i]);
        EXPECT_EQ(loaded.cells.manning_n[i], authored.cells.manning_n[i]);
    }
}

}  // namespace

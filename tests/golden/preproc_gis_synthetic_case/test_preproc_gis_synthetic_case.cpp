// G30 preproc_gis_synthetic_case: locks the M287-B GIS preprocessing pipeline
// output as a committed deterministic fixture:
//   synthetic D-5 package -> scau_preproc.pipeline (gmsh subprocess, policy
//   fail-closed, bitwise rerun check) -> synthetic_city_block.stcf.nc
// The fixture was certified by `scau_preproc validate` and its SHA-256 is
// recorded in the committed pipeline manifest. This golden strict-loads the
// committed case through the authoritative read path and asserts:
//   1. recorded shape (nodes/faces/edges/soil entries) and field domains;
//   2. mesh quality has no fatal findings;
//   3. a wet lake at rest over the sampled DEM bed stays at rest (1e-12)
//      when every field arrives from the pipeline-generated file.
// Regeneration (gmsh) is NOT run here: hosted CI has no gmsh; determinism is
// enforced pipeline-side and re-checked whenever the fixture is regenerated.

#include <algorithm>
#include <cmath>
#include <filesystem>

#include <gtest/gtest.h>

#include "mesh/quality.hpp"
#include "surface2d/boundary/conditions.hpp"
#include "surface2d/geometry/cache.hpp"
#include "surface2d/state/state.hpp"
#include "surface2d/stcf_bridge/load_case.hpp"
#include "surface2d/time_integration/step.hpp"

namespace {

using scau::surface2d::BoundaryConditions;
using scau::surface2d::GeometryCache;
using scau::surface2d::load_surface2d_case;
using scau::surface2d::StepConfig;
using scau::surface2d::SurfaceState;

constexpr double kRestTolerance = 1.0e-12;

std::filesystem::path fixture_path() {
    return std::filesystem::path(SCAU_PREPROC_GIS_CASE_DIR) / "synthetic_city_block.stcf.nc";
}

TEST(GoldenPreprocGisSyntheticCase, FixtureMatchesRecordedShapeAndFieldDomains) {
    const auto loaded = load_surface2d_case(fixture_path());

    EXPECT_EQ(loaded.mesh.nodes.size(), 526U);
    EXPECT_EQ(loaded.mesh.cells.size(), 469U);
    EXPECT_EQ(loaded.mesh.edges.size(), 996U);
    EXPECT_EQ(loaded.bed_elevations.size(), 469U);

    for (std::size_t i = 0; i < loaded.mesh.cells.size(); ++i) {
        // Pipeline v1 placeholder DPM rules: uniform unit storage/conveyance.
        EXPECT_EQ(loaded.dpm_fields.cells[i].phi_t, 1.0);
        // z_b sampled from the synthetic DEM (10.0 .. 10.9 m).
        EXPECT_GE(loaded.bed_elevations[i], 10.0);
        EXPECT_LE(loaded.bed_elevations[i], 10.9);
        // Manning from the landcover lookup (road/water/grass).
        const double n = loaded.source_fields.manning_n[i];
        EXPECT_TRUE(n == 0.015 || n == 0.020 || n == 0.030);
    }
    for (std::size_t e = 0; e < loaded.mesh.edges.size(); ++e) {
        EXPECT_EQ(loaded.dpm_fields.edges[e].omega_edge, 1.0);
        EXPECT_EQ(loaded.dpm_fields.edges[e].phi_e_n, 1.0);
    }
}

TEST(GoldenPreprocGisSyntheticCase, FixtureMeshHasNoFatalQualityFindings) {
    const auto loaded = load_surface2d_case(fixture_path());
    const auto report = scau::mesh::evaluate_mesh_quality(loaded.mesh);
    EXPECT_FALSE(report.fatal);
    EXPECT_EQ(report.reviewed_cells, loaded.mesh.cells.size());
    // The pipeline quality report recorded min angle 43.64 deg / max edge
    // ratio 2.36, far inside the 10 deg / 20 review thresholds, so the
    // review list must be empty too.
    EXPECT_TRUE(report.issues.empty());
}

TEST(GoldenPreprocGisSyntheticCase, FileCarriedLakeAtRestOverSampledDemStaysAtRest) {
    const auto loaded = load_surface2d_case(fixture_path());
    const auto geometry = GeometryCache::for_mesh(loaded.mesh);
    const auto boundary = BoundaryConditions::for_mesh(loaded.mesh);

    const double eta = 12.0;  // above the DEM maximum (10.9): every cell wet.
    auto state = SurfaceState::for_mesh(loaded.mesh);
    for (std::size_t i = 0; i < loaded.mesh.cells.size(); ++i) {
        state.cells[i].conserved.h = eta - loaded.bed_elevations[i];
        ASSERT_GT(state.cells[i].conserved.h, 0.0);
        state.cells[i].eta = eta;
    }
    const auto initial = state;

    const StepConfig config{.dt = 0.05, .cfl_safety = 0.45, .c_rollback = 10.0};
    for (int step = 0; step < 20; ++step) {
        const auto diagnostics = scau::surface2d::advance_one_step_cpu(
            loaded.mesh, state, config, loaded.dpm_fields, boundary,
            loaded.source_fields, geometry);
        ASSERT_FALSE(diagnostics.rollback_required);
    }

    for (std::size_t i = 0; i < state.cells.size(); ++i) {
        EXPECT_NEAR(state.cells[i].conserved.h, initial.cells[i].conserved.h, kRestTolerance);
        EXPECT_NEAR(state.cells[i].conserved.hu, 0.0, kRestTolerance);
        EXPECT_NEAR(state.cells[i].conserved.hv, 0.0, kRestTolerance);
    }
}

}  // namespace

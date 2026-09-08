// G37 preproc_crs_governance_case: locks the M287-B2 CRS governance stage as a
// committed fixture:
//   synthetic D-5 package (authored in EPSG:3857) + cases/crs_policy_3395.json
//   (target EPSG:3395, allowed source EPSG:3857, dem declare_only)
//   -> governed staging package (vectors + mesh controls reprojected, SWMM
//      [COORDINATES] rewritten, everything else verbatim) -> meshed case
//   synthetic_city_block_epsg3395.stcf.nc
// The golden asserts through the authoritative read path that:
//   1. the mesh lives in the TARGET frame: Mercator compression of the
//      120 m northing to 119.196674 m is visible in the node extent while
//      eastings keep 0..200 (transform really happened, was not identity);
//   2. the rewritten SWMM inlet coordinates (J1 55,9.933056 / J2 135,9.933056)
//      are contained by road cells of the reprojected mesh, i.e. vector and
//      SWMM reprojections are mutually consistent;
//   3. the recorded audit pins the PROJ pipeline string and round-trip
//      errors below 1e-9 m; the case is a valid strict CF/UGRID file with the
//      recorded shape; a wet lake at rest stays at rest to 1e-12.
// Regeneration (gmsh + pyproj) is pipeline-side; determinism (byte-identical
// governed files and case across runs) is evidence on the pipeline side.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

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
constexpr double kExtentTolerance = 1.0e-5;   // coordinates are written with 6 decimals
constexpr double kCompressedNorthing = 119.196674;  // EPSG:3857 y=120 -> EPSG:3395

std::filesystem::path case_dir() {
    return std::filesystem::path(SCAU_PREPROC_CRS_CASE_DIR);
}

std::filesystem::path fixture_path() {
    return case_dir() / "synthetic_city_block_epsg3395.stcf.nc";
}

std::string read_text(const std::filesystem::path& path) {
    std::ifstream stream(path);
    std::stringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

bool contains(const std::string& text, const std::string& needle) {
    return text.find(needle) != std::string::npos;
}

using NodeXY = std::unordered_map<std::string, std::pair<double, double>>;

bool point_in_cell(const scau::mesh::Mesh& mesh, const NodeXY& xy, std::size_t cell, double x, double y) {
    const auto& c = mesh.cells[cell];
    bool inside = false;
    const std::size_t n = c.node_ids.size();
    for (std::size_t i = 0; i < n; ++i) {
        const auto& a = xy.at(c.node_ids[i]);
        const auto& b = xy.at(c.node_ids[(i + 1U) % n]);
        if ((a.second > y) != (b.second > y)) {
            const double x_cross = a.first + (y - a.second) * (b.first - a.first) / (b.second - a.second);
            if (x_cross > x) inside = !inside;
        }
    }
    return inside;
}

TEST(GoldenPreprocCrsGovernanceCase, MeshLivesInTheTargetFrame) {
    const auto loaded = load_surface2d_case(fixture_path());
    EXPECT_EQ(loaded.mesh.nodes.size(), 1116U);
    EXPECT_EQ(loaded.mesh.cells.size(), 1072U);
    EXPECT_EQ(loaded.mesh.edges.size(), 2189U);

    double min_x = 1e300, max_x = -1e300, min_y = 1e300, max_y = -1e300;
    for (const auto& node : loaded.mesh.nodes) {
        min_x = std::min(min_x, node.x);
        max_x = std::max(max_x, node.x);
        min_y = std::min(min_y, node.y);
        max_y = std::max(max_y, node.y);
    }
    EXPECT_NEAR(min_x, 0.0, kExtentTolerance);
    EXPECT_NEAR(max_x, 200.0, kExtentTolerance);
    EXPECT_NEAR(min_y, 0.0, kExtentTolerance);
    // The whole point: not the source 120 m, but the EPSG:3395 northing.
    EXPECT_NEAR(max_y, kCompressedNorthing, kExtentTolerance);
    EXPECT_LT(max_y, 119.5);

    const auto report = scau::mesh::evaluate_mesh_quality(loaded.mesh);
    EXPECT_FALSE(report.fatal);
    EXPECT_TRUE(report.issues.empty());
}

TEST(GoldenPreprocCrsGovernanceCase, RewrittenSwmmInletsLieInsideRoadCells) {
    const auto loaded = load_surface2d_case(fixture_path());
    NodeXY xy;
    for (const auto& node : loaded.mesh.nodes) xy.emplace(node.id, std::make_pair(node.x, node.y));

    // From cases/model_epsg3395.inp [COORDINATES] (rewritten by the governance stage).
    const std::pair<double, double> inlets[] = {{55.0, 9.933056}, {135.0, 9.933056}};
    for (const auto& [x, y] : inlets) {
        std::size_t found = loaded.mesh.cells.size();
        for (std::size_t c = 0; c < loaded.mesh.cells.size(); ++c) {
            if (point_in_cell(loaded.mesh, xy, c, x, y)) { found = c; break; }
        }
        ASSERT_LT(found, loaded.mesh.cells.size()) << "no cell contains inlet (" << x << ", " << y << ")";
        // Road strip (landcover reprojected consistently): Manning 0.015.
        EXPECT_EQ(loaded.source_fields.manning_n[found], 0.015) << "inlet (" << x << ", " << y << ")";
    }
    const auto inp = read_text(case_dir() / "model_epsg3395.inp");
    EXPECT_TRUE(contains(inp, "J1      55.000000       9.933056"));
    EXPECT_TRUE(contains(inp, "J2      135.000000      9.933056"));
    EXPECT_TRUE(contains(inp, "[JUNCTIONS]"));  // non-coordinate sections survive verbatim
    EXPECT_TRUE(contains(inp, "C1      J1    J2  100     0.013"));
}

TEST(GoldenPreprocCrsGovernanceCase, AuditPinsPipelineAndRoundTrip) {
    const auto audit = read_text(case_dir() / "crs_audit.json");
    EXPECT_TRUE(contains(audit, "\"crs_audit_schema_version\": 1"));
    EXPECT_TRUE(contains(audit, "\"target_crs\": \"EPSG:3395\""));
    EXPECT_TRUE(contains(audit, "\"proj_pipeline\": \"proj=pipeline step inv proj=webmerc lat_0=0 lon_0=0 x_0=0 y_0=0 ellps=WGS84 step proj=merc lon_0=0 k=1 x_0=0 y_0=0 ellps=WGS84\""));
    EXPECT_TRUE(contains(audit, "\"action\": \"reprojected\""));
    EXPECT_TRUE(contains(audit, "\"action\": \"coordinates_rewritten\""));
    EXPECT_TRUE(contains(audit, "\"action\": \"declared_mismatch_allowed\""));  // DEM never resampled in B2
    EXPECT_FALSE(contains(audit, "e-0"));  // no round-trip error at or above 1e-9 recorded (values are ~1e-14 or 0)
    const auto manifest = read_text(case_dir() / "synthetic_city_block_epsg3395.pipeline_manifest.json");
    EXPECT_TRUE(contains(manifest, "\"proj_pipelines\""));
    EXPECT_TRUE(contains(manifest, "\"governed_package_sha256\""));
    EXPECT_TRUE(contains(manifest, "\"case_sha256\": \"8a4d72b6068c4678"));
}

TEST(GoldenPreprocCrsGovernanceCase, LakeAtRestStaysAtRest) {
    const auto loaded = load_surface2d_case(fixture_path());
    const auto geometry = GeometryCache::for_mesh(loaded.mesh);
    const auto boundary = BoundaryConditions::for_mesh(loaded.mesh);
    const double eta = 12.0;
    auto state = SurfaceState::for_mesh(loaded.mesh);
    for (std::size_t i = 0; i < loaded.mesh.cells.size(); ++i) {
        state.cells[i].conserved.h = eta - loaded.bed_elevations[i];
        ASSERT_GT(state.cells[i].conserved.h, 0.0);
        state.cells[i].eta = eta;
    }
    const auto initial = state;
    const StepConfig config{.dt = 0.02, .cfl_safety = 0.45, .c_rollback = 10.0};
    for (int step = 0; step < 20; ++step) {
        const auto diagnostics = scau::surface2d::advance_one_step_cpu(
            loaded.mesh, state, config, loaded.dpm_fields, boundary, loaded.source_fields, geometry);
        ASSERT_FALSE(diagnostics.rollback_required);
    }
    for (std::size_t i = 0; i < state.cells.size(); ++i) {
        EXPECT_NEAR(state.cells[i].conserved.h, initial.cells[i].conserved.h, kRestTolerance);
        EXPECT_NEAR(state.cells[i].conserved.hu, 0.0, kRestTolerance);
        EXPECT_NEAR(state.cells[i].conserved.hv, 0.0, kRestTolerance);
    }
}

}  // namespace

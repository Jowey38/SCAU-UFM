// G38 preproc_terrain_condition_case: locks the M287-B3 terrain conditioning
// stage as a committed fixture pair on ONE mesh:
//   synthetic D-5 package with cases/dem_depression.asc (20 x 12 @ 10 m: west
//   -> east slope 10.02..10.78 m, 4 x 4 bowl lowered 0.30 / 0.60 m whose lowest
//   rim exit is the 10.30 m column) + cases/streams.geojson (line along y = 100 m
//   from the west rim to x = 180 m)
//     -> policy_fill (authorized: stream burn 0.20 m, then Priority-Flood)
//        -> synthetic_city_block_conditioned.stcf.nc
//     -> policy_off  (same operations declared, enabled = false)
//        -> synthetic_city_block_raw.stcf.nc (DEM sampled verbatim)
// The golden asserts through the authoritative read path that:
//   1. both twins share the mesh (geometry never depends on the DEM) and every
//      cell whose centroid lies in the bowl has z_b == 10.30 exactly in the
//      conditioned case (fill to the spill elevation, no epsilon) while the raw
//      twin dips to 9.78; the stream row is lower by exactly 0.20 m; every
//      other cell is bit-identical between the twins (the stage touches only
//      what the policy authorizes);
//   2. the report pins algorithm + version, seed / filled counts and the
//      before/after SHA-256; the manifest of the disabled-policy run on the
//      unmodified sample records the G30 case SHA (policy off = bitwise no-op);
//   3. the conditioned case is a valid strict CF/UGRID file; a wet lake at rest
//      stays at rest to 1e-12 on the conditioned bed.
// Regeneration: py -3 tests/golden/preproc_terrain_condition_case/cases/regenerate.py --run

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

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
constexpr double kSpillElevation = 10.30;   // lowest rim exit of the authored bowl (x = 75 m column)
constexpr double kBurnDepth = 0.20;

std::filesystem::path case_dir() {
    return std::filesystem::path(SCAU_PREPROC_TERRAIN_CASE_DIR);
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

struct Centroids {
    std::vector<double> x;
    std::vector<double> y;
};

Centroids centroids_of(const scau::mesh::Mesh& mesh) {
    std::unordered_map<std::string, std::pair<double, double>> xy;
    for (const auto& node : mesh.nodes) xy.emplace(node.id, std::make_pair(node.x, node.y));
    Centroids c;
    for (const auto& cell : mesh.cells) {
        double sx = 0.0, sy = 0.0;
        for (const auto& id : cell.node_ids) {
            sx += xy.at(id).first;
            sy += xy.at(id).second;
        }
        c.x.push_back(sx / static_cast<double>(cell.node_ids.size()));
        c.y.push_back(sy / static_cast<double>(cell.node_ids.size()));
    }
    return c;
}

bool in_bowl(double x, double y) { return x >= 80.0 && x < 120.0 && y >= 40.0 && y < 80.0; }
// Stream cells: DEM row y in [100, 110) for columns x < 190 (the line ends at x = 180 -> column 18).
bool on_stream(double x, double y) { return y >= 100.0 && y < 110.0 && x < 190.0; }

TEST(GoldenPreprocTerrainConditionCase, ConditioningTouchesOnlyAuthorizedCells) {
    const auto conditioned = load_surface2d_case(case_dir() / "synthetic_city_block_conditioned.stcf.nc");
    const auto raw = load_surface2d_case(case_dir() / "synthetic_city_block_raw.stcf.nc");
    ASSERT_EQ(conditioned.mesh.nodes.size(), 526U);
    ASSERT_EQ(conditioned.mesh.cells.size(), 469U);
    ASSERT_EQ(conditioned.mesh.edges.size(), 996U);
    ASSERT_EQ(raw.mesh.cells.size(), conditioned.mesh.cells.size());

    const auto c = centroids_of(conditioned.mesh);
    const auto r = centroids_of(raw.mesh);
    std::size_t bowl = 0, stream = 0, untouched = 0;
    double raw_bowl_min = 1e300;
    for (std::size_t i = 0; i < c.x.size(); ++i) {
        ASSERT_EQ(c.x[i], r.x[i]);   // same mesh, same cell order
        ASSERT_EQ(c.y[i], r.y[i]);
        const double zc = conditioned.bed_elevations[i];
        const double zr = raw.bed_elevations[i];
        if (in_bowl(c.x[i], c.y[i])) {
            ++bowl;
            EXPECT_EQ(zc, kSpillElevation) << "bowl cell " << i;   // exact: fill = spill grid value
            raw_bowl_min = std::min(raw_bowl_min, zr);
            EXPECT_LT(zr, kSpillElevation) << "raw bowl cell " << i;
        } else if (on_stream(c.x[i], c.y[i])) {
            ++stream;
            EXPECT_NEAR(zc, zr - kBurnDepth, 1.0e-12) << "stream cell " << i;
        } else {
            ++untouched;
            EXPECT_EQ(zc, zr) << "cell " << i << " outside bowl/stream changed";
        }
        // Everything that is not z_b is untouched by the stage.
        EXPECT_EQ(conditioned.source_fields.manning_n[i], raw.source_fields.manning_n[i]);
        EXPECT_EQ(conditioned.dpm_fields.cells[i].phi_t, raw.dpm_fields.cells[i].phi_t);
    }
    EXPECT_EQ(bowl, 39U);
    EXPECT_GT(stream, 20U);
    EXPECT_GT(untouched, 350U);
    EXPECT_NEAR(raw_bowl_min, 9.78, 1.0e-12);

    const auto report = scau::mesh::evaluate_mesh_quality(conditioned.mesh);
    EXPECT_FALSE(report.fatal);
    EXPECT_TRUE(report.issues.empty());
}

TEST(GoldenPreprocTerrainConditionCase, ReportPinsAlgorithmAndDisabledPolicyIsBitwiseNoOp) {
    const auto report = read_text(case_dir() / "terrain_condition_report.json");
    EXPECT_TRUE(contains(report, "\"terrain_report_schema_version\": 1"));
    EXPECT_TRUE(contains(report, "\"enabled\": true"));
    EXPECT_TRUE(contains(report, "\"active_operations\": [\n    \"stream_enforcement\",\n    \"fill_depressions\"\n  ]"));
    EXPECT_TRUE(contains(report, "\"algorithm\": \"priority_flood\""));
    EXPECT_TRUE(contains(report, "\"algorithm_version\": 1"));
    EXPECT_TRUE(contains(report, "\"epsilon_m\": 0.0"));
    EXPECT_TRUE(contains(report, "\"connectivity\": 8"));
    EXPECT_TRUE(contains(report, "\"seed_cells\": 60"));
    EXPECT_TRUE(contains(report, "\"cells_filled\": 16"));
    EXPECT_TRUE(contains(report, "\"cells_burned\": 19"));
    EXPECT_TRUE(contains(report, "\"burn_depth_m\": 0.2"));
    EXPECT_TRUE(contains(report, "\"max_fill_depth_m\": 0.52"));
    EXPECT_TRUE(contains(report, "\"cells_changed\": 35"));
    EXPECT_TRUE(contains(report, "\"authorized_by\": \"synthetic-fixture (SCAU-UFM golden G38)\""));
    EXPECT_TRUE(contains(report, "\"findings\": []"));
    EXPECT_TRUE(contains(report, "\"source_dem_sha256\": \""));
    EXPECT_TRUE(contains(report, "\"conditioned_dem_sha256\": \""));
    EXPECT_FALSE(contains(report, "\"reproject\": {"));   // not authorized -> not run

    const auto conditioned_manifest = read_text(case_dir() / "synthetic_city_block_conditioned.pipeline_manifest.json");
    EXPECT_TRUE(contains(conditioned_manifest, "\"case_sha256\": \"7d3e43eacb45e573"));
    EXPECT_TRUE(contains(conditioned_manifest, "\"enabled\": true"));
    EXPECT_TRUE(contains(conditioned_manifest, "\"fill\": 1"));

    // Disabled policy on the UNMODIFIED sample: the case SHA is the G30 fixture SHA.
    const auto off_manifest = read_text(case_dir() / "synthetic_city_block_terrain_off.pipeline_manifest.json");
    EXPECT_TRUE(contains(off_manifest, "\"enabled\": false"));
    EXPECT_TRUE(contains(off_manifest, "\"conditioned_dem_sha256\": null"));
    const auto g30_manifest = read_text(case_dir() / ".." / ".." / "preproc_gis_synthetic_case" / "cases" /
                                        "synthetic_city_block.pipeline_manifest.json");
    const std::string key = "\"case_sha256\": \"";
    const auto g30_pos = g30_manifest.find(key);
    ASSERT_NE(g30_pos, std::string::npos);
    const std::string g30_sha = g30_manifest.substr(g30_pos + key.size(), 64);
    EXPECT_EQ(g30_sha.size(), 64U);
    EXPECT_TRUE(contains(off_manifest, key + g30_sha)) << "disabled terrain policy must reproduce the G30 case bytes";
}

TEST(GoldenPreprocTerrainConditionCase, LakeAtRestStaysAtRestOnConditionedBed) {
    const auto loaded = load_surface2d_case(case_dir() / "synthetic_city_block_conditioned.stcf.nc");
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

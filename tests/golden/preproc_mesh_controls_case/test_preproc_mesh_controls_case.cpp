// G32 preproc_mesh_controls_case: locks the M287-B4 mesh-control pipeline
// output as a committed deterministic fixture:
//   synthetic D-5 package + cases/mesh_controls.geojson (3 breaklines, 2
//   refinement regions, one region owning a building hole)
//   -> scau_preproc.pipeline (gmsh embed + size fields, bitwise rerun check)
//   -> synthetic_city_block_controls.stcf.nc
// The fixture was certified by `scau_preproc validate`; its SHA-256 is in the
// committed pipeline manifest. This golden strict-loads the case through the
// authoritative read path and asserts, INDEPENDENTLY of the generator's own
// python-side check:
//   1. recorded shape and field domains (placeholder DPM rules unchanged);
//   2. every breakline segment and every refinement-region ring survives as a
//      chain of mesh edges (constraint preservation = the B4 exit criterion);
//   3. cells inside each refinement region are finer than the unrefined
//      remainder and bounded by the requested size;
//   4. no fatal quality finding; a wet lake at rest stays at rest (1e-12).
// Regeneration (gmsh) is NOT run here: hosted CI has no gmsh; determinism is
// enforced pipeline-side and re-checked whenever the fixture is regenerated.

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <set>
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
constexpr double kOnSegmentTolerance = 1.0e-6;

using Point = std::array<double, 2>;

// Mirrors cases/mesh_controls.geojson (the committed operator input).
const std::vector<std::vector<Point>> kBreaklines{
    {{10.0, 20.0}, {190.0, 20.0}},                    // BL_ROAD_EDGE (size 3 m / dist 12 m)
    {{15.0, 85.0}, {35.0, 100.0}, {60.0, 110.0}},     // BL_DIAGONAL
    {{85.0, 78.0}, {112.0, 104.0}},                   // BL_IN_POND (inside RR_POND)
};

struct Region {
    const char* id;
    double size_m;
    std::size_t recorded_cells;
    std::vector<Point> ring;  // closed
};

const std::vector<Region> kRegions{
    {"RR_POND", 2.5, 288U, {{78, 72}, {118, 72}, {118, 108}, {78, 108}, {78, 72}}},
    {"RR_BUILDING_1", 4.0, 104U, {{30, 30}, {76, 30}, {76, 76}, {30, 76}, {30, 30}}},
};

std::filesystem::path fixture_path() {
    return std::filesystem::path(SCAU_PREPROC_MESH_CONTROLS_CASE_DIR)
        / "synthetic_city_block_controls.stcf.nc";
}

struct IndexedMesh {
    std::vector<Point> nodes;
    std::unordered_map<std::string, std::size_t> node_index;
    std::set<std::pair<std::size_t, std::size_t>> edges;

    explicit IndexedMesh(const scau::mesh::Mesh& mesh) {
        for (const auto& node : mesh.nodes) {
            node_index.emplace(node.id, nodes.size());
            nodes.push_back({node.x, node.y});
        }
        for (const auto& edge : mesh.edges) {
            const auto a = node_index.at(edge.node_ids[0]);
            const auto b = node_index.at(edge.node_ids[1]);
            edges.emplace(std::min(a, b), std::max(a, b));
        }
    }
};

double point_segment_distance(const Point& p, const Point& a, const Point& b) {
    const double dx = b[0] - a[0];
    const double dy = b[1] - a[1];
    const double length2 = dx * dx + dy * dy;
    double t = ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / length2;
    t = std::clamp(t, 0.0, 1.0);
    return std::hypot(p[0] - (a[0] + t * dx), p[1] - (a[1] + t * dy));
}

// A segment is preserved when the mesh nodes lying on it, ordered along the
// segment, start/end at its vertices and are pairwise connected by mesh edges.
std::size_t preserved_edge_chain_length(const IndexedMesh& mesh, const Point& a, const Point& b) {
    const double dx = b[0] - a[0];
    const double dy = b[1] - a[1];
    const double length2 = dx * dx + dy * dy;
    std::vector<std::pair<double, std::size_t>> on_segment;
    for (std::size_t n = 0; n < mesh.nodes.size(); ++n) {
        if (point_segment_distance(mesh.nodes[n], a, b) <= kOnSegmentTolerance) {
            const auto& p = mesh.nodes[n];
            on_segment.emplace_back(((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / length2, n);
        }
    }
    std::sort(on_segment.begin(), on_segment.end());
    if (on_segment.size() < 2U) return 0U;
    const auto& first = mesh.nodes[on_segment.front().second];
    const auto& last = mesh.nodes[on_segment.back().second];
    if (std::hypot(first[0] - a[0], first[1] - a[1]) > kOnSegmentTolerance) return 0U;
    if (std::hypot(last[0] - b[0], last[1] - b[1]) > kOnSegmentTolerance) return 0U;
    for (std::size_t k = 0; k + 1 < on_segment.size(); ++k) {
        const auto u = on_segment[k].second;
        const auto v = on_segment[k + 1].second;
        if (mesh.edges.count({std::min(u, v), std::max(u, v)}) == 0U) return 0U;
    }
    return on_segment.size() - 1U;
}

bool point_in_ring(const Point& p, const std::vector<Point>& ring) {
    bool inside = false;
    for (std::size_t i = 0; i + 1 < ring.size(); ++i) {
        const auto& [x1, y1] = ring[i];
        const auto& [x2, y2] = ring[i + 1];
        if ((y1 > p[1]) != (y2 > p[1])) {
            const double x_cross = x1 + (p[1] - y1) * (x2 - x1) / (y2 - y1);
            if (x_cross > p[0]) inside = !inside;
        }
    }
    return inside;
}

Point cell_centroid(const scau::mesh::Mesh& mesh, const IndexedMesh& indexed, std::size_t cell) {
    Point c{0.0, 0.0};
    for (const auto& node_id : mesh.cells[cell].node_ids) {
        const auto& p = indexed.nodes[indexed.node_index.at(node_id)];
        c[0] += p[0];
        c[1] += p[1];
    }
    const auto n = static_cast<double>(mesh.cells[cell].node_ids.size());
    return {c[0] / n, c[1] / n};
}

TEST(GoldenPreprocMeshControlsCase, FixtureMatchesRecordedShapeAndFieldDomains) {
    const auto loaded = load_surface2d_case(fixture_path());

    EXPECT_EQ(loaded.mesh.nodes.size(), 1113U);
    EXPECT_EQ(loaded.mesh.cells.size(), 1070U);
    EXPECT_EQ(loaded.mesh.edges.size(), 2184U);
    EXPECT_EQ(loaded.bed_elevations.size(), 1070U);

    std::size_t triangles = 0U;
    for (std::size_t i = 0; i < loaded.mesh.cells.size(); ++i) {
        triangles += loaded.mesh.cells[i].node_ids.size() == 3U ? 1U : 0U;
        // Mesh controls change geometry only; DPM placeholder rules are untouched.
        EXPECT_EQ(loaded.dpm_fields.cells[i].phi_t, 1.0);
        EXPECT_GE(loaded.bed_elevations[i], 10.0);
        EXPECT_LE(loaded.bed_elevations[i], 10.9);
        const double n = loaded.source_fields.manning_n[i];
        EXPECT_TRUE(n == 0.015 || n == 0.020 || n == 0.030);
    }
    EXPECT_EQ(triangles, 44U);  // recorded: 44 triangles / 1026 quadrilaterals
    for (std::size_t e = 0; e < loaded.mesh.edges.size(); ++e) {
        EXPECT_EQ(loaded.dpm_fields.edges[e].omega_edge, 1.0);
        EXPECT_EQ(loaded.dpm_fields.edges[e].phi_e_n, 1.0);
    }
}

TEST(GoldenPreprocMeshControlsCase, BreaklinesAndRegionRingsArePreservedAsMeshEdgeChains) {
    const auto loaded = load_surface2d_case(fixture_path());
    const IndexedMesh indexed(loaded.mesh);

    const std::array<std::size_t, 3> recorded_edges_on_breakline{59U, 8U, 15U};
    for (std::size_t b = 0; b < kBreaklines.size(); ++b) {
        std::size_t chain = 0U;
        for (std::size_t s = 0; s + 1 < kBreaklines[b].size(); ++s) {
            const auto segment_chain =
                preserved_edge_chain_length(indexed, kBreaklines[b][s], kBreaklines[b][s + 1]);
            EXPECT_GT(segment_chain, 0U) << "breakline " << b << " segment " << s << " not preserved";
            chain += segment_chain;
        }
        EXPECT_EQ(chain, recorded_edges_on_breakline[b]) << "breakline " << b;
    }
    for (const auto& region : kRegions) {
        for (std::size_t s = 0; s + 1 < region.ring.size(); ++s) {
            EXPECT_GT(preserved_edge_chain_length(indexed, region.ring[s], region.ring[s + 1]), 0U)
                << region.id << " ring segment " << s;
        }
    }
}

TEST(GoldenPreprocMeshControlsCase, RefinementRegionsAreFinerThanUnrefinedRemainder) {
    const auto loaded = load_surface2d_case(fixture_path());
    const IndexedMesh indexed(loaded.mesh);

    std::vector<int> owner(loaded.mesh.cells.size(), -1);
    for (std::size_t c = 0; c < loaded.mesh.cells.size(); ++c) {
        const auto centroid = cell_centroid(loaded.mesh, indexed, c);
        for (std::size_t r = 0; r < kRegions.size(); ++r) {
            if (point_in_ring(centroid, kRegions[r].ring)) owner[c] = static_cast<int>(r);
        }
    }
    std::unordered_map<std::string, std::size_t> cell_index;
    for (std::size_t c = 0; c < loaded.mesh.cells.size(); ++c) cell_index.emplace(loaded.mesh.cells[c].id, c);

    std::vector<double> sum(kRegions.size() + 1U, 0.0);
    std::vector<std::size_t> count(kRegions.size() + 1U, 0U);
    std::vector<double> max_len(kRegions.size() + 1U, 0.0);
    for (const auto& edge : loaded.mesh.edges) {
        std::set<int> owners;
        if (edge.left_cell) owners.insert(owner[cell_index.at(*edge.left_cell)]);
        if (edge.right_cell) owners.insert(owner[cell_index.at(*edge.right_cell)]);
        for (const int o : owners) {
            const auto slot = static_cast<std::size_t>(o + 1);
            sum[slot] += edge.length;
            count[slot] += 1U;
            max_len[slot] = std::max(max_len[slot], edge.length);
        }
    }
    const double unrefined_mean = sum[0] / static_cast<double>(count[0]);
    for (std::size_t r = 0; r < kRegions.size(); ++r) {
        const auto cells_inside =
            static_cast<std::size_t>(std::count(owner.begin(), owner.end(), static_cast<int>(r)));
        EXPECT_EQ(cells_inside, kRegions[r].recorded_cells) << kRegions[r].id;
        const double mean = sum[r + 1] / static_cast<double>(count[r + 1]);
        // Requested size_m is a target for gmsh, not a hard bound: lock a 2x
        // envelope plus a strict "finer than outside" ordering, and pin the
        // manifest-recorded region means (mean_edge_length_inside_m: 2.2746 m
        // / 3.5487 m; unrefined remainder ~5.3 m for lc = 8 m).
        EXPECT_LE(max_len[r + 1], 2.0 * kRegions[r].size_m) << kRegions[r].id;
        EXPECT_LT(mean, 0.75 * unrefined_mean) << kRegions[r].id;
        EXPECT_NEAR(mean, r == 0 ? 2.2746 : 3.5487, 1.0e-3) << kRegions[r].id;
    }
}

TEST(GoldenPreprocMeshControlsCase, FixtureMeshHasNoFatalQualityFindings) {
    const auto loaded = load_surface2d_case(fixture_path());
    const auto report = scau::mesh::evaluate_mesh_quality(loaded.mesh);
    EXPECT_FALSE(report.fatal);
    EXPECT_EQ(report.reviewed_cells, loaded.mesh.cells.size());
    // Recorded pipeline quality: min angle 18.96 deg / max edge ratio 3.17,
    // inside the 10 deg / 20 review thresholds -> no review items either.
    EXPECT_TRUE(report.issues.empty());
}

TEST(GoldenPreprocMeshControlsCase, FileCarriedLakeAtRestOverSampledDemStaysAtRest) {
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

    // dt tightened for the 2.5 m refinement cells (recorded minimum edge
    // lengths ~1.5 m; sqrt(g*h) ~ 4.4 m/s).
    const StepConfig config{.dt = 0.02, .cfl_safety = 0.45, .c_rollback = 10.0};
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

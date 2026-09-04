// G34 coupling_spatial_candidates_case: locks the M287-C5 spatial-candidate
// mapping mode as a committed fixture:
//   synthetic D-5 package, mode = spatial_candidates (NO mapping tables read)
//   -> surface_swmm_mapping.json / roof_drain_mapping.json / simdriver_links.conf
// The golden asserts through the authoritative consumers (SimDriver runtime
// config parser + surface2d loader) that:
//   1. spatial candidates land in exactly the cells the explicit G31 tables
//      resolve to (containment is the same geometry, only the authority differs);
//   2. the placeholder crest of every candidate equals the containing cell's
//      z_b from the STCF case, so nothing is invented beyond recorded rules;
//   3. every candidate is review-only / needs_confirmation, the report declares
//      the mode and the roof distance threshold, and outfall O1 never becomes
//      a street inlet.
// Regeneration is pipeline-side; nothing here re-implements the generator.

#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

#include <gtest/gtest.h>

#include "runtime_config_io.hpp"
#include "surface2d/stcf_bridge/load_case.hpp"

namespace {

using scau::apps::sim_driver::read_runtime_config_file;
using scau::surface2d::load_surface2d_case;

std::filesystem::path case_dir() {
    return std::filesystem::path(SCAU_COUPLING_SPATIAL_CASE_DIR);
}

std::filesystem::path explicit_case_dir() {
    return std::filesystem::path(SCAU_COUPLING_MAPS_CASE_DIR);
}

std::filesystem::path mesh_fixture_path() {
    return std::filesystem::path(SCAU_PREPROC_GIS_CASE_DIR) / "synthetic_city_block.stcf.nc";
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

std::size_t count(const std::string& text, const std::string& needle) {
    std::size_t total = 0U;
    for (auto pos = text.find(needle); pos != std::string::npos; pos = text.find(needle, pos + 1U)) {
        ++total;
    }
    return total;
}

using NodeXY = std::unordered_map<std::string, std::pair<double, double>>;

bool point_in_cell(const scau::mesh::Mesh& mesh, const NodeXY& xy, std::size_t cell_index,
                   double x, double y) {
    const auto& cell = mesh.cells[cell_index];
    bool inside = false;
    const std::size_t n = cell.node_ids.size();
    for (std::size_t i = 0; i < n; ++i) {
        const auto& a = xy.at(cell.node_ids[i]);
        const auto& b = xy.at(cell.node_ids[(i + 1U) % n]);
        if ((a.second > y) != (b.second > y)) {
            const double x_cross = a.first + (y - a.second) * (b.first - a.first) / (b.second - a.second);
            if (x_cross > x) inside = !inside;
        }
    }
    return inside;
}

TEST(GoldenCouplingSpatialCandidatesCase, SpatialCandidatesResolveToTheExplicitCells) {
    const auto spatial = read_runtime_config_file(case_dir() / "simdriver_links.conf");
    const auto explicit_links = read_runtime_config_file(explicit_case_dir() / "simdriver_links.conf");

    ASSERT_EQ(spatial.surface_drainage.size(), 2U);
    ASSERT_EQ(explicit_links.surface_drainage.size(), 2U);
    for (std::size_t i = 0; i < 2U; ++i) {
        EXPECT_EQ(spatial.surface_drainage[i].node_name, explicit_links.surface_drainage[i].node_name);
        EXPECT_EQ(spatial.surface_drainage[i].cell, explicit_links.surface_drainage[i].cell);
        // Same geometry, different authority: the crest is NOT the table value.
        EXPECT_NE(spatial.surface_drainage[i].crest_level, explicit_links.surface_drainage[i].crest_level);
        EXPECT_EQ(spatial.surface_drainage[i].exchange_width, 1.0);
        EXPECT_EQ(spatial.surface_drainage[i].priority_weight, 1.0);
    }
    EXPECT_EQ(spatial.surface_drainage[0].cell, 379U);
    EXPECT_EQ(spatial.surface_drainage[1].cell, 234U);
}

TEST(GoldenCouplingSpatialCandidatesCase, PlaceholderCrestIsTheContainingCellBed) {
    const auto loaded = load_surface2d_case(mesh_fixture_path());
    const auto spatial = read_runtime_config_file(case_dir() / "simdriver_links.conf");
    NodeXY xy;
    for (const auto& node : loaded.mesh.nodes) xy.emplace(node.id, std::make_pair(node.x, node.y));

    const std::pair<double, double> node_xy[] = {{55.0, 10.0}, {135.0, 10.0}};  // J1, J2 from [COORDINATES]
    ASSERT_EQ(spatial.surface_drainage.size(), 2U);
    for (std::size_t i = 0; i < 2U; ++i) {
        const auto cell = spatial.surface_drainage[i].cell;
        ASSERT_LT(cell, loaded.mesh.cells.size());
        EXPECT_TRUE(point_in_cell(loaded.mesh, xy, cell, node_xy[i].first, node_xy[i].second));
        EXPECT_EQ(spatial.surface_drainage[i].crest_level, loaded.bed_elevations[cell]);
    }
    // Recorded values (DEM 10.0..10.9 sampled at the cell centroid).
    EXPECT_EQ(spatial.surface_drainage[0].crest_level, 10.4);
    EXPECT_EQ(spatial.surface_drainage[1].crest_level, 10.7);
}

TEST(GoldenCouplingSpatialCandidatesCase, EverythingIsReviewOnlyAndModeIsDeclared) {
    const auto surface = read_text(case_dir() / "surface_swmm_mapping.json");
    const auto roof = read_text(case_dir() / "roof_drain_mapping.json");
    const auto report = read_text(case_dir() / "mapping_report.json");

    EXPECT_EQ(count(surface, "\"confidence\": \"review\""), 2U);
    EXPECT_EQ(count(surface, "\"review_status\": \"needs_confirmation\""), 2U);
    EXPECT_EQ(count(surface, "\"method\": \"spatial_point_in_cell_candidate\""), 2U);
    EXPECT_EQ(count(surface, "\"exchange_elevation_source\": \"placeholder:cell_z_b\""), 2U);
    EXPECT_FALSE(contains(surface, "\"confidence\": \"high\""));
    EXPECT_FALSE(contains(surface, "\"swmm_node_id\": \"O1\""));  // outfall is never a street inlet

    EXPECT_EQ(count(roof, "\"method\": \"spatial_nearest_junction_candidate\""), 2U);
    EXPECT_EQ(count(roof, "\"confidence\": \"review\""), 2U);
    EXPECT_EQ(count(roof, "\"catchment_area_source\": \"placeholder:footprint_area\""), 2U);
    EXPECT_EQ(count(roof, "\"node_candidate_distance_m\": 45.0"), 2U);  // both roofs 45 m from their junction
    EXPECT_TRUE(contains(roof, "\"building_id\": \"BLDG_SYNTH_001\""));
    EXPECT_TRUE(contains(roof, "\"building_id\": \"BLDG_SYNTH_002\""));

    EXPECT_TRUE(contains(report, "\"mode\": \"spatial_candidates\""));
    EXPECT_TRUE(contains(report, "\"roof_node_max_distance_m\": 50.0"));
    EXPECT_TRUE(contains(report, "\"high\": 0"));
    EXPECT_TRUE(contains(report, "\"runtime_semantics\": \"none (Q_limit/deficit/arbitration remain CouplingLib-owned)\""));
}

}  // namespace

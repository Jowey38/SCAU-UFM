// G33 coupling_confirmations_synthetic_case: locks the M287-C4 confirmation
// contract (pipeline stage E') as a committed fixture:
//   G31 candidate relations (synthetic D-5 package on the G30 mesh)
//   + cases/confirmed/*.json (accept J1, retarget J2 -> cell 50 / crest 9.6,
//     reject roof 1, accept roof 2)
//   -> effective_links.json + effective/simdriver_links.conf
// The golden asserts through the AUTHORITATIVE consumer (SimDriver runtime
// config parser + surface2d loader) that:
//   1. the candidate fragment and the effective fragment are both valid
//      RuntimeConfig v2 and differ exactly by the operator decisions;
//   2. the retargeted cell contains no SWMM node (it is an operator choice,
//      not a containment result) but IS a valid neighbouring road cell, and
//      the accepted link is unchanged from its candidate;
//   3. effective_links.json carries confirmation provenance (confirmed_by,
//      timestamp, candidate hash) and a complete status with zero unconfirmed
//      candidates, and the rejected roof candidate is absent.
// Regeneration is pipeline-side (python); nothing here re-implements the merge.

#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <regex>
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
    return std::filesystem::path(SCAU_COUPLING_CONFIRMATIONS_CASE_DIR);
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

// Minimal deterministic probes into the committed JSON (no JSON library in
// the golden target; the contract fields are asserted by literal substring
// so a schema drift is loud).
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

std::pair<double, double> centroid(const scau::mesh::Mesh& mesh, const NodeXY& xy, std::size_t cell) {
    double cx = 0.0, cy = 0.0;
    for (const auto& id : mesh.cells[cell].node_ids) {
        cx += xy.at(id).first;
        cy += xy.at(id).second;
    }
    const auto n = static_cast<double>(mesh.cells[cell].node_ids.size());
    return {cx / n, cy / n};
}

TEST(GoldenCouplingConfirmationsSyntheticCase, EffectiveFragmentReflectsOperatorDecisions) {
    const auto candidates = read_runtime_config_file(case_dir() / "simdriver_links.conf");
    const auto effective = read_runtime_config_file(case_dir() / "effective" / "simdriver_links.conf");

    ASSERT_EQ(candidates.surface_drainage.size(), 2U);
    ASSERT_EQ(effective.surface_drainage.size(), 2U);
    EXPECT_EQ(effective.version, 2);

    // accept: J1 unchanged from its candidate.
    EXPECT_EQ(effective.surface_drainage[0].node_name, "J1");
    EXPECT_EQ(effective.surface_drainage[0].cell, candidates.surface_drainage[0].cell);
    EXPECT_EQ(effective.surface_drainage[0].cell, 379U);
    EXPECT_EQ(effective.surface_drainage[0].crest_level, candidates.surface_drainage[0].crest_level);

    // retarget: J2 moved 234 -> 50 and crest 9.5 -> 9.6 by the operator.
    EXPECT_EQ(effective.surface_drainage[1].node_name, "J2");
    EXPECT_EQ(candidates.surface_drainage[1].cell, 234U);
    EXPECT_EQ(effective.surface_drainage[1].cell, 50U);
    EXPECT_EQ(candidates.surface_drainage[1].crest_level, 9.5);
    EXPECT_EQ(effective.surface_drainage[1].crest_level, 9.6);

    for (const auto& link : effective.surface_drainage) {
        EXPECT_EQ(link.exchange_width, 1.0);   // recorded placeholders pass through untouched
        EXPECT_EQ(link.priority_weight, 1.0);
    }
}

TEST(GoldenCouplingConfirmationsSyntheticCase, RetargetedCellIsAnOperatorChoiceOnTheRoad) {
    const auto loaded = load_surface2d_case(mesh_fixture_path());
    const auto effective = read_runtime_config_file(case_dir() / "effective" / "simdriver_links.conf");
    NodeXY xy;
    for (const auto& node : loaded.mesh.nodes) xy.emplace(node.id, std::make_pair(node.x, node.y));

    ASSERT_EQ(effective.surface_drainage.size(), 2U);
    const auto accepted = effective.surface_drainage[0].cell;
    const auto retargeted = effective.surface_drainage[1].cell;
    ASSERT_LT(accepted, loaded.mesh.cells.size());
    ASSERT_LT(retargeted, loaded.mesh.cells.size());

    // J1 (55,10) is still contained by its accepted cell.
    EXPECT_TRUE(point_in_cell(loaded.mesh, xy, accepted, 55.0, 10.0));
    // J2 (135,10) is NOT inside the retargeted cell: the effective link is an
    // operator decision, distinguishable from the generator's containment.
    EXPECT_FALSE(point_in_cell(loaded.mesh, xy, retargeted, 135.0, 10.0));
    // ...but the operator moved it to an adjacent road-strip cell (landcover
    // road = y in [0,20], Manning 0.015) within one characteristic length.
    const auto [cx, cy] = centroid(loaded.mesh, xy, retargeted);
    EXPECT_GT(cy, 0.0);
    EXPECT_LT(cy, 20.0);
    EXPECT_LT(std::abs(cx - 135.0), 8.0);
    EXPECT_EQ(loaded.source_fields.manning_n[retargeted], 0.015);
    // Crest lifted above the bed of the new cell (DEM 10.0..10.9): the
    // operator crest 9.6 stays below z_b, consistent with the candidate's
    // synthetic 9.5 (the fixture does not invent a crest rule).
    EXPECT_LT(effective.surface_drainage[1].crest_level, loaded.bed_elevations[retargeted]);
}

TEST(GoldenCouplingConfirmationsSyntheticCase, EffectiveLinksCarryProvenanceAndCompleteStatus) {
    const auto text = read_text(case_dir() / "effective_links.json");
    EXPECT_TRUE(contains(text, "\"effective_links_schema_version\": 1"));
    EXPECT_TRUE(contains(text, "\"status\": \"complete\""));
    EXPECT_TRUE(contains(text, "\"unconfirmed_total\": 0"));
    EXPECT_TRUE(contains(text, "\"confirmations_loaded\": 4"));
    EXPECT_TRUE(contains(text, "\"runtime_semantics\": \"none (Q_limit/deficit/arbitration remain CouplingLib-owned)\""));

    // Every effective link is confirmed and carries who/when/what-hash.
    EXPECT_EQ(count(text, "\"review_status\": \"confirmed\""), 3U);  // J1, J2, roof 2
    EXPECT_EQ(count(text, "\"confirmed_by\": \"synthetic_fixture_operator\""), 3U);
    EXPECT_EQ(count(text, "\"confirmed_at\": \"2026-09-03T10:00:00\""), 3U);
    EXPECT_EQ(count(text, "\"decision\": \"accept\""), 2U);
    EXPECT_EQ(count(text, "\"decision\": \"retarget\""), 1U);
    EXPECT_EQ(count(text, "\"decision\": \"reject\""), 0U);    // rejected links are absent, not flagged
    EXPECT_TRUE(contains(text, "\"method\": \"explicit_id_plus_point_in_cell+operator_retarget\""));
    EXPECT_FALSE(contains(text, "\"mapping_id\": \"MAP_ROOF_SYNTH_001\""));
    EXPECT_TRUE(contains(text, "\"mapping_id\": \"MAP_ROOF_SYNTH_002\""));
    EXPECT_TRUE(std::regex_search(text, std::regex("\"candidate_sha256\": \"[0-9a-f]{64}\"")));

    // Provenance of the inputs: all four confirmation files and both
    // candidate files are hashed into the report.
    EXPECT_EQ(count(text, ".json\": \""), 6U);
}

}  // namespace

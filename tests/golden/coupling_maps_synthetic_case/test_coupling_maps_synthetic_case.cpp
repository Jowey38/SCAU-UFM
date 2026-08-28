// G31 coupling_maps_synthetic_case: locks the M287-C coupling-map generator
// output for the synthetic D-5 package and proves SimDriver consumption:
//   1. The committed SimDriver fragment (simdriver_links.conf) parses through
//      the REAL runtime-config parser (apps/sim_driver) with the exact ground
//      chain the generator emitted (cells 379/234 -> J1/J2, crest 10.0/9.5).
//   2. C++ geometry independently re-verifies the python point-in-cell
//      resolution: each SWMM node coordinate lies inside its mapped cell of
//      the committed G30 mesh fixture, and roof overflow target cells exist.
// JSON schema checks stay python-side (no JSON dependency in the C++ graph);
// the fragment is the machine contract SimDriver actually consumes.

#include <cstddef>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <utility>

#include <gtest/gtest.h>

#include "runtime_config_io.hpp"
#include "surface2d/stcf_bridge/load_case.hpp"

namespace {

using scau::apps::sim_driver::read_runtime_config_file;
using scau::surface2d::load_surface2d_case;

std::filesystem::path mapping_case_dir() {
    return std::filesystem::path(SCAU_COUPLING_MAPS_CASE_DIR);
}

std::filesystem::path mesh_fixture_path() {
    return std::filesystem::path(SCAU_PREPROC_GIS_CASE_DIR) / "synthetic_city_block.stcf.nc";
}

using NodeXY = std::unordered_map<std::string, std::pair<double, double>>;

NodeXY node_coordinates(const scau::mesh::Mesh& mesh) {
    NodeXY coordinates;
    for (const auto& node : mesh.nodes) {
        coordinates.emplace(node.id, std::make_pair(node.x, node.y));
    }
    return coordinates;
}

bool point_in_cell(
    const scau::mesh::Mesh& mesh,
    const NodeXY& coordinates,
    std::size_t cell_index,
    double x,
    double y) {
    const auto& cell = mesh.cells[cell_index];
    bool inside = false;
    const std::size_t n = cell.node_ids.size();
    for (std::size_t i = 0; i < n; ++i) {
        const auto& a = coordinates.at(cell.node_ids[i]);
        const auto& b = coordinates.at(cell.node_ids[(i + 1U) % n]);
        if ((a.second > y) != (b.second > y)) {
            const double x_cross =
                a.first + (y - a.second) * (b.first - a.first) / (b.second - a.second);
            if (x_cross > x) {
                inside = !inside;
            }
        }
    }
    return inside;
}

TEST(GoldenCouplingMapsSyntheticCase, SimDriverParsesTheCommittedFragment) {
    const auto config = read_runtime_config_file(mapping_case_dir() / "simdriver_links.conf");

    ASSERT_EQ(config.surface_drainage.size(), 2U);
    EXPECT_EQ(config.surface_drainage[0].cell, 379U);
    EXPECT_EQ(config.surface_drainage[0].node_name, "J1");
    EXPECT_EQ(config.surface_drainage[0].crest_level, 10.0);
    EXPECT_EQ(config.surface_drainage[1].cell, 234U);
    EXPECT_EQ(config.surface_drainage[1].node_name, "J2");
    EXPECT_EQ(config.surface_drainage[1].crest_level, 9.5);
    for (const auto& link : config.surface_drainage) {
        EXPECT_EQ(link.exchange_width, 1.0);
        EXPECT_EQ(link.priority_weight, 1.0);
    }
    EXPECT_TRUE(config.surface_river.empty());
    EXPECT_TRUE(config.drainage_river.empty());
}

TEST(GoldenCouplingMapsSyntheticCase, GroundChainCellsContainTheirSwmmNodes) {
    const auto loaded = load_surface2d_case(mesh_fixture_path());
    const auto config = read_runtime_config_file(mapping_case_dir() / "simdriver_links.conf");

    // Street inlets from the synthetic SWMM [COORDINATES]: J1 (55,10),
    // J2 (135,10) on the road strip. C++ re-verifies python's containment.
    struct Expected {
        std::size_t cell;
        double x;
        double y;
    };
    const Expected expected[] = {{379U, 55.0, 10.0}, {234U, 135.0, 10.0}};
    ASSERT_EQ(config.surface_drainage.size(), 2U);
    const auto coordinates = node_coordinates(loaded.mesh);
    for (std::size_t i = 0; i < 2U; ++i) {
        ASSERT_LT(config.surface_drainage[i].cell, loaded.mesh.cells.size());
        EXPECT_EQ(config.surface_drainage[i].cell, expected[i].cell);
        EXPECT_TRUE(point_in_cell(
            loaded.mesh, coordinates, config.surface_drainage[i].cell,
            expected[i].x, expected[i].y));
    }

    // Roof overflow target candidates recorded by the generator (nearest
    // centroid; review-only) must at least be valid cells of the fixture.
    for (const std::size_t roof_cell : {179U, 408U}) {
        EXPECT_LT(roof_cell, loaded.mesh.cells.size());
    }
}

}  // namespace

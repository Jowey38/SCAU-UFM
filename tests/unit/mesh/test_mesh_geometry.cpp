#include <cmath>
#include <map>
#include <string>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"

namespace {

void expect_near(scau::core::Real actual, scau::core::Real expected) {
    EXPECT_NEAR(actual, expected, 1.0e-15);
}

std::map<std::string, scau::mesh::Edge> edge_map(const scau::mesh::Mesh& mesh) {
    std::map<std::string, scau::mesh::Edge> edges;
    for (const auto& edge : mesh.edges) {
        edges.emplace(edge.id, edge);
    }
    return edges;
}

}  // namespace

TEST(MeshGeometry, BuildsMixedMinimalMesh) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();

    ASSERT_EQ(mesh.nodes.size(), 6U);
    ASSERT_EQ(mesh.cells.size(), 3U);
    EXPECT_EQ(mesh.cells[0].cell_type, scau::mesh::CellType::Quadrilateral);
    EXPECT_EQ(mesh.cells[1].cell_type, scau::mesh::CellType::Triangle);
    EXPECT_EQ(mesh.cells[2].cell_type, scau::mesh::CellType::Triangle);

    const auto edges = edge_map(mesh);
    ASSERT_EQ(edges.size(), 8U);

    EXPECT_EQ(edges.at("E0").node_ids, (std::array<std::string, 2>{"N0", "N1"}));
    EXPECT_EQ(edges.at("E0").left_cell, std::nullopt);
    EXPECT_EQ(edges.at("E0").right_cell, std::optional<std::string>{"C0"});
    expect_near(edges.at("E0").normal.x, 0.0);
    expect_near(edges.at("E0").normal.y, 1.0);

    EXPECT_EQ(edges.at("E1").node_ids, (std::array<std::string, 2>{"N1", "N4"}));
    EXPECT_EQ(edges.at("E1").left_cell, std::optional<std::string>{"C0"});
    EXPECT_EQ(edges.at("E1").right_cell, std::optional<std::string>{"C1"});
    expect_near(edges.at("E1").normal.x, 1.0);
    expect_near(edges.at("E1").normal.y, 0.0);

    EXPECT_EQ(edges.at("E3").node_ids, (std::array<std::string, 2>{"N2", "N4"}));
    EXPECT_EQ(edges.at("E3").left_cell, std::optional<std::string>{"C1"});
    EXPECT_EQ(edges.at("E3").right_cell, std::optional<std::string>{"C2"});
    expect_near(edges.at("E3").normal.x, std::sqrt(0.5));
    expect_near(edges.at("E3").normal.y, std::sqrt(0.5));

    EXPECT_EQ(edges.at("E6").node_ids, (std::array<std::string, 2>{"N2", "N5"}));
    EXPECT_EQ(edges.at("E6").left_cell, std::nullopt);
    EXPECT_EQ(edges.at("E6").right_cell, std::optional<std::string>{"C2"});
    expect_near(edges.at("E6").normal.x, -1.0);
    expect_near(edges.at("E6").normal.y, 0.0);
}

TEST(MeshGeometry, ComputesAreasCentersLengthsAndMidpoints) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto nodes = scau::mesh::node_lookup(mesh.nodes);
    const auto edges = edge_map(mesh);

    expect_near(scau::mesh::cell_area(mesh.cells[0], nodes), 1.0);
    expect_near(scau::mesh::cell_area(mesh.cells[1], nodes), 0.5);
    expect_near(scau::mesh::cell_area(mesh.cells[2], nodes), 0.5);

    const auto c0 = scau::mesh::cell_center(mesh.cells[0], nodes);
    expect_near(c0.x, 0.5);
    expect_near(c0.y, 0.5);

    const auto c1 = scau::mesh::cell_center(mesh.cells[1], nodes);
    expect_near(c1.x, 4.0 / 3.0);
    expect_near(c1.y, 1.0 / 3.0);

    expect_near(edges.at("E3").length, std::sqrt(2.0));
    expect_near(edges.at("E3").midpoint.x, 1.5);
    expect_near(edges.at("E3").midpoint.y, 0.5);
}

TEST(MeshGeometry, BuildsControlMeshes) {
    EXPECT_EQ(scau::mesh::build_quad_only_control_mesh().cells.size(), 2U);
    EXPECT_EQ(scau::mesh::build_tri_only_control_mesh().cells.size(), 4U);
}

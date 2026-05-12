#include <gtest/gtest.h>

#include "mesh/mesh.hpp"

namespace {

std::vector<scau::mesh::Node> triangle_nodes() {
    return {
        scau::mesh::Node{.id = "N0", .x = 0.0, .y = 0.0},
        scau::mesh::Node{.id = "N1", .x = 1.0, .y = 0.0},
        scau::mesh::Node{.id = "N3", .x = 0.0, .y = 1.0},
    };
}

std::vector<scau::mesh::Cell> triangle_cells() {
    return {scau::mesh::Cell{.id = "C0", .cell_type = scau::mesh::CellType::Triangle, .node_ids = {"N0", "N1", "N3"}}};
}

}  // namespace

TEST(MeshValidation, RejectsInvalidCellTopology) {
    EXPECT_THROW(
        static_cast<void>(scau::mesh::build_mesh(
            triangle_nodes(),
            {scau::mesh::Cell{.id = "C0", .cell_type = scau::mesh::CellType::Triangle, .node_ids = {"N0", "N1", "N3", "N0"}}})),
        std::invalid_argument);

    EXPECT_THROW(
        static_cast<void>(scau::mesh::build_mesh(
            triangle_nodes(),
            {scau::mesh::Cell{.id = "C0", .cell_type = scau::mesh::CellType::Quadrilateral, .node_ids = {"N0", "N1", "N3"}}})),
        std::invalid_argument);

    EXPECT_THROW(
        static_cast<void>(scau::mesh::build_mesh(
            triangle_nodes(),
            {scau::mesh::Cell{.id = "C0", .cell_type = scau::mesh::CellType::Triangle, .node_ids = {"N0", "N1", "NX"}}})),
        std::invalid_argument);
}

TEST(MeshValidation, RejectsInvalidEdgeSpecs) {
    EXPECT_THROW(
        static_cast<void>(scau::mesh::build_mesh(
            triangle_nodes(),
            triangle_cells(),
            {
                scau::mesh::EdgeSpec{.id = "E0", .node_ids = {"N1", "N0"}, .left_cell = "C0", .right_cell = std::nullopt},
                scau::mesh::EdgeSpec{.id = "E1", .node_ids = {"N1", "N3"}, .left_cell = "C0", .right_cell = std::nullopt},
                scau::mesh::EdgeSpec{.id = "E2", .node_ids = {"N3", "N0"}, .left_cell = "C0", .right_cell = std::nullopt},
            })),
        std::invalid_argument);

    EXPECT_THROW(
        static_cast<void>(scau::mesh::build_mesh(
            triangle_nodes(),
            triangle_cells(),
            {
                scau::mesh::EdgeSpec{.id = "E0", .node_ids = {"N0", "N1"}, .left_cell = "C0", .right_cell = std::nullopt},
                scau::mesh::EdgeSpec{.id = "E0", .node_ids = {"N1", "N3"}, .left_cell = "C0", .right_cell = std::nullopt},
                scau::mesh::EdgeSpec{.id = "E2", .node_ids = {"N3", "N0"}, .left_cell = "C0", .right_cell = std::nullopt},
            })),
        std::invalid_argument);

    EXPECT_THROW(
        static_cast<void>(scau::mesh::build_mesh(
            triangle_nodes(),
            triangle_cells(),
            {
                scau::mesh::EdgeSpec{.id = "E0", .node_ids = {"N0", "N1"}, .left_cell = "C0", .right_cell = std::nullopt},
                scau::mesh::EdgeSpec{.id = "E1", .node_ids = {"N1", "N3"}, .left_cell = "C0", .right_cell = std::nullopt},
            })),
        std::invalid_argument);

    EXPECT_THROW(
        static_cast<void>(scau::mesh::build_mesh(
            triangle_nodes(),
            triangle_cells(),
            {
                scau::mesh::EdgeSpec{.id = "E0", .node_ids = {"N0", "N1"}, .left_cell = "C0", .right_cell = std::nullopt},
                scau::mesh::EdgeSpec{.id = "E1", .node_ids = {"N1", "N3"}, .left_cell = "C0", .right_cell = std::nullopt},
                scau::mesh::EdgeSpec{.id = "E2", .node_ids = {"N3", "N0"}, .left_cell = "C0", .right_cell = std::nullopt},
                scau::mesh::EdgeSpec{.id = "E3", .node_ids = {"N0", "N3"}, .left_cell = "C0", .right_cell = std::nullopt},
            })),
        std::invalid_argument);
}

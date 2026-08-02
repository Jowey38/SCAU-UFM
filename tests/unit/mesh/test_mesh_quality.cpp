#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "mesh/quality.hpp"

TEST(MeshQuality, AcceptsMixedMinimalMeshAtDefaultThresholds) {
    const auto report = scau::mesh::evaluate_mesh_quality(
        scau::mesh::build_mixed_minimal_mesh());
    EXPECT_FALSE(report.fatal);
    EXPECT_EQ(report.reviewed_cells, 3U);
}

TEST(MeshQuality, ReportsThinCellWithoutSilentRepair) {
    const auto mesh = scau::mesh::build_mesh(
        {
            {.id = "N0", .x = 0.0, .y = 0.0},
            {.id = "N1", .x = 100.0, .y = 0.0},
            {.id = "N2", .x = 100.0, .y = 0.01},
            {.id = "N3", .x = 0.0, .y = 0.01},
        },
        {{.id = "C0", .cell_type = scau::mesh::CellType::Quadrilateral,
          .node_ids = {"N0", "N1", "N2", "N3"}}});

    const auto report = scau::mesh::evaluate_mesh_quality(mesh);
    ASSERT_FALSE(report.issues.empty());
    EXPECT_FALSE(report.fatal);
    EXPECT_EQ(mesh.nodes[1].x, 100.0);
}

TEST(MeshQuality, RejectsInvalidThresholdConfiguration) {
    EXPECT_THROW(
        static_cast<void>(scau::mesh::evaluate_mesh_quality(
            scau::mesh::build_mixed_minimal_mesh(), {.min_angle_degrees = 0.0})),
        std::invalid_argument);
}

#include <cmath>
#include <filesystem>
#include <stdexcept>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "stcf/io_netcdf.hpp"
#include "stcf/schema.hpp"
#include "stcf/topology.hpp"
#include "surface2d/boundary/conditions.hpp"
#include "surface2d/geometry/cache.hpp"
#include "surface2d/state/state.hpp"
#include "surface2d/stcf_bridge/load_case.hpp"
#include "surface2d/time_integration/step.hpp"

namespace {

using scau::stcf::kConnectivityFillValue;
using scau::stcf::make_uniform_dataset;
using scau::stcf::StcfCase;
using scau::surface2d::load_surface2d_case;

StcfCase make_mixed_case() {
    StcfCase stcf_case;
    stcf_case.topology.node_x = {0.0, 1.0, 1.0, 0.0, 2.0};
    stcf_case.topology.node_y = {0.0, 0.0, 1.0, 1.0, 0.0};
    stcf_case.topology.face_nodes = {
        {0, 1, 2, 3},
        {1, 4, 2, kConnectivityFillValue},
    };
    stcf_case.topology.edge_nodes = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0}, {1, 4}, {4, 2},
    };
    stcf_case.topology.edge_faces = {
        {0, kConnectivityFillValue}, {0, 1}, {0, kConnectivityFillValue},
        {0, kConnectivityFillValue}, {1, kConnectivityFillValue},
        {1, kConnectivityFillValue},
    };
    stcf_case.topology.face_edges = {
        {0, 1, 2, 3},
        {4, 5, 1, kConnectivityFillValue},
    };
    stcf_case.fields = make_uniform_dataset(2, 6);
    stcf_case.fields.cells.phi_t = {1.0, 0.8};
    stcf_case.fields.cells.phi_xx = {1.0, 0.7};
    stcf_case.fields.cells.phi_yy = {1.0, 0.75};
    stcf_case.fields.cells.manning_n = {0.02, 0.03};
    stcf_case.fields.cells.z_b = {0.0, 0.1};
    for (std::size_t edge = 0; edge < 6U; ++edge) {
        stcf_case.fields.edges.phi_e_n[edge] = 0.4 + 0.05 * static_cast<double>(edge);
        stcf_case.fields.edges.omega_edge[edge] = 0.5 + 0.05 * static_cast<double>(edge);
    }
    return stcf_case;
}

scau::core::Vector2 center_of(
    const scau::mesh::Mesh& mesh,
    const std::string& cell_id) {
    const auto nodes = scau::mesh::node_lookup(mesh.nodes);
    for (const auto& cell : mesh.cells) {
        if (cell.id == cell_id) {
            return scau::mesh::cell_center(cell, nodes);
        }
    }
    throw std::invalid_argument("cell not found");
}

TEST(StcfCaseLoader, PreservesMixedTopologyEdgeOrderAndFields) {
    const auto stcf_case = make_mixed_case();
    const auto loaded = load_surface2d_case(stcf_case);

    ASSERT_EQ(loaded.mesh.nodes.size(), 5U);
    ASSERT_EQ(loaded.mesh.cells.size(), 2U);
    ASSERT_EQ(loaded.mesh.edges.size(), 6U);
    EXPECT_EQ(loaded.mesh.cells[0].cell_type, scau::mesh::CellType::Quadrilateral);
    EXPECT_EQ(loaded.mesh.cells[1].cell_type, scau::mesh::CellType::Triangle);
    for (std::size_t edge = 0; edge < loaded.mesh.edges.size(); ++edge) {
        EXPECT_EQ(loaded.mesh.edges[edge].id, "E" + std::to_string(edge));
        EXPECT_EQ(loaded.dpm_fields.edges[edge].phi_e_n,
                  stcf_case.fields.edges.phi_e_n[edge]);
        EXPECT_EQ(loaded.dpm_fields.edges[edge].omega_edge,
                  stcf_case.fields.edges.omega_edge[edge]);
    }
    EXPECT_EQ(loaded.source_fields.manning_n, stcf_case.fields.cells.manning_n);
    EXPECT_EQ(loaded.bed_elevations, stcf_case.fields.cells.z_b);
}

TEST(StcfCaseLoader, NormalizesClockwiseFaces) {
    auto stcf_case = make_mixed_case();
    stcf_case.topology.face_nodes[0] = {3, 2, 1, 0};
    stcf_case.topology.face_edges[0] = {2, 1, 0, 3};
    stcf_case.topology.face_nodes[1] = {2, 4, 1, kConnectivityFillValue};
    stcf_case.topology.face_edges[1] = {5, 4, 1, kConnectivityFillValue};

    const auto loaded = load_surface2d_case(stcf_case);

    EXPECT_EQ(loaded.mesh.cells[0].node_ids,
              (std::vector<std::string>{"N0", "N1", "N2", "N3"}));
    EXPECT_EQ(loaded.mesh.cells[1].node_ids,
              (std::vector<std::string>{"N1", "N4", "N2"}));
}

TEST(StcfCaseLoader, NormalsPointLeftToRightOrOutward) {
    const auto loaded = load_surface2d_case(make_mixed_case());
    for (const auto& edge : loaded.mesh.edges) {
        const auto left = center_of(loaded.mesh, *edge.left_cell);
        const double left_side = (left.x - edge.midpoint.x) * edge.normal.x
            + (left.y - edge.midpoint.y) * edge.normal.y;
        EXPECT_LT(left_side, 0.0);
        if (edge.right_cell.has_value()) {
            const auto right = center_of(loaded.mesh, *edge.right_cell);
            const double right_side = (right.x - edge.midpoint.x) * edge.normal.x
                + (right.y - edge.midpoint.y) * edge.normal.y;
            EXPECT_GT(right_side, 0.0);
        }
    }
}

TEST(StcfCaseLoader, RejectsZeroAreaGeometry) {
    auto stcf_case = make_mixed_case();
    for (auto& y : stcf_case.topology.node_y) {
        y = 0.0;
    }
    EXPECT_THROW(static_cast<void>(load_surface2d_case(stcf_case)), std::invalid_argument);
}

TEST(StcfCaseLoader, LoadsStrictNetcdfFile) {
    const auto path = std::filesystem::path(::testing::TempDir()) / "loaded_case.stcf.nc";
    scau::stcf::write_stcf_case(path, make_mixed_case());

    const auto loaded = load_surface2d_case(path);

    EXPECT_EQ(loaded.mesh.nodes.size(), 5U);
    EXPECT_EQ(loaded.mesh.cells.size(), 2U);
    EXPECT_EQ(loaded.mesh.edges.size(), 6U);
}

TEST(StcfCaseLoader, LoadedSlopingBedLakeAtRestRemainsBalanced) {
    const auto loaded = load_surface2d_case(make_mixed_case());
    auto state = scau::surface2d::SurfaceState::for_mesh(loaded.mesh);
    constexpr double eta = 1.0;
    for (std::size_t cell = 0; cell < state.cells.size(); ++cell) {
        state.cells[cell].conserved.h = eta - loaded.bed_elevations[cell];
        state.cells[cell].eta = eta;
    }
    const auto initial = state;
    const auto boundary = scau::surface2d::BoundaryConditions::for_mesh(loaded.mesh);
    const auto geometry = scau::surface2d::GeometryCache::for_mesh(loaded.mesh);
    const scau::surface2d::StepConfig config{
        .dt = 0.01,
        .cfl_safety = 0.45,
        .c_rollback = 10.0,
    };

    for (int step = 0; step < 10; ++step) {
        const auto diagnostics = scau::surface2d::advance_one_step_cpu(
            loaded.mesh,
            state,
            config,
            loaded.dpm_fields,
            boundary,
            loaded.source_fields,
            geometry);
        ASSERT_FALSE(diagnostics.rollback_required);
    }

    for (std::size_t cell = 0; cell < state.cells.size(); ++cell) {
        EXPECT_NEAR(state.cells[cell].conserved.h, initial.cells[cell].conserved.h, 1.0e-12);
        EXPECT_NEAR(state.cells[cell].conserved.hu, 0.0, 1.0e-12);
        EXPECT_NEAR(state.cells[cell].conserved.hv, 0.0, 1.0e-12);
    }
}

}  // namespace

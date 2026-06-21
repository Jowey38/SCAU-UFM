#include <cstddef>
#include <stdexcept>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/geometry/cache.hpp"
#include "surface2d/source_terms/fields.hpp"
#include "surface2d/state/state.hpp"
#include "surface2d/time_integration/step.hpp"

TEST(GeometryCache, AreasMatchMeshGeometry) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto cache = scau::surface2d::GeometryCache::for_mesh(mesh);
    const auto nodes = scau::mesh::node_lookup(mesh.nodes);

    ASSERT_EQ(cache.cell_areas.size(), mesh.cells.size());
    for (std::size_t index = 0; index < mesh.cells.size(); ++index) {
        EXPECT_EQ(cache.cell_areas[index], scau::mesh::cell_area(mesh.cells[index], nodes));
    }
}

TEST(GeometryCache, AdjacencyMatchesEdgeCellIds) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto cache = scau::surface2d::GeometryCache::for_mesh(mesh);

    ASSERT_EQ(cache.edge_cells.size(), mesh.edges.size());
    for (std::size_t edge_index = 0; edge_index < mesh.edges.size(); ++edge_index) {
        const auto& edge = mesh.edges[edge_index];
        const auto& adjacency = cache.edge_cells[edge_index];
        EXPECT_EQ(adjacency.left.has_value(), edge.left_cell.has_value());
        EXPECT_EQ(adjacency.right.has_value(), edge.right_cell.has_value());
        if (adjacency.left.has_value()) {
            EXPECT_EQ(mesh.cells[*adjacency.left].id, *edge.left_cell);
        }
        if (adjacency.right.has_value()) {
            EXPECT_EQ(mesh.cells[*adjacency.right].id, *edge.right_cell);
        }
    }
}

TEST(GeometryCache, MismatchedCacheFailsClosed) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto cache = scau::surface2d::GeometryCache::for_mesh(mesh);
    cache.cell_areas.pop_back();
    EXPECT_THROW(
        scau::surface2d::validate_geometry_cache_matches_mesh(cache, mesh),
        std::invalid_argument);
}

TEST(GeometryCache, CachedStepMatchesUncachedStep) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state_uncached = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    auto state_cached = state_uncached;
    state_uncached.cells[0].conserved.h = 1.5;
    state_cached.cells[0].conserved.h = 1.5;

    const auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    const auto boundary = scau::surface2d::BoundaryConditions::for_mesh(mesh);
    const auto sources = scau::surface2d::SourceTermFields::for_mesh(mesh);
    const auto geometry = scau::surface2d::GeometryCache::for_mesh(mesh);
    const scau::surface2d::StepConfig config{.dt = 0.01, .cfl_safety = 0.45, .c_rollback = 100.0, .h_min = 1.0e-8};

    const auto diag_uncached = scau::surface2d::advance_one_step_cpu(
        mesh, state_uncached, config, dpm_fields, boundary, sources);
    const auto diag_cached = scau::surface2d::advance_one_step_cpu(
        mesh, state_cached, config, dpm_fields, boundary, sources, geometry);

    EXPECT_EQ(diag_uncached.max_cell_cfl, diag_cached.max_cell_cfl);
    EXPECT_EQ(diag_uncached.rollback_required, diag_cached.rollback_required);
    ASSERT_EQ(state_uncached.cells.size(), state_cached.cells.size());
    for (std::size_t index = 0; index < state_uncached.cells.size(); ++index) {
        EXPECT_EQ(state_uncached.cells[index].conserved.h, state_cached.cells[index].conserved.h);
        EXPECT_EQ(state_uncached.cells[index].conserved.hu, state_cached.cells[index].conserved.hu);
        EXPECT_EQ(state_uncached.cells[index].conserved.hv, state_cached.cells[index].conserved.hv);
    }
}

TEST(GeometryCache, LegacyOverloadStillAdvances) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    state.cells[0].conserved.h = 1.2;
    const auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    const scau::surface2d::StepConfig config{.dt = 0.01, .cfl_safety = 0.45, .c_rollback = 100.0, .h_min = 1.0e-8};

    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields);
    EXPECT_FALSE(diagnostics.rollback_required);
    EXPECT_EQ(diagnostics.cell_count, mesh.cells.size());
}

TEST(GeometryCache, OutOfRangeAdjacencyFailsClosed) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto cache = scau::surface2d::GeometryCache::for_mesh(mesh);
    cache.edge_cells[0].left = mesh.cells.size();
    EXPECT_THROW(
        scau::surface2d::validate_geometry_cache_matches_mesh(cache, mesh),
        std::invalid_argument);
}

TEST(GeometryCache, EmptyMeshStepFailsClosed) {
    const scau::mesh::Mesh empty_mesh{};
    scau::surface2d::SurfaceState empty_state{};
    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 1.0, .h_min = 1.0e-8};
    EXPECT_THROW(
        static_cast<void>(scau::surface2d::advance_one_step_cpu(empty_mesh, empty_state, config)),
        std::invalid_argument);
}

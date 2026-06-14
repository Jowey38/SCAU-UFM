#include <stdexcept>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/source_terms/runoff/fields.hpp"

namespace {

using scau::surface2d::RoofDrainageMap;
using scau::surface2d::RunoffFields;
using scau::surface2d::RunoffMethod;
using scau::surface2d::RunoffState;

}  // namespace

TEST(RunoffFields, ForMeshHasNeutralDefaults) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto fields = RunoffFields::for_mesh(mesh);

    ASSERT_EQ(fields.pervious_fraction.size(), mesh.cells.size());
    EXPECT_EQ(fields.method, RunoffMethod::GreenAmptMeinLarson);
    for (std::size_t i = 0; i < mesh.cells.size(); ++i) {
        EXPECT_EQ(fields.pervious_fraction[i], 1.0);
        EXPECT_EQ(fields.impervious_fraction[i], 0.0);
        EXPECT_EQ(fields.roof_fraction[i], 0.0);
        EXPECT_EQ(fields.initial_abstraction_capacity[i], 0.0);
        EXPECT_EQ(fields.depression_storage_capacity[i], 0.0);
        EXPECT_EQ(fields.roof_abstraction_capacity[i], 0.0);
        EXPECT_EQ(fields.roof_storage_capacity[i], 0.0);
        EXPECT_EQ(fields.soil_type[i], 0U);
    }
}

TEST(RunoffState, ForMeshPondingTimeIsMinusOne) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto state = RunoffState::for_mesh(mesh);

    ASSERT_EQ(state.cumulative_infiltration.size(), mesh.cells.size());
    for (std::size_t i = 0; i < mesh.cells.size(); ++i) {
        EXPECT_EQ(state.cumulative_infiltration[i], 0.0);
        EXPECT_EQ(state.ponding_time[i], -1.0);
        EXPECT_EQ(state.abstraction_filled[i], 0.0);
        EXPECT_EQ(state.depression_storage_filled[i], 0.0);
        EXPECT_EQ(state.roof_abstraction_filled[i], 0.0);
        EXPECT_EQ(state.roof_storage_filled[i], 0.0);
        EXPECT_EQ(state.roof_pending_volume[i], 0.0);
    }
}

TEST(RoofDrainageMap, ForMeshIndicesAreMinusOne) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto map = RoofDrainageMap::for_mesh(mesh);

    ASSERT_EQ(map.swmm_node_index.size(), mesh.cells.size());
    for (std::size_t i = 0; i < mesh.cells.size(); ++i) {
        EXPECT_EQ(map.roof_drain_capacity[i], 0.0);
        EXPECT_EQ(map.swmm_node_index[i], -1);
        EXPECT_EQ(map.roof_overflow_to_surface_cell_index[i], -1);
        EXPECT_EQ(map.roof_to_surface_fraction[i], 0.0);
    }
}

TEST(RunoffFields, MismatchedSizeFailsClosed) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto fields = RunoffFields::for_mesh(mesh);
    fields.roof_fraction.pop_back();
    EXPECT_THROW(scau::surface2d::validate_runoff_fields_match_mesh(fields, mesh), std::invalid_argument);
}

TEST(RunoffState, MismatchedSizeFailsClosed) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = RunoffState::for_mesh(mesh);
    state.ponding_time.pop_back();
    EXPECT_THROW(scau::surface2d::validate_runoff_state_match_mesh(state, mesh), std::invalid_argument);
}

TEST(RoofDrainageMap, MismatchedSizeFailsClosed) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto map = RoofDrainageMap::for_mesh(mesh);
    map.swmm_node_index.pop_back();
    EXPECT_THROW(scau::surface2d::validate_roof_drainage_map_match_mesh(map, mesh), std::invalid_argument);
}

TEST(RunoffFields, OversizedArrayFailsClosed) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto fields = RunoffFields::for_mesh(mesh);
    fields.roof_fraction.push_back(0.0);
    EXPECT_THROW(scau::surface2d::validate_runoff_fields_match_mesh(fields, mesh), std::invalid_argument);
}

TEST(RunoffState, OversizedArrayFailsClosed) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = RunoffState::for_mesh(mesh);
    state.ponding_time.push_back(0.0);
    EXPECT_THROW(scau::surface2d::validate_runoff_state_match_mesh(state, mesh), std::invalid_argument);
}

TEST(RoofDrainageMap, OversizedArrayFailsClosed) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto map = RoofDrainageMap::for_mesh(mesh);
    map.swmm_node_index.push_back(-1);
    EXPECT_THROW(scau::surface2d::validate_roof_drainage_map_match_mesh(map, mesh), std::invalid_argument);
}

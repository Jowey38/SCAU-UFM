#include <stdexcept>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/source_terms/runoff/roof_step.hpp"

namespace {
using scau::surface2d::RoofStepInputs;
}  // namespace

TEST(RoofStepInputs, ForMeshHasNeutralDefaults) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto in = RoofStepInputs::for_mesh(mesh);
    ASSERT_EQ(in.map.swmm_node_index.size(), mesh.cells.size());
    for (std::size_t i = 0; i < mesh.cells.size(); ++i) {
        EXPECT_EQ(in.map.swmm_node_index[i], -1);
    }
    ASSERT_TRUE(static_cast<bool>(in.accept));
}

TEST(RoofStepInputs, NullAcceptFailsClosed) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto in = RoofStepInputs::for_mesh(mesh);
    in.accept = nullptr;
    EXPECT_THROW(scau::surface2d::validate_roof_step_inputs_match_mesh(in, mesh), std::invalid_argument);
}

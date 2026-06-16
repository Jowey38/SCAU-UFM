#include <stdexcept>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/source_terms/runoff/step_inputs.hpp"

namespace {
using scau::surface2d::RunoffStepInputs;
}  // namespace

TEST(RunoffStepInputs, ForMeshHasNeutralDefaults) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto in = RunoffStepInputs::for_mesh(mesh);
    ASSERT_EQ(in.rainfall_rate.size(), mesh.cells.size());
    ASSERT_EQ(in.fields.pervious_fraction.size(), mesh.cells.size());
    ASSERT_FALSE(in.lut.entries.empty());
    EXPECT_GT(in.f_inf_floor, 0.0);
    for (std::size_t i = 0; i < mesh.cells.size(); ++i) {
        EXPECT_EQ(in.rainfall_rate[i], 0.0);
        EXPECT_EQ(in.fields.pervious_fraction[i], 1.0);
    }
}

TEST(RunoffStepInputs, MismatchedRainfallSizeFailsClosed) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto in = RunoffStepInputs::for_mesh(mesh);
    in.rainfall_rate.pop_back();
    EXPECT_THROW(scau::surface2d::validate_runoff_step_inputs_match_mesh(in, mesh), std::invalid_argument);
}

#include <gtest/gtest.h>

#include "surface2d/reconstruction/hydrostatic.hpp"
#include "surface2d/state/state.hpp"

TEST(HydrostaticReconstruction, EqualEtaZeroVelocityPreservesDepth) {
    const scau::surface2d::CellState left{.conserved = {.h = 1.0, .hu = 0.0, .hv = 0.0}, .eta = 1.0};
    const scau::surface2d::CellState right{.conserved = {.h = 1.0, .hu = 0.0, .hv = 0.0}, .eta = 1.0};

    const auto pair = scau::surface2d::reconstruct_hydrostatic_pair(left, right);

    EXPECT_EQ(pair.left.conserved.h, 1.0);
    EXPECT_EQ(pair.right.conserved.h, 1.0);
    EXPECT_EQ(pair.left.conserved.hu, 0.0);
    EXPECT_EQ(pair.left.conserved.hv, 0.0);
    EXPECT_EQ(pair.right.conserved.hu, 0.0);
    EXPECT_EQ(pair.right.conserved.hv, 0.0);
    EXPECT_EQ(pair.left.eta, 1.0);
    EXPECT_EQ(pair.right.eta, 1.0);
}

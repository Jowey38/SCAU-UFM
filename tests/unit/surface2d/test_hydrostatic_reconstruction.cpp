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

TEST(HydrostaticReconstruction, AudusseClipsDepthAtHigherNeighbourBedAndPreservesVelocity) {
    const scau::surface2d::CellState left{.conserved = {.h = 2.0, .hu = 4.0, .hv = -2.0}, .eta = 2.0};
    const scau::surface2d::CellState right{.conserved = {.h = 0.3, .hu = 0.15, .hv = 0.075}, .eta = 1.5};

    const auto pair = scau::surface2d::reconstruct_hydrostatic_pair(left, right);

    // z_b_L = eta_L - h_L = 0 ; z_b_R = 1.5 - 0.3 = 1.2 ; z_b* = 1.2.
    // h_L* = max(0, 2.0 - 1.2) = 0.8 ; h_R* = max(0, 1.5 - 1.2) = 0.3.
    EXPECT_DOUBLE_EQ(pair.left.conserved.h, 0.8);
    EXPECT_DOUBLE_EQ(pair.right.conserved.h, 0.3);
    // Velocity (u = hu/h) recovered from the ORIGINAL cell, applied to h*:
    // u_L = 2.0 -> hu_L* = 0.8*2.0 = 1.6 ; v_L = -1.0 -> hv_L* = -0.8.
    EXPECT_DOUBLE_EQ(pair.left.conserved.hu, 1.6);
    EXPECT_DOUBLE_EQ(pair.left.conserved.hv, -0.8);
    // u_R = 0.5 -> hu_R* = 0.3*0.5 = 0.15 ; v_R = 0.25 -> hv_R* = 0.075.
    EXPECT_DOUBLE_EQ(pair.right.conserved.hu, 0.15);
    EXPECT_DOUBLE_EQ(pair.right.conserved.hv, 0.075);
}

#include <gtest/gtest.h>

#include "surface2d/dpm/edge_classification.hpp"

TEST(EdgeClassification, RegularEdgePassesDefaults) {
    const auto c = scau::surface2d::classify_edge(1.0, 1.0);
    EXPECT_EQ(c.block_class, scau::surface2d::EdgeBlockClass::Regular);
    EXPECT_FALSE(c.advective_flux_zeroed);
    EXPECT_TRUE(c.wb_pairing_assembled);
}

TEST(EdgeClassification, HardBlockWhenOmegaZero) {
    const auto c = scau::surface2d::classify_edge(0.0, 1.0);
    EXPECT_EQ(c.block_class, scau::surface2d::EdgeBlockClass::HardBlock);
    EXPECT_TRUE(c.advective_flux_zeroed);
    EXPECT_FALSE(c.wb_pairing_assembled);
}

TEST(EdgeClassification, HardBlockWhenOmegaNegative) {
    const auto c = scau::surface2d::classify_edge(-0.5, 1.0);
    EXPECT_EQ(c.block_class, scau::surface2d::EdgeBlockClass::HardBlock);
}

TEST(EdgeClassification, SoftBlockWhenOmegaBelowEpsilon) {
    // 0 < omega < EpsilonOmega.
    const auto c = scau::surface2d::classify_edge(
        scau::surface2d::EpsilonOmega * 0.5, 1.0);
    EXPECT_EQ(c.block_class, scau::surface2d::EdgeBlockClass::SoftBlock);
    EXPECT_TRUE(c.advective_flux_zeroed);
    EXPECT_TRUE(c.wb_pairing_assembled);
}

TEST(EdgeClassification, SoftBlockWhenPhiEnBelowMin) {
    const auto c = scau::surface2d::classify_edge(1.0, scau::surface2d::PhiEdgeMin * 0.5);
    EXPECT_EQ(c.block_class, scau::surface2d::EdgeBlockClass::SoftBlock);
    EXPECT_TRUE(c.advective_flux_zeroed);
    EXPECT_TRUE(c.wb_pairing_assembled);
}

TEST(EdgeClassification, BoundaryValuesAreRegular) {
    // At exactly the thresholds the edge is regular (>= is inclusive).
    const auto c = scau::surface2d::classify_edge(
        scau::surface2d::EpsilonOmega, scau::surface2d::PhiEdgeMin);
    EXPECT_EQ(c.block_class, scau::surface2d::EdgeBlockClass::Regular);
}

TEST(EdgeClassification, SpecThresholdValues) {
    EXPECT_EQ(scau::surface2d::EpsilonOmega, 1.0e-4);
    EXPECT_EQ(scau::surface2d::PhiEdgeMin, 0.01);
}

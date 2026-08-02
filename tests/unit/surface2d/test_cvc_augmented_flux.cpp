#include <cmath>
#include <limits>

#include <gtest/gtest.h>

#include "surface2d/dpm/cvc_augmented_flux.hpp"

namespace {

using scau::surface2d::cvc_side_fluxes;
using scau::surface2d::EdgeFlux;

TEST(CvcAugmentedFlux, UniformPhiReturnsBaselineFluxExactly) {
    const EdgeFlux baseline{
        .mass = 2.0,
        .momentum_x = 3.0,
        .momentum_y = -4.0,
    };
    const auto fluxes = cvc_side_fluxes(baseline, 0.7, 0.7);

    EXPECT_EQ(fluxes.left_mass, baseline.mass);
    EXPECT_EQ(fluxes.right_mass, baseline.mass);
    EXPECT_EQ(fluxes.left_momentum_x, baseline.momentum_x);
    EXPECT_EQ(fluxes.right_momentum_x, baseline.momentum_x);
    EXPECT_EQ(fluxes.left_momentum_y, baseline.momentum_y);
    EXPECT_EQ(fluxes.right_momentum_y, baseline.momentum_y);
    EXPECT_FALSE(fluxes.applied);
}

TEST(CvcAugmentedFlux, PositiveFluxUsesLeftStoragePorosityAndCloses) {
    const EdgeFlux baseline{
        .mass = 2.0,
        .momentum_x = 3.0,
        .momentum_y = 4.0,
    };
    const auto fluxes = cvc_side_fluxes(baseline, 1.0, 0.4);

    EXPECT_DOUBLE_EQ(fluxes.left_mass, 2.0);
    EXPECT_DOUBLE_EQ(fluxes.right_mass, 5.0);
    EXPECT_DOUBLE_EQ(-1.0 * fluxes.left_mass + 0.4 * fluxes.right_mass, 0.0);
    EXPECT_DOUBLE_EQ(-1.0 * fluxes.left_momentum_x + 0.4 * fluxes.right_momentum_x, 0.0);
    EXPECT_DOUBLE_EQ(-1.0 * fluxes.left_momentum_y + 0.4 * fluxes.right_momentum_y, 0.0);
    EXPECT_DOUBLE_EQ(fluxes.storage_residual_after, 0.0);
    EXPECT_TRUE(fluxes.applied);
}

TEST(CvcAugmentedFlux, NegativeFluxUsesRightStoragePorosityAndCloses) {
    const EdgeFlux baseline{
        .mass = -2.0,
        .momentum_x = -3.0,
        .momentum_y = -4.0,
    };
    const auto fluxes = cvc_side_fluxes(baseline, 1.0, 0.4);

    EXPECT_DOUBLE_EQ(fluxes.left_mass, -0.8);
    EXPECT_DOUBLE_EQ(fluxes.right_mass, -2.0);
    EXPECT_DOUBLE_EQ(-1.0 * fluxes.left_mass + 0.4 * fluxes.right_mass, 0.0);
    EXPECT_DOUBLE_EQ(-1.0 * fluxes.left_momentum_x + 0.4 * fluxes.right_momentum_x, 0.0);
    EXPECT_DOUBLE_EQ(-1.0 * fluxes.left_momentum_y + 0.4 * fluxes.right_momentum_y, 0.0);
    EXPECT_DOUBLE_EQ(fluxes.storage_residual_after, 0.0);
    EXPECT_TRUE(fluxes.applied);
}

TEST(CvcAugmentedFlux, LakeAtRestIsZeroAndNotApplied) {
    const auto fluxes = cvc_side_fluxes(EdgeFlux{}, 1.0, 0.4);

    EXPECT_DOUBLE_EQ(fluxes.left_mass, 0.0);
    EXPECT_DOUBLE_EQ(fluxes.right_mass, 0.0);
    EXPECT_DOUBLE_EQ(fluxes.storage_residual_after, 0.0);
    EXPECT_FALSE(fluxes.applied);
}

TEST(CvcAugmentedFlux, NearDryLowPorositySideRemainsFiniteAndCloses) {
    const EdgeFlux baseline{
        .mass = 1.0e-10,
        .momentum_x = 2.0e-12,
        .momentum_y = -3.0e-12,
    };
    const auto fluxes = cvc_side_fluxes(baseline, 1.0, 1.0e-6);

    EXPECT_TRUE(std::isfinite(fluxes.right_mass));
    EXPECT_TRUE(std::isfinite(fluxes.right_momentum_x));
    EXPECT_TRUE(std::isfinite(fluxes.right_momentum_y));
    EXPECT_NEAR(-fluxes.left_mass + 1.0e-6 * fluxes.right_mass, 0.0, 1.0e-24);
    EXPECT_NEAR(
        -fluxes.left_momentum_y + 1.0e-6 * fluxes.right_momentum_y,
        0.0,
        1.0e-24);
}

TEST(CvcAugmentedFlux, ObliqueMomentumClosesForBothFluxDirections) {
    for (const double sign : {1.0, -1.0}) {
        const EdgeFlux baseline{
            .mass = sign * 0.25,
            .momentum_x = sign * 0.125,
            .momentum_y = -sign * 0.375,
        };
        const auto fluxes = cvc_side_fluxes(baseline, 0.9, 0.2);
        EXPECT_NEAR(
            -0.9 * fluxes.left_mass + 0.2 * fluxes.right_mass,
            0.0,
            1.0e-15);
        EXPECT_NEAR(
            -0.9 * fluxes.left_momentum_x + 0.2 * fluxes.right_momentum_x,
            0.0,
            1.0e-15);
        EXPECT_NEAR(
            -0.9 * fluxes.left_momentum_y + 0.2 * fluxes.right_momentum_y,
            0.0,
            1.0e-15);
    }
}

TEST(CvcAugmentedFlux, RejectsInvalidPorosity) {
    const EdgeFlux baseline{.mass = 1.0};
    EXPECT_THROW(static_cast<void>(cvc_side_fluxes(baseline, 0.0, 0.4)), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(cvc_side_fluxes(baseline, 1.0, -0.1)), std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(cvc_side_fluxes(
            baseline, std::numeric_limits<double>::quiet_NaN(), 0.4)),
        std::invalid_argument);
}

}  // namespace

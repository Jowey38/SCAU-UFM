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

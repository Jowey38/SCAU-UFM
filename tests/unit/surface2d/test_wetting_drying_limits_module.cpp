#include <stdexcept>

#include <gtest/gtest.h>

#include "surface2d/wetting_drying/limits.hpp"

TEST(WettingDryingLimitsModule, NonnegativeDepthClampPreservesPositiveIncrement) {
    EXPECT_NEAR(scau::surface2d::nonnegative_depth_after_increment(0.25, 0.75), 1.0, 1.0e-15);
    EXPECT_NEAR(scau::surface2d::nonnegative_depth_after_increment(0.25, -0.1), 0.15, 1.0e-15);
}

TEST(WettingDryingLimitsModule, NonnegativeDepthClampStopsAtZero) {
    EXPECT_EQ(scau::surface2d::nonnegative_depth_after_increment(0.25, -10.0), 0.0);
}

TEST(WettingDryingLimitsModule, DryCellMomentumLimitZeroesDryMomentumOnly) {
    const scau::surface2d::ConservedState dry{.h = 1.0e-9, .hu = 2.0, .hv = -3.0};
    const auto dry_limited = scau::surface2d::apply_dry_cell_momentum_limit(dry, 1.0e-8);
    EXPECT_EQ(dry_limited.hu, 0.0);
    EXPECT_EQ(dry_limited.hv, 0.0);

    const scau::surface2d::ConservedState wet{.h = 1.0, .hu = 2.0, .hv = -3.0};
    const auto wet_limited = scau::surface2d::apply_dry_cell_momentum_limit(wet, 1.0e-8);
    EXPECT_EQ(wet_limited.hu, wet.hu);
    EXPECT_EQ(wet_limited.hv, wet.hv);
}

TEST(WettingDryingLimitsModule, InvalidInputsFailClosed) {
    EXPECT_THROW(
        static_cast<void>(scau::surface2d::nonnegative_depth_after_increment(-1.0, 0.0)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::surface2d::apply_dry_cell_momentum_limit(
            scau::surface2d::ConservedState{.h = -1.0, .hu = 0.0, .hv = 0.0},
            1.0e-8)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::surface2d::apply_dry_cell_momentum_limit(
            scau::surface2d::ConservedState{.h = 1.0, .hu = 0.0, .hv = 0.0},
            -1.0)),
        std::invalid_argument);
}

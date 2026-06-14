#include <stdexcept>

#include <gtest/gtest.h>

#include "surface2d/source_terms/runoff/green_ampt.hpp"
#include "surface2d/source_terms/runoff/soil_params.hpp"

namespace {

using scau::surface2d::green_ampt_infiltration_step;
using scau::surface2d::SoilParams;

// k_s=1e-5, psi_f=0.1, theta_s=0.4, theta_i=0.1 -> delta_theta=0.3.
// At F_eval=1e-4: f_c = 1e-5 * (1 + 0.1*0.3/1e-4) = 1e-5 * 301 = 3.01e-3 m/s.
SoilParams sample_soil() {
    return SoilParams{.k_s = 1.0e-5, .psi_f = 0.1, .theta_s = 0.4, .theta_i = 0.1};
}

}  // namespace

TEST(GreenAmptStep, SupplyLimitedNoPonding) {
    // available (1e-3) < potential (3.01e-3): all infiltrates, no ponding.
    const auto step = green_ampt_infiltration_step(sample_soil(), 0.0, 1.0e-3, 1.0, 1.0e-4);
    EXPECT_NEAR(step.infiltrated_depth, 1.0e-3, 1.0e-12);
    EXPECT_NEAR(step.cumulative_infiltration, 1.0e-3, 1.0e-12);
    EXPECT_FALSE(step.ponding_started);
}

TEST(GreenAmptStep, CapacityLimitedPonding) {
    // available (1e-2) > potential (3.01e-3): capacity-limited, ponding starts.
    const auto step = green_ampt_infiltration_step(sample_soil(), 0.0, 1.0e-2, 1.0, 1.0e-4);
    EXPECT_NEAR(step.infiltrated_depth, 3.01e-3, 1.0e-9);
    EXPECT_NEAR(step.cumulative_infiltration, 3.01e-3, 1.0e-9);
    EXPECT_TRUE(step.ponding_started);
}

TEST(GreenAmptStep, CapacityDropsAsCumulativeGrows) {
    // At F_before=0.3: f_c = 1e-5*(1 + 0.1*0.3/0.3) = 1e-5*1.1 = 1.1e-5 m/s.
    const auto step = green_ampt_infiltration_step(sample_soil(), 0.3, 1.0e-2, 1.0, 1.0e-4);
    EXPECT_NEAR(step.infiltrated_depth, 1.1e-5, 1.0e-12);
    EXPECT_NEAR(step.cumulative_infiltration, 0.3 + 1.1e-5, 1.0e-12);
    EXPECT_TRUE(step.ponding_started);
}

TEST(GreenAmptStep, ZeroAvailableInfiltratesNothing) {
    const auto step = green_ampt_infiltration_step(sample_soil(), 0.05, 0.0, 1.0, 1.0e-4);
    EXPECT_EQ(step.infiltrated_depth, 0.0);
    EXPECT_NEAR(step.cumulative_infiltration, 0.05, 1.0e-15);
    EXPECT_FALSE(step.ponding_started);
}

TEST(GreenAmptStep, InvalidInputsFailClosed) {
    EXPECT_THROW(
        static_cast<void>(green_ampt_infiltration_step(sample_soil(), -1.0, 1.0e-3, 1.0, 1.0e-4)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(green_ampt_infiltration_step(sample_soil(), 0.0, -1.0e-3, 1.0, 1.0e-4)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(green_ampt_infiltration_step(sample_soil(), 0.0, 1.0e-3, 0.0, 1.0e-4)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(green_ampt_infiltration_step(sample_soil(), 0.0, 1.0e-3, 1.0, 0.0)),
        std::invalid_argument);
}

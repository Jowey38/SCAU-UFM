#include <cmath>
#include <stdexcept>

#include <gtest/gtest.h>

#include "coupling/core/state.hpp"

// Head-driven exchange intent flow (spec section 7.5 RiverExchangePoint and
// the flow-regime catalogue: free weir, submerged weir with Villemonte
// correction, orifice, smoothstep incipient transition). The formula only
// produces the INTENT magnitude and direction; Q_limit arbitration, deficit
// and conservation stay in the existing core pipeline.

namespace {

using scau::coupling::core::ExchangeFlowGeometry;
using scau::coupling::core::HeadDrivenExchangeRegime;
using scau::coupling::core::compute_head_driven_exchange_flow;

constexpr double kGravity = 9.80665;

ExchangeFlowGeometry make_weir() {
    return ExchangeFlowGeometry{
        .crest_level = 10.0,
        .exchange_width = 2.0,
        .weir_coefficient = 1.7,
        .orifice_coefficient = 0.6,
        .orifice_area = 0.0,
        .smooth_depth = 0.0,  // disabled unless a test enables it
    };
}

}  // namespace

TEST(CouplingHeadDrivenExchange, BothLevelsBelowCrestYieldZero) {
    const auto flow = compute_head_driven_exchange_flow(9.5, 9.8, make_weir());

    EXPECT_DOUBLE_EQ(flow.q_surface_to_engine, 0.0);
    EXPECT_EQ(flow.regime, HeadDrivenExchangeRegime::none);
}

TEST(CouplingHeadDrivenExchange, FreeWeirSurfaceToEngine) {
    // Surface 0.4 m over crest, engine side below crest -> free weir.
    const auto flow = compute_head_driven_exchange_flow(10.4, 9.0, make_weir());

    const double expected = 1.7 * 2.0 * std::pow(0.4, 1.5);
    EXPECT_NEAR(flow.q_surface_to_engine, expected, 1.0e-12);
    EXPECT_EQ(flow.regime, HeadDrivenExchangeRegime::free_weir);
}

TEST(CouplingHeadDrivenExchange, FreeWeirEngineToSurfaceIsNegative) {
    // Engine side higher: same magnitude, opposite sign (reverse spill).
    const auto forward = compute_head_driven_exchange_flow(10.4, 9.0, make_weir());
    const auto reverse = compute_head_driven_exchange_flow(9.0, 10.4, make_weir());

    EXPECT_NEAR(reverse.q_surface_to_engine, -forward.q_surface_to_engine, 1.0e-12);
    EXPECT_EQ(reverse.regime, HeadDrivenExchangeRegime::free_weir);
}

TEST(CouplingHeadDrivenExchange, SubmergedWeirAppliesVillemonteReduction) {
    // Both sides over crest: submerged weir, Villemonte:
    // Q = Q_free * (1 - (h_down/h_up)^1.5)^0.385
    const auto flow = compute_head_driven_exchange_flow(10.4, 10.2, make_weir());

    const double q_free = 1.7 * 2.0 * std::pow(0.4, 1.5);
    const double ratio = 0.2 / 0.4;
    const double expected = q_free * std::pow(1.0 - std::pow(ratio, 1.5), 0.385);
    EXPECT_NEAR(flow.q_surface_to_engine, expected, 1.0e-12);
    EXPECT_EQ(flow.regime, HeadDrivenExchangeRegime::submerged_weir);
    EXPECT_LT(flow.q_surface_to_engine, q_free);
}

TEST(CouplingHeadDrivenExchange, EqualLevelsYieldZero) {
    const auto flow = compute_head_driven_exchange_flow(10.3, 10.3, make_weir());

    EXPECT_DOUBLE_EQ(flow.q_surface_to_engine, 0.0);
    EXPECT_EQ(flow.regime, HeadDrivenExchangeRegime::none);
}

TEST(CouplingHeadDrivenExchange, OrificeRegimeWhenAreaConfigured) {
    // gate_outlet / culvert style: orifice flow Q = C_o * A_o * sqrt(2 g dh),
    // active when an orifice area is configured and both ends are submerged.
    auto geometry = make_weir();
    geometry.orifice_area = 0.5;

    const auto flow = compute_head_driven_exchange_flow(10.4, 10.2, geometry);

    const double expected = 0.6 * 0.5 * std::sqrt(2.0 * kGravity * 0.2);
    EXPECT_NEAR(flow.q_surface_to_engine, expected, 1.0e-12);
    EXPECT_EQ(flow.regime, HeadDrivenExchangeRegime::orifice);
}

TEST(CouplingHeadDrivenExchange, SmoothstepDampsIncipientOverflow) {
    // With smooth_depth = 0.1, an upstream head of 0.05 over the crest gets
    // smoothstep(0.5) = 0.5 damping; at >= 0.1 over the crest the factor is 1.
    auto geometry = make_weir();
    geometry.smooth_depth = 0.1;

    const auto damped = compute_head_driven_exchange_flow(10.05, 9.0, geometry);
    const double q_raw = 1.7 * 2.0 * std::pow(0.05, 1.5);
    const double t = 0.5;
    const double smooth = t * t * (3.0 - 2.0 * t);
    EXPECT_NEAR(damped.q_surface_to_engine, q_raw * smooth, 1.0e-12);

    const auto full = compute_head_driven_exchange_flow(10.4, 9.0, geometry);
    EXPECT_NEAR(full.q_surface_to_engine, 1.7 * 2.0 * std::pow(0.4, 1.5), 1.0e-12);
}

TEST(CouplingHeadDrivenExchange, RejectsInvalidGeometryAndLevels) {
    {
        auto geometry = make_weir();
        geometry.exchange_width = 0.0;
        EXPECT_THROW(
            static_cast<void>(compute_head_driven_exchange_flow(10.4, 9.0, geometry)),
            std::invalid_argument);
    }
    {
        auto geometry = make_weir();
        geometry.weir_coefficient = -1.0;
        EXPECT_THROW(
            static_cast<void>(compute_head_driven_exchange_flow(10.4, 9.0, geometry)),
            std::invalid_argument);
    }
    {
        auto geometry = make_weir();
        geometry.orifice_area = -0.5;
        EXPECT_THROW(
            static_cast<void>(compute_head_driven_exchange_flow(10.4, 9.0, geometry)),
            std::invalid_argument);
    }
    {
        auto geometry = make_weir();
        geometry.crest_level = std::nan("");
        EXPECT_THROW(
            static_cast<void>(compute_head_driven_exchange_flow(10.4, 9.0, geometry)),
            std::invalid_argument);
    }
    EXPECT_THROW(
        static_cast<void>(compute_head_driven_exchange_flow(std::nan(""), 9.0, make_weir())),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(compute_head_driven_exchange_flow(10.4, std::nan(""), make_weir())),
        std::invalid_argument);
}

#include <string>

#include <gtest/gtest.h>

#include "coupling/core/state.hpp"
#include "coupling/drainage/swmm_boundary.hpp"
#include "coupling/drainage/swmm_engine.hpp"

namespace {

std::string g8_case_path() {
    return std::string(SCAU_G8_SWMM_CASE_DIR) + "/swmm_manhole_overflow.inp";
}

}  // namespace

TEST(SwmmSinglePipeSurchargeGolden, RealSwmmOverflowRequestIsLimitedAndDeficitRepaid) {
    constexpr double kDtSub = 10.0;
    constexpr double kPhiT = 0.25;
    constexpr double kDepth = 1.0;
    constexpr double kArea = 4.0;
    constexpr double kInitialVolume = 3.0;

    scau::coupling::drainage::SwmmEngine swmm;
    swmm.initialize(g8_case_path());

    const int j1 = swmm.node_index("J1");
    double q_request = 0.0;
    bool saw_positive_overflow = false;
    for (int step = 0; step < 100; ++step) {
        swmm.step(kDtSub);
        q_request = swmm.get_node_overflow(j1);
        if (q_request > 0.0 && swmm.is_surcharged(j1)) {
            saw_positive_overflow = true;
            break;
        }
    }

    ASSERT_TRUE(saw_positive_overflow);
    ASSERT_GT(q_request, 0.30);

    scau::coupling::core::CouplingState state{{{
        .volume = kInitialVolume,
        .phi_t = kPhiT,
        .h = kDepth,
        .area = kArea,
    }}};

    const auto limit = scau::coupling::core::compute_flow_limit(state.cells()[0], kDtSub);
    EXPECT_DOUBLE_EQ(limit.v_limit, 0.9 * kPhiT * kDepth * kArea);
    EXPECT_DOUBLE_EQ(limit.q_limit, limit.v_limit / kDtSub);
    ASSERT_LT(limit.q_limit, q_request);

    const auto request = scau::coupling::drainage::make_swmm_exchange_request(q_request, kDtSub);
    const auto first = state.apply_exchange(0U, request);

    EXPECT_DOUBLE_EQ(first.exchange.q_granted, limit.q_limit);
    EXPECT_DOUBLE_EQ(first.exchange.v_granted, limit.v_limit);
    EXPECT_DOUBLE_EQ(first.exchange.q_repay, 0.0);
    EXPECT_DOUBLE_EQ(first.exchange.v_repay, 0.0);
    EXPECT_NEAR(first.exchange.v_unmet, (q_request - limit.q_limit) * kDtSub, 1.0e-12);
    EXPECT_GT(first.exchange.v_unmet, 0.0);

    scau::coupling::drainage::accept_swmm_exchange_decision(swmm, j1, first.exchange, kDtSub);
    swmm.step(kDtSub);
    EXPECT_NEAR(swmm.get_node_lateral_inflow(j1), first.exchange.q_granted + first.exchange.q_repay, 1.0e-6);

    state.replay_pending();
    ASSERT_EQ(state.cells().size(), 1U);
    EXPECT_NEAR(state.cells()[0].volume, kInitialVolume - limit.v_limit, 1.0e-12);
    EXPECT_NEAR(state.cells()[0].mass_deficit_account.volume, first.exchange.v_unmet, 1.0e-12);

    const auto repay_request = scau::coupling::drainage::make_swmm_exchange_request(0.0, kDtSub);
    const auto second = state.apply_exchange(0U, repay_request);
    const auto second_limit = scau::coupling::core::compute_flow_limit(state.cells()[0], kDtSub);

    EXPECT_DOUBLE_EQ(second.exchange.q_granted, 0.0);
    EXPECT_DOUBLE_EQ(second.exchange.v_granted, 0.0);
    EXPECT_DOUBLE_EQ(second.exchange.q_repay, second_limit.q_limit);
    EXPECT_DOUBLE_EQ(second.exchange.v_repay, second_limit.v_limit);
    EXPECT_DOUBLE_EQ(second.exchange.v_unmet, 0.0);

    scau::coupling::drainage::accept_swmm_exchange_decision(swmm, j1, second.exchange, kDtSub);
    swmm.step(kDtSub);
    EXPECT_NEAR(swmm.get_node_lateral_inflow(j1), second.exchange.q_granted + second.exchange.q_repay, 1.0e-6);

    state.replay_pending();
    EXPECT_NEAR(state.cells()[0].volume, kInitialVolume - limit.v_limit - second_limit.v_limit, 1.0e-12);
    EXPECT_NEAR(state.cells()[0].mass_deficit_account.volume, first.exchange.v_unmet - second_limit.v_limit, 1.0e-12);

    swmm.finalize();
}

#include <gtest/gtest.h>

#include "coupling/core/state.hpp"
#include "coupling/drainage/swmm_boundary.hpp"

TEST(SwmmSinglePipeSurchargeGolden, SurchargeDrainRequestIsLimitedAndDeficitRepaid) {
    constexpr int kNodeId = 7;
    constexpr double kDtSub = 10.0;
    constexpr double kPhiT = 0.5;
    constexpr double kDepth = 2.0;
    constexpr double kArea = 4.0;
    constexpr double kRequestedQ = 1.0;

    scau::coupling::drainage::MockSwmmEngine swmm;
    swmm.initialize("mock-single-pipe-surcharge.inp");
    swmm.set_node_surcharge_fixture(kNodeId, true);

    scau::coupling::core::CouplingState state{{{
        .volume = kPhiT * kDepth * kArea,
        .phi_t = kPhiT,
        .h = kDepth,
        .area = kArea,
    }}};

    const auto limit = scau::coupling::core::compute_flow_limit(state.cells()[0], kDtSub);
    EXPECT_DOUBLE_EQ(limit.v_limit, 0.9 * kPhiT * kDepth * kArea);
    EXPECT_DOUBLE_EQ(limit.q_limit, limit.v_limit / kDtSub);
    ASSERT_TRUE(swmm.is_surcharged(kNodeId));

    const auto request = scau::coupling::drainage::make_swmm_exchange_request(kRequestedQ, kDtSub);
    const auto first = state.apply_exchange(0U, request);

    EXPECT_DOUBLE_EQ(first.exchange.q_granted, limit.q_limit);
    EXPECT_DOUBLE_EQ(first.exchange.v_granted, limit.v_limit);
    EXPECT_DOUBLE_EQ(first.exchange.q_repay, 0.0);
    EXPECT_DOUBLE_EQ(first.exchange.v_repay, 0.0);
    EXPECT_DOUBLE_EQ(first.exchange.v_unmet, (kRequestedQ - limit.q_limit) * kDtSub);

    scau::coupling::drainage::accept_swmm_exchange_decision(swmm, kNodeId, first.exchange, kDtSub);
    EXPECT_DOUBLE_EQ(swmm.get_node_lateral_inflow(kNodeId), limit.q_limit);

    state.replay_pending();
    ASSERT_EQ(state.cells().size(), 1U);
    EXPECT_NEAR(state.cells()[0].volume, kPhiT * kDepth * kArea - limit.v_limit, 1.0e-12);
    EXPECT_DOUBLE_EQ(state.cells()[0].mass_deficit_account.volume, first.exchange.v_unmet);

    const auto repay_request = scau::coupling::drainage::make_swmm_exchange_request(0.0, kDtSub);
    const auto second = state.apply_exchange(0U, repay_request);
    const auto second_limit = scau::coupling::core::compute_flow_limit(state.cells()[0], kDtSub);

    EXPECT_DOUBLE_EQ(second.exchange.q_granted, 0.0);
    EXPECT_DOUBLE_EQ(second.exchange.v_granted, 0.0);
    EXPECT_DOUBLE_EQ(second.exchange.q_repay, second_limit.q_limit);
    EXPECT_DOUBLE_EQ(second.exchange.v_repay, second_limit.v_limit);
    EXPECT_DOUBLE_EQ(second.exchange.v_unmet, 0.0);

    scau::coupling::drainage::accept_swmm_exchange_decision(swmm, kNodeId, second.exchange, kDtSub);
    EXPECT_DOUBLE_EQ(swmm.get_node_lateral_inflow(kNodeId), second_limit.q_limit);

    state.replay_pending();
    EXPECT_NEAR(state.cells()[0].volume, kPhiT * kDepth * kArea - limit.v_limit - second_limit.v_limit, 1.0e-12);
    EXPECT_DOUBLE_EQ(state.cells()[0].mass_deficit_account.volume, first.exchange.v_unmet - second_limit.v_limit);
    EXPECT_TRUE(swmm.is_surcharged(kNodeId));
}

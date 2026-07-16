#include <stdexcept>

#include <gtest/gtest.h>

#include "coupling/core/state.hpp"

TEST(CouplingApplyExchange, ReturnsDecisionConsistentWithPipeline) {
    scau::coupling::core::CouplingState state{{
        {.volume = 40.0, .mass_deficit_account = {.volume = 0.0},
         .phi_t = 0.4, .h = 2.0, .area = 50.0},
    }};
    const scau::coupling::core::ExchangeRequest request{.q_request = 4.0, .dt_sub = 4.0};

    const auto expected = scau::coupling::core::evaluate_exchange_pipeline(
        state.cells()[0], request);
    const auto actual = state.apply_exchange(0U, request);

    EXPECT_DOUBLE_EQ(actual.exchange.q_granted, expected.exchange.q_granted);
    EXPECT_DOUBLE_EQ(actual.exchange.v_granted, expected.exchange.v_granted);
    EXPECT_DOUBLE_EQ(actual.exchange.v_unmet, expected.exchange.v_unmet);
    EXPECT_DOUBLE_EQ(actual.exchange.q_repay, expected.exchange.q_repay);
    EXPECT_DOUBLE_EQ(actual.exchange.v_repay, expected.exchange.v_repay);
    EXPECT_EQ(actual.drain_split.micro_steps, expected.drain_split.micro_steps);
    EXPECT_DOUBLE_EQ(actual.drain_split.dt_micro, expected.drain_split.dt_micro);
    EXPECT_DOUBLE_EQ(actual.drain_split.v_per_micro_step, expected.drain_split.v_per_micro_step);
    EXPECT_EQ(actual.drain_split_engaged, expected.drain_split_engaged);
    EXPECT_EQ(actual.negative_depth_fix_engaged, expected.negative_depth_fix_engaged);
}

TEST(CouplingApplyExchange, EnqueuesEventButDoesNotMutateVolume) {
    scau::coupling::core::CouplingState state{{
        {.volume = 40.0, .mass_deficit_account = {.volume = 0.0},
         .phi_t = 0.4, .h = 2.0, .area = 50.0},
    }};
    const scau::coupling::core::ExchangeRequest request{.q_request = 4.0, .dt_sub = 4.0};

    const auto decision = state.apply_exchange(0U, request);
    EXPECT_DOUBLE_EQ(state.cells()[0].volume, 40.0);

    state.replay_pending();
    EXPECT_DOUBLE_EQ(state.cells()[0].volume, 40.0 - decision.exchange.v_granted - decision.exchange.v_repay);
    EXPECT_DOUBLE_EQ(
        state.cells()[0].h,
        2.0 - (decision.exchange.v_granted + decision.exchange.v_repay) / (0.4 * 50.0));
}

TEST(CouplingApplyExchange, UpdatesRuntimeCounters) {
    scau::coupling::core::CouplingState state{{
        {.volume = 40.0, .mass_deficit_account = {.volume = 0.0},
         .phi_t = 0.4, .h = 2.0, .area = 50.0},
    }};
    const scau::coupling::core::ExchangeRequest request{.q_request = 4.0, .dt_sub = 4.0};

    const auto decision = state.apply_exchange(0U, request);

    if (decision.drain_split_engaged) {
        EXPECT_EQ(state.runtime_counters().count_drain_split, 1U);
    } else {
        EXPECT_EQ(state.runtime_counters().count_drain_split, 0U);
    }
    EXPECT_EQ(state.runtime_counters().count_negative_depth_fix, 0U);
}

TEST(CouplingApplyExchange, ReplayUpdatesMassDeficitAccount) {
    scau::coupling::core::CouplingState state{{
        {.volume = 40.0, .mass_deficit_account = {.volume = 5.0},
         .phi_t = 0.4, .h = 2.0, .area = 50.0},
    }};
    const scau::coupling::core::ExchangeRequest request{.q_request = 4.0, .dt_sub = 4.0};

    const auto decision = state.apply_exchange(0U, request);
    state.replay_pending();

    const double expected_deficit =
        5.0 + decision.exchange.v_unmet - decision.exchange.v_repay;
    const double clamped = expected_deficit < 0.0 ? 0.0 : expected_deficit;
    EXPECT_DOUBLE_EQ(state.cells()[0].mass_deficit_account.volume, clamped);
}

TEST(CouplingApplyExchange, ReplayAppliesGrantedAndRepaymentVolumesToCellVolume) {
    scau::coupling::core::CouplingState state{{
        {.volume = 40.0, .mass_deficit_account = {.volume = 5.0},
         .phi_t = 0.4, .h = 2.0, .area = 50.0},
    }};
    const scau::coupling::core::ExchangeRequest request{.q_request = 4.0, .dt_sub = 4.0};

    const auto decision = state.apply_exchange(0U, request);
    state.replay_pending();

    EXPECT_GT(decision.exchange.v_repay, 0.0);
    EXPECT_DOUBLE_EQ(
        state.cells()[0].volume,
        40.0 - decision.exchange.v_granted - decision.exchange.v_repay);
    EXPECT_DOUBLE_EQ(
        state.cells()[0].h,
        2.0 - (decision.exchange.v_granted + decision.exchange.v_repay) / (0.4 * 50.0));
}

TEST(CouplingApplyExchange, RejectsAggregateExchangePipelineOnSharedCell) {
    const scau::coupling::core::ExchangeCellState shared_cell{
        .volume = 40.0,
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
        .shared_deficit_accounts = {
            {
                .endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::drainage, .node_id = 10U},
                .mass_deficit_account = {.volume = 4.0},
            },
        },
    };

    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::evaluate_exchange_pipeline(
            shared_cell,
            {.q_request = 1.0, .dt_sub = 4.0})),
        std::invalid_argument);
}

TEST(CouplingApplyExchange, RejectsAggregateApplyExchangeOnSharedCell) {
    scau::coupling::core::CouplingState state{{
        {
            .volume = 40.0,
            .phi_t = 0.4,
            .h = 2.0,
            .area = 50.0,
            .shared_deficit_accounts = {
                {
                    .endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::drainage, .node_id = 10U},
                    .mass_deficit_account = {.volume = 4.0},
                },
            },
        },
    }};

    EXPECT_THROW(
        static_cast<void>(state.apply_exchange(0U, {.q_request = 1.0, .dt_sub = 4.0})),
        std::invalid_argument);
}

TEST(CouplingApplyExchange, RejectsOutOfRangeCellIndex) {
    scau::coupling::core::CouplingState state{{
        {.volume = 40.0, .phi_t = 0.4, .h = 2.0, .area = 50.0},
    }};
    const scau::coupling::core::ExchangeRequest request{.q_request = 1.0, .dt_sub = 4.0};

    EXPECT_THROW(
        static_cast<void>(state.apply_exchange(1U, request)),
        std::out_of_range);
}

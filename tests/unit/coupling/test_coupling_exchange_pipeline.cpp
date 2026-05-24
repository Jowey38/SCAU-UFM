#include <stdexcept>

#include <gtest/gtest.h>

#include "coupling/core/state.hpp"

namespace {

scau::coupling::core::ExchangeCellState make_cell(
    double deficit_volume = 0.0,
    double phi_t = 0.4,
    double h = 2.0,
    double area = 50.0) {
    return scau::coupling::core::ExchangeCellState{
        .volume = 0.0,
        .mass_deficit_account = {.volume = deficit_volume},
        .phi_t = phi_t,
        .h = h,
        .area = area,
    };
}

}  // namespace

TEST(CouplingExchangePipeline, SafeRequestReturnsFullGrantAndSingleMicroStep) {
    const auto cell = make_cell();
    const scau::coupling::core::ExchangeRequest request{.q_request = 4.0, .dt_sub = 4.0};

    const auto decision = scau::coupling::core::evaluate_exchange_pipeline(cell, request);

    EXPECT_DOUBLE_EQ(decision.exchange.q_granted, 4.0);
    EXPECT_DOUBLE_EQ(decision.exchange.v_granted, 16.0);
    EXPECT_DOUBLE_EQ(decision.exchange.q_repay, 0.0);
    EXPECT_DOUBLE_EQ(decision.exchange.v_repay, 0.0);
    EXPECT_DOUBLE_EQ(decision.exchange.v_unmet, 0.0);
    EXPECT_EQ(decision.drain_split.micro_steps, 2);
    EXPECT_DOUBLE_EQ(decision.drain_split.dt_micro, 2.0);
    EXPECT_DOUBLE_EQ(decision.drain_split.v_per_micro_step, 8.0);
}

TEST(CouplingExchangePipeline, RequestAboveHardGateClampsAndAccruesUnmet) {
    const auto cell = make_cell();
    const scau::coupling::core::ExchangeRequest request{.q_request = 20.0, .dt_sub = 4.0};

    const auto decision = scau::coupling::core::evaluate_exchange_pipeline(cell, request);

    EXPECT_DOUBLE_EQ(decision.exchange.q_granted, 9.0);
    EXPECT_DOUBLE_EQ(decision.exchange.v_granted, 36.0);
    EXPECT_DOUBLE_EQ(decision.exchange.v_unmet, 44.0);
    EXPECT_EQ(decision.drain_split.micro_steps, 5);
    EXPECT_DOUBLE_EQ(decision.drain_split.dt_micro, 0.8);
    EXPECT_DOUBLE_EQ(decision.drain_split.v_per_micro_step, 7.2);
}

TEST(CouplingExchangePipeline, NonnegativeStorageBackoffRunsAfterHardGateClamp) {
    const auto cell = make_cell(0.0, 0.2, 1.0, 50.0);
    const scau::coupling::core::ExchangeRequest request{.q_request = 5.0, .dt_sub = 4.0};

    const auto decision = scau::coupling::core::evaluate_exchange_pipeline(cell, request);

    EXPECT_DOUBLE_EQ(decision.exchange.q_granted, 2.25);
    EXPECT_DOUBLE_EQ(decision.exchange.v_granted, 9.0);
    EXPECT_DOUBLE_EQ(decision.exchange.v_unmet, 11.0);
    EXPECT_EQ(decision.drain_split.micro_steps, 5);
    EXPECT_DOUBLE_EQ(decision.drain_split.dt_micro, 0.8);
    EXPECT_DOUBLE_EQ(decision.drain_split.v_per_micro_step, 1.8);
}

TEST(CouplingExchangePipeline, DeficitRepaymentPrecedesNewRequestGrant) {
    const auto cell = make_cell(8.0);
    const scau::coupling::core::ExchangeRequest request{.q_request = 5.0, .dt_sub = 4.0};

    const auto decision = scau::coupling::core::evaluate_exchange_pipeline(cell, request);

    EXPECT_DOUBLE_EQ(decision.exchange.q_repay, 2.0);
    EXPECT_DOUBLE_EQ(decision.exchange.v_repay, 8.0);
    EXPECT_DOUBLE_EQ(decision.exchange.q_granted, 5.0);
    EXPECT_DOUBLE_EQ(decision.exchange.v_granted, 20.0);
    EXPECT_DOUBLE_EQ(decision.exchange.v_unmet, 0.0);
    EXPECT_EQ(decision.drain_split.micro_steps, 3);
    EXPECT_DOUBLE_EQ(decision.drain_split.dt_micro, 4.0 / 3.0);
    EXPECT_DOUBLE_EQ(decision.drain_split.v_per_micro_step, 20.0 / 3.0);
}

TEST(CouplingExchangePipeline, InvalidInputsAreRejectedByComposedHelpers) {
    const auto cell = make_cell();
    const auto negative_deficit_cell = make_cell(-0.1);
    const auto negative_phi_t_cell = make_cell(0.0, -0.1, 2.0, 50.0);

    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::evaluate_exchange_pipeline(
            cell, scau::coupling::core::ExchangeRequest{.q_request = -0.1, .dt_sub = 4.0})),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::evaluate_exchange_pipeline(
            cell, scau::coupling::core::ExchangeRequest{.q_request = 1.0, .dt_sub = 0.0})),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::evaluate_exchange_pipeline(
            negative_deficit_cell, scau::coupling::core::ExchangeRequest{.q_request = 1.0, .dt_sub = 4.0})),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::evaluate_exchange_pipeline(
            negative_phi_t_cell, scau::coupling::core::ExchangeRequest{.q_request = 1.0, .dt_sub = 4.0})),
        std::invalid_argument);
}

#include <stdexcept>

#include <gtest/gtest.h>

#include "coupling/core/state.hpp"

namespace {

scau::coupling::core::ExchangeCellState make_cell(double deficit_volume) {
    return scau::coupling::core::ExchangeCellState{
        .volume = 0.0,
        .mass_deficit_account = {.volume = deficit_volume},
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
    };
}

}  // namespace

TEST(CouplingExchangeDecision, GrantsFullRequestBelowHardGateWithNoDeficit) {
    const auto cell = make_cell(0.0);
    const scau::coupling::core::ExchangeRequest request{.q_request = 4.0, .dt_sub = 4.0};

    const auto decision = scau::coupling::core::evaluate_exchange(cell, request);

    EXPECT_DOUBLE_EQ(decision.q_granted, 4.0);
    EXPECT_DOUBLE_EQ(decision.v_granted, 16.0);
    EXPECT_DOUBLE_EQ(decision.q_repay, 0.0);
    EXPECT_DOUBLE_EQ(decision.v_repay, 0.0);
    EXPECT_DOUBLE_EQ(decision.v_unmet, 0.0);
}

TEST(CouplingExchangeDecision, ClampsRequestAboveHardGateAndAccruesUnmetVolume) {
    const auto cell = make_cell(0.0);
    const scau::coupling::core::ExchangeRequest request{.q_request = 20.0, .dt_sub = 4.0};

    const auto decision = scau::coupling::core::evaluate_exchange(cell, request);

    EXPECT_DOUBLE_EQ(decision.q_granted, 9.0);
    EXPECT_DOUBLE_EQ(decision.v_granted, 36.0);
    EXPECT_DOUBLE_EQ(decision.q_repay, 0.0);
    EXPECT_DOUBLE_EQ(decision.v_repay, 0.0);
    EXPECT_DOUBLE_EQ(decision.v_unmet, 44.0);
}

TEST(CouplingExchangeDecision, RepaysDeficitBeforeGrantingRequest) {
    const auto cell = make_cell(8.0);
    const scau::coupling::core::ExchangeRequest request{.q_request = 5.0, .dt_sub = 4.0};

    const auto decision = scau::coupling::core::evaluate_exchange(cell, request);

    EXPECT_DOUBLE_EQ(decision.q_repay, 2.0);
    EXPECT_DOUBLE_EQ(decision.v_repay, 8.0);
    EXPECT_DOUBLE_EQ(decision.q_granted, 5.0);
    EXPECT_DOUBLE_EQ(decision.v_granted, 20.0);
    EXPECT_DOUBLE_EQ(decision.v_unmet, 0.0);
}

TEST(CouplingExchangeDecision, DeficitAtOrAboveHardGateBlocksNewRequest) {
    const auto cell = make_cell(40.0);
    const scau::coupling::core::ExchangeRequest request{.q_request = 5.0, .dt_sub = 4.0};

    const auto decision = scau::coupling::core::evaluate_exchange(cell, request);

    EXPECT_DOUBLE_EQ(decision.q_repay, 9.0);
    EXPECT_DOUBLE_EQ(decision.v_repay, 36.0);
    EXPECT_DOUBLE_EQ(decision.q_granted, 0.0);
    EXPECT_DOUBLE_EQ(decision.v_granted, 0.0);
    EXPECT_DOUBLE_EQ(decision.v_unmet, 20.0);
}

TEST(CouplingExchangeDecision, RejectsNegativeRequestOrDeficitVolume) {
    const auto cell = make_cell(1.0);
    const auto negative_cell = scau::coupling::core::ExchangeCellState{
        .volume = 0.0,
        .mass_deficit_account = {.volume = -0.1},
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
    };

    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::evaluate_exchange(
            cell, scau::coupling::core::ExchangeRequest{.q_request = -0.1, .dt_sub = 4.0})),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::evaluate_exchange(
            negative_cell, scau::coupling::core::ExchangeRequest{.q_request = 1.0, .dt_sub = 4.0})),
        std::invalid_argument);
}

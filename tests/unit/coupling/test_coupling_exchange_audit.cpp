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
        .volume = phi_t * h * area,
        .mass_deficit_account = {.volume = deficit_volume},
        .phi_t = phi_t,
        .h = h,
        .area = area,
    };
}

}  // namespace

TEST(CouplingExchangeAudit, SafeRequestIsBalancedWithZeroResidual) {
    const auto cell = make_cell();
    const scau::coupling::core::ExchangeRequest request{.q_request = 4.0, .dt_sub = 4.0};
    const auto decision = scau::coupling::core::evaluate_exchange_pipeline(cell, request);

    const auto audit = scau::coupling::core::audit_exchange_decision(request, decision);

    EXPECT_DOUBLE_EQ(audit.request_volume, 16.0);
    EXPECT_DOUBLE_EQ(audit.accounted_volume, 16.0);
    EXPECT_DOUBLE_EQ(audit.residual, 0.0);
    EXPECT_TRUE(audit.balanced);
}

TEST(CouplingExchangeAudit, HardGateClampScenarioIsBalanced) {
    const auto cell = make_cell();
    const scau::coupling::core::ExchangeRequest request{.q_request = 20.0, .dt_sub = 4.0};
    const auto decision = scau::coupling::core::evaluate_exchange_pipeline(cell, request);

    const auto audit = scau::coupling::core::audit_exchange_decision(request, decision);

    EXPECT_DOUBLE_EQ(audit.request_volume, 80.0);
    EXPECT_DOUBLE_EQ(audit.accounted_volume, 80.0);
    EXPECT_DOUBLE_EQ(audit.residual, 0.0);
    EXPECT_TRUE(audit.balanced);
}

TEST(CouplingExchangeAudit, DeficitRepaymentScenarioIsBalanced) {
    const auto cell = make_cell(8.0);
    const scau::coupling::core::ExchangeRequest request{.q_request = 5.0, .dt_sub = 4.0};
    const auto decision = scau::coupling::core::evaluate_exchange_pipeline(cell, request);

    const auto audit = scau::coupling::core::audit_exchange_decision(request, decision);

    EXPECT_DOUBLE_EQ(audit.request_volume, 20.0);
    EXPECT_DOUBLE_EQ(audit.accounted_volume, 20.0);
    EXPECT_DOUBLE_EQ(audit.residual, 0.0);
    EXPECT_TRUE(audit.balanced);
}

TEST(CouplingExchangeAudit, NonnegativeStorageBackoffScenarioIsBalanced) {
    const auto cell = make_cell(0.0, 0.2, 1.0, 50.0);
    const scau::coupling::core::ExchangeRequest request{.q_request = 5.0, .dt_sub = 4.0};
    const auto decision = scau::coupling::core::evaluate_exchange_pipeline(cell, request);

    const auto audit = scau::coupling::core::audit_exchange_decision(request, decision);

    EXPECT_DOUBLE_EQ(audit.request_volume, 20.0);
    EXPECT_DOUBLE_EQ(audit.accounted_volume, 20.0);
    EXPECT_DOUBLE_EQ(audit.residual, 0.0);
    EXPECT_TRUE(audit.balanced);
}

TEST(CouplingExchangeAudit, ManuallyConstructedImbalancedDecisionFlagsResidual) {
    const scau::coupling::core::ExchangeRequest request{.q_request = 4.0, .dt_sub = 4.0};
    const scau::coupling::core::ExchangePipelineDecision decision{
        .exchange = {.q_granted = 1.0, .v_granted = 4.0, .v_unmet = 8.0},
    };

    const auto audit = scau::coupling::core::audit_exchange_decision(request, decision);

    EXPECT_DOUBLE_EQ(audit.request_volume, 16.0);
    EXPECT_DOUBLE_EQ(audit.accounted_volume, 12.0);
    EXPECT_DOUBLE_EQ(audit.residual, 4.0);
    EXPECT_FALSE(audit.balanced);
}

TEST(CouplingExchangeAudit, RejectsNegativeRequestOrDecisionVolumes) {
    const scau::coupling::core::ExchangePipelineDecision good_decision{
        .exchange = {.v_granted = 0.0, .v_unmet = 0.0},
    };

    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::audit_exchange_decision(
            scau::coupling::core::ExchangeRequest{.q_request = -0.1, .dt_sub = 4.0}, good_decision)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::audit_exchange_decision(
            scau::coupling::core::ExchangeRequest{.q_request = 1.0, .dt_sub = 0.0}, good_decision)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::audit_exchange_decision(
            scau::coupling::core::ExchangeRequest{.q_request = 1.0, .dt_sub = 4.0},
            scau::coupling::core::ExchangePipelineDecision{
                .exchange = {.v_granted = -0.1, .v_unmet = 0.0}})),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::audit_exchange_decision(
            scau::coupling::core::ExchangeRequest{.q_request = 1.0, .dt_sub = 4.0},
            scau::coupling::core::ExchangePipelineDecision{
                .exchange = {.v_granted = 0.0, .v_unmet = -0.1}})),
        std::invalid_argument);
}

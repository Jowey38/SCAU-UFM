#include <gtest/gtest.h>

#include "coupling/core/state.hpp"

namespace {

scau::coupling::core::MockCouplingSchedulerFaultConsumption make_consumption(
    scau::coupling::core::FaultControllerProposedActionState state) {
    const scau::coupling::core::FaultControllerProposedAction action{.state = state};
    return scau::coupling::core::consume_mock_scheduler_fault_action(
        scau::coupling::core::observe_mock_scheduler_fault_action(action));
}

}  // namespace

TEST(CouplingFaultControllerPassiveState, ContinueConsumptionClassifiesAsRunningWithoutControl) {
    const auto consumption = make_consumption(
        scau::coupling::core::FaultControllerProposedActionState::continue_run);

    const auto classification = scau::coupling::core::classify_fault_controller_passive_state(consumption);

    EXPECT_EQ(
        classification.state,
        scau::coupling::core::FaultControllerPassiveStateLabel::running);
    EXPECT_EQ(
        classification.consumption.consumed_state,
        scau::coupling::core::FaultControllerProposedActionState::continue_run);
    EXPECT_FALSE(classification.scheduler_control_enabled);
    EXPECT_FALSE(classification.isolation_enabled);
    EXPECT_FALSE(classification.reconnect_enabled);
    EXPECT_FALSE(classification.abort_enabled);
}

TEST(CouplingFaultControllerPassiveState, ReviewRequiredConsumptionClassifiesAsReviewRequiredWithoutControl) {
    const auto consumption = make_consumption(
        scau::coupling::core::FaultControllerProposedActionState::review_required);

    const auto classification = scau::coupling::core::classify_fault_controller_passive_state(consumption);

    EXPECT_EQ(
        classification.state,
        scau::coupling::core::FaultControllerPassiveStateLabel::review_required);
    EXPECT_TRUE(classification.consumption.review_required_consumed);
    EXPECT_FALSE(classification.scheduler_control_enabled);
    EXPECT_FALSE(classification.isolation_enabled);
    EXPECT_FALSE(classification.reconnect_enabled);
    EXPECT_FALSE(classification.abort_enabled);
}

TEST(CouplingFaultControllerPassiveState, ClassificationDoesNotPromoteConsumptionExecutionFlags) {
    const scau::coupling::core::MockCouplingSchedulerFaultConsumption consumption{
        .consumed_state = scau::coupling::core::FaultControllerProposedActionState::review_required,
        .review_required_consumed = true,
        .exchange_scheduling_allowed = false,
        .replay_allowed = false,
        .audit_allowed = false,
        .executed_isolation = true,
        .executed_reconnect = true,
        .executed_abort = true,
    };

    const auto classification = scau::coupling::core::classify_fault_controller_passive_state(consumption);

    EXPECT_EQ(
        classification.state,
        scau::coupling::core::FaultControllerPassiveStateLabel::review_required);
    EXPECT_TRUE(classification.consumption.executed_isolation);
    EXPECT_TRUE(classification.consumption.executed_reconnect);
    EXPECT_TRUE(classification.consumption.executed_abort);
    EXPECT_FALSE(classification.scheduler_control_enabled);
    EXPECT_FALSE(classification.isolation_enabled);
    EXPECT_FALSE(classification.reconnect_enabled);
    EXPECT_FALSE(classification.abort_enabled);
}

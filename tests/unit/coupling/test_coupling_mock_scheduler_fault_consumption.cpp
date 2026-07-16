#include <gtest/gtest.h>

#include "coupling/core/state.hpp"

TEST(CouplingMockSchedulerFaultConsumption, ContinueObservationIsConsumedWithoutExecution) {
    const scau::coupling::core::FaultControllerProposedAction action{
        .state = scau::coupling::core::FaultControllerProposedActionState::continue_run,
    };
    const auto observation = scau::coupling::core::observe_mock_scheduler_fault_action(action);

    const auto consumption = scau::coupling::core::consume_mock_scheduler_fault_action(observation);

    EXPECT_EQ(
        consumption.consumed_state,
        scau::coupling::core::FaultControllerProposedActionState::continue_run);
    EXPECT_FALSE(consumption.review_required_consumed);
    EXPECT_TRUE(consumption.exchange_scheduling_allowed);
    EXPECT_TRUE(consumption.replay_allowed);
    EXPECT_TRUE(consumption.audit_allowed);
    EXPECT_FALSE(consumption.executed_isolation);
    EXPECT_FALSE(consumption.executed_reconnect);
    EXPECT_FALSE(consumption.executed_abort);
}

TEST(CouplingMockSchedulerFaultConsumption, ReviewRequiredObservationIsConsumedWithoutExecution) {
    const scau::coupling::core::FaultControllerProposedAction action{
        .diagnostic = {
            .status = scau::coupling::core::FaultControllerDiagnosticStatus::fault_detected,
            .health = {
                .status = scau::coupling::core::EngineHealthAggregateStatus::any_unhealthy,
                .report_count = 2U,
                .unhealthy_count = 1U,
            },
        },
        .state = scau::coupling::core::FaultControllerProposedActionState::review_required,
    };
    const auto observation = scau::coupling::core::observe_mock_scheduler_fault_action(action);

    const auto consumption = scau::coupling::core::consume_mock_scheduler_fault_action(observation);

    EXPECT_EQ(
        consumption.consumed_state,
        scau::coupling::core::FaultControllerProposedActionState::review_required);
    EXPECT_TRUE(consumption.review_required_consumed);
    EXPECT_EQ(
        consumption.observation.action.diagnostic.status,
        scau::coupling::core::FaultControllerDiagnosticStatus::fault_detected);
    EXPECT_EQ(consumption.observation.action.diagnostic.health.report_count, 2U);
    EXPECT_EQ(consumption.observation.action.diagnostic.health.unhealthy_count, 1U);
    EXPECT_TRUE(consumption.exchange_scheduling_allowed);
    EXPECT_TRUE(consumption.replay_allowed);
    EXPECT_TRUE(consumption.audit_allowed);
    EXPECT_FALSE(consumption.executed_isolation);
    EXPECT_FALSE(consumption.executed_reconnect);
    EXPECT_FALSE(consumption.executed_abort);
}

TEST(CouplingMockSchedulerFaultConsumption, ConsumptionDoesNotPromoteObservationExecutionFlags) {
    const scau::coupling::core::MockCouplingSchedulerFaultObservation observation{
        .action = {
            .state = scau::coupling::core::FaultControllerProposedActionState::review_required,
            .execute_isolation = true,
            .execute_reconnect = true,
            .execute_abort = true,
        },
        .observed_state = scau::coupling::core::FaultControllerProposedActionState::review_required,
        .observed_review_required = true,
        .executed_isolation = true,
        .executed_reconnect = true,
        .executed_abort = true,
    };

    const auto consumption = scau::coupling::core::consume_mock_scheduler_fault_action(observation);

    EXPECT_EQ(
        consumption.consumed_state,
        scau::coupling::core::FaultControllerProposedActionState::review_required);
    EXPECT_TRUE(consumption.observation.action.execute_isolation);
    EXPECT_TRUE(consumption.observation.action.execute_reconnect);
    EXPECT_TRUE(consumption.observation.action.execute_abort);
    EXPECT_TRUE(consumption.observation.executed_isolation);
    EXPECT_TRUE(consumption.observation.executed_reconnect);
    EXPECT_TRUE(consumption.observation.executed_abort);
    EXPECT_TRUE(consumption.exchange_scheduling_allowed);
    EXPECT_TRUE(consumption.replay_allowed);
    EXPECT_TRUE(consumption.audit_allowed);
    EXPECT_FALSE(consumption.executed_isolation);
    EXPECT_FALSE(consumption.executed_reconnect);
    EXPECT_FALSE(consumption.executed_abort);
}

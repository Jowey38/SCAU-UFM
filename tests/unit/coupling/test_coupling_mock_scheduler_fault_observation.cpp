#include <gtest/gtest.h>

#include "coupling/core/state.hpp"

TEST(CouplingMockSchedulerFaultObservation, ContinueActionIsObservedWithoutExecution) {
    const scau::coupling::core::FaultControllerProposedAction action{
        .state = scau::coupling::core::FaultControllerProposedActionState::continue_run,
    };

    const auto observation = scau::coupling::core::observe_mock_scheduler_fault_action(action);

    EXPECT_EQ(
        observation.observed_state,
        scau::coupling::core::FaultControllerProposedActionState::continue_run);
    EXPECT_FALSE(observation.observed_review_required);
    EXPECT_FALSE(observation.executed_isolation);
    EXPECT_FALSE(observation.executed_reconnect);
    EXPECT_FALSE(observation.executed_abort);
}

TEST(CouplingMockSchedulerFaultObservation, ReviewRequiredActionIsObservedWithoutExecution) {
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

    EXPECT_EQ(
        observation.observed_state,
        scau::coupling::core::FaultControllerProposedActionState::review_required);
    EXPECT_TRUE(observation.observed_review_required);
    EXPECT_EQ(
        observation.action.diagnostic.status,
        scau::coupling::core::FaultControllerDiagnosticStatus::fault_detected);
    EXPECT_EQ(observation.action.diagnostic.health.report_count, 2U);
    EXPECT_EQ(observation.action.diagnostic.health.unhealthy_count, 1U);
    EXPECT_FALSE(observation.executed_isolation);
    EXPECT_FALSE(observation.executed_reconnect);
    EXPECT_FALSE(observation.executed_abort);
}

TEST(CouplingMockSchedulerFaultObservation, ObservationDoesNotPromoteActionExecutionFlags) {
    const scau::coupling::core::FaultControllerProposedAction action{
        .state = scau::coupling::core::FaultControllerProposedActionState::review_required,
        .execute_isolation = true,
        .execute_reconnect = true,
        .execute_abort = true,
    };

    const auto observation = scau::coupling::core::observe_mock_scheduler_fault_action(action);

    EXPECT_EQ(
        observation.observed_state,
        scau::coupling::core::FaultControllerProposedActionState::review_required);
    EXPECT_TRUE(observation.action.execute_isolation);
    EXPECT_TRUE(observation.action.execute_reconnect);
    EXPECT_TRUE(observation.action.execute_abort);
    EXPECT_FALSE(observation.executed_isolation);
    EXPECT_FALSE(observation.executed_reconnect);
    EXPECT_FALSE(observation.executed_abort);
}

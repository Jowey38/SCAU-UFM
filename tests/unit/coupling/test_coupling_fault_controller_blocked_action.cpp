#include <gtest/gtest.h>

#include "coupling/core/state.hpp"

namespace {

scau::coupling::core::FaultControllerPassiveActionOutcome make_outcome(
    scau::coupling::core::FaultControllerProposedActionState action_state) {
    const scau::coupling::core::FaultControllerProposedAction action{.state = action_state};
    const auto observation = scau::coupling::core::observe_mock_scheduler_fault_action(action);
    const auto consumption = scau::coupling::core::consume_mock_scheduler_fault_action(observation);
    const auto classification = scau::coupling::core::classify_fault_controller_passive_state(consumption);
    const auto transition = scau::coupling::core::make_fault_controller_passive_transition(
        scau::coupling::core::FaultControllerPassiveStateLabel::running,
        classification);
    const auto audit = scau::coupling::core::make_fault_controller_passive_action_audit_record(transition);
    return scau::coupling::core::make_fault_controller_passive_action_outcome(audit);
}

}  // namespace

TEST(CouplingFaultControllerBlockedAction, NominalOutcomeProducesNoActionBlockedRecord) {
    const auto outcome = make_outcome(
        scau::coupling::core::FaultControllerProposedActionState::continue_run);

    const auto blocked = scau::coupling::core::make_fault_controller_blocked_action(outcome);

    EXPECT_EQ(
        blocked.reason,
        scau::coupling::core::FaultControllerBlockedActionReason::no_action_requested);
    EXPECT_EQ(
        blocked.outcome.outcome_kind,
        scau::coupling::core::FaultControllerPassiveActionOutcomeKind::not_requested);
    EXPECT_FALSE(blocked.execution_allowed);
    EXPECT_FALSE(blocked.scheduler_control_allowed);
    EXPECT_FALSE(blocked.adapter_call_allowed);
    EXPECT_FALSE(blocked.isolation_allowed);
    EXPECT_FALSE(blocked.reconnect_allowed);
    EXPECT_FALSE(blocked.abort_allowed);
    EXPECT_FALSE(blocked.release_gate_action_allowed);
}

TEST(CouplingFaultControllerBlockedAction, ReviewRequiredOutcomeProducesOperatorReviewBlockedRecord) {
    const auto outcome = make_outcome(
        scau::coupling::core::FaultControllerProposedActionState::review_required);

    const auto blocked = scau::coupling::core::make_fault_controller_blocked_action(outcome);

    EXPECT_EQ(
        blocked.reason,
        scau::coupling::core::FaultControllerBlockedActionReason::operator_review_required);
    EXPECT_EQ(
        blocked.outcome.outcome_kind,
        scau::coupling::core::FaultControllerPassiveActionOutcomeKind::operator_review_required);
    EXPECT_TRUE(blocked.outcome.operator_review_required);
    EXPECT_FALSE(blocked.execution_allowed);
    EXPECT_FALSE(blocked.scheduler_control_allowed);
    EXPECT_FALSE(blocked.adapter_call_allowed);
    EXPECT_FALSE(blocked.isolation_allowed);
    EXPECT_FALSE(blocked.reconnect_allowed);
    EXPECT_FALSE(blocked.abort_allowed);
    EXPECT_FALSE(blocked.release_gate_action_allowed);
}

TEST(CouplingFaultControllerBlockedAction, BlockedActionDoesNotPromoteOutcomeControlFlags) {
    const scau::coupling::core::FaultControllerPassiveActionOutcome outcome{
        .outcome_kind = scau::coupling::core::FaultControllerPassiveActionOutcomeKind::operator_review_required,
        .reason = scau::coupling::core::FaultControllerPassiveActionOutcomeReason::review_required_only,
        .scheduler_control_available = true,
        .scheduler_control_used = true,
        .adapter_boundary_available = true,
        .adapter_call_attempted = true,
        .adapter_call_succeeded = true,
        .adapter_call_failed = true,
        .runtime_action_requested = true,
        .runtime_action_attempted = true,
        .runtime_action_executed = true,
        .runtime_action_skipped = true,
        .runtime_action_failed = true,
        .operator_review_required = true,
        .rollback_required = true,
        .replay_required = true,
        .mass_audit_required = true,
        .release_gate_action_executed = true,
    };

    const auto blocked = scau::coupling::core::make_fault_controller_blocked_action(outcome);

    EXPECT_TRUE(blocked.outcome.scheduler_control_available);
    EXPECT_TRUE(blocked.outcome.scheduler_control_used);
    EXPECT_TRUE(blocked.outcome.adapter_boundary_available);
    EXPECT_TRUE(blocked.outcome.adapter_call_attempted);
    EXPECT_TRUE(blocked.outcome.adapter_call_succeeded);
    EXPECT_TRUE(blocked.outcome.adapter_call_failed);
    EXPECT_TRUE(blocked.outcome.runtime_action_requested);
    EXPECT_TRUE(blocked.outcome.runtime_action_attempted);
    EXPECT_TRUE(blocked.outcome.runtime_action_executed);
    EXPECT_TRUE(blocked.outcome.runtime_action_skipped);
    EXPECT_TRUE(blocked.outcome.runtime_action_failed);
    EXPECT_TRUE(blocked.outcome.rollback_required);
    EXPECT_TRUE(blocked.outcome.replay_required);
    EXPECT_TRUE(blocked.outcome.mass_audit_required);
    EXPECT_TRUE(blocked.outcome.release_gate_action_executed);
    EXPECT_FALSE(blocked.execution_allowed);
    EXPECT_FALSE(blocked.scheduler_control_allowed);
    EXPECT_FALSE(blocked.adapter_call_allowed);
    EXPECT_FALSE(blocked.isolation_allowed);
    EXPECT_FALSE(blocked.reconnect_allowed);
    EXPECT_FALSE(blocked.abort_allowed);
    EXPECT_FALSE(blocked.release_gate_action_allowed);
}

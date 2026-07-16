#include <gtest/gtest.h>

#include "coupling/core/state.hpp"

namespace {

scau::coupling::core::FaultControllerPassiveActionAuditRecord make_audit(
    scau::coupling::core::FaultControllerProposedActionState action_state) {
    const scau::coupling::core::FaultControllerProposedAction action{.state = action_state};
    const auto observation = scau::coupling::core::observe_mock_scheduler_fault_action(action);
    const auto consumption = scau::coupling::core::consume_mock_scheduler_fault_action(observation);
    const auto classification = scau::coupling::core::classify_fault_controller_passive_state(consumption);
    const auto transition = scau::coupling::core::make_fault_controller_passive_transition(
        scau::coupling::core::FaultControllerPassiveStateLabel::running,
        classification);
    return scau::coupling::core::make_fault_controller_passive_action_audit_record(transition);
}

}  // namespace

TEST(CouplingFaultControllerPassiveActionOutcome, NominalAuditProducesNotRequestedOutcome) {
    const auto audit = make_audit(
        scau::coupling::core::FaultControllerProposedActionState::continue_run);

    const auto outcome = scau::coupling::core::make_fault_controller_passive_action_outcome(audit);

    EXPECT_EQ(
        outcome.outcome_kind,
        scau::coupling::core::FaultControllerPassiveActionOutcomeKind::not_requested);
    EXPECT_EQ(
        outcome.reason,
        scau::coupling::core::FaultControllerPassiveActionOutcomeReason::nominal_no_action);
    EXPECT_FALSE(outcome.operator_review_required);
    EXPECT_FALSE(outcome.scheduler_control_available);
    EXPECT_FALSE(outcome.scheduler_control_used);
    EXPECT_FALSE(outcome.adapter_boundary_available);
    EXPECT_FALSE(outcome.adapter_call_attempted);
    EXPECT_FALSE(outcome.adapter_call_succeeded);
    EXPECT_FALSE(outcome.adapter_call_failed);
    EXPECT_FALSE(outcome.runtime_action_requested);
    EXPECT_FALSE(outcome.runtime_action_attempted);
    EXPECT_FALSE(outcome.runtime_action_executed);
    EXPECT_FALSE(outcome.runtime_action_skipped);
    EXPECT_FALSE(outcome.runtime_action_failed);
    EXPECT_FALSE(outcome.rollback_required);
    EXPECT_FALSE(outcome.replay_required);
    EXPECT_FALSE(outcome.mass_audit_required);
    EXPECT_FALSE(outcome.release_gate_action_executed);
}

TEST(CouplingFaultControllerPassiveActionOutcome, ReviewRequiredAuditProducesOperatorReviewOutcome) {
    const auto audit = make_audit(
        scau::coupling::core::FaultControllerProposedActionState::review_required);

    const auto outcome = scau::coupling::core::make_fault_controller_passive_action_outcome(audit);

    EXPECT_EQ(
        outcome.outcome_kind,
        scau::coupling::core::FaultControllerPassiveActionOutcomeKind::operator_review_required);
    EXPECT_EQ(
        outcome.reason,
        scau::coupling::core::FaultControllerPassiveActionOutcomeReason::review_required_only);
    EXPECT_TRUE(outcome.operator_review_required);
    EXPECT_EQ(
        outcome.audit.action_kind,
        scau::coupling::core::FaultControllerPassiveActionAuditKind::operator_review);
    EXPECT_FALSE(outcome.scheduler_control_available);
    EXPECT_FALSE(outcome.scheduler_control_used);
    EXPECT_FALSE(outcome.adapter_boundary_available);
    EXPECT_FALSE(outcome.adapter_call_attempted);
    EXPECT_FALSE(outcome.adapter_call_succeeded);
    EXPECT_FALSE(outcome.adapter_call_failed);
    EXPECT_FALSE(outcome.runtime_action_requested);
    EXPECT_FALSE(outcome.runtime_action_attempted);
    EXPECT_FALSE(outcome.runtime_action_executed);
    EXPECT_FALSE(outcome.runtime_action_skipped);
    EXPECT_FALSE(outcome.runtime_action_failed);
    EXPECT_FALSE(outcome.rollback_required);
    EXPECT_FALSE(outcome.replay_required);
    EXPECT_FALSE(outcome.mass_audit_required);
    EXPECT_FALSE(outcome.release_gate_action_executed);
}

TEST(CouplingFaultControllerPassiveActionOutcome, OutcomeDoesNotPromoteAuditExecutionFlags) {
    const scau::coupling::core::FaultControllerPassiveActionAuditRecord audit{
        .action_kind = scau::coupling::core::FaultControllerPassiveActionAuditKind::operator_review,
        .reason = scau::coupling::core::FaultControllerPassiveActionAuditReason::review_required_only,
        .scheduler_control_enabled = true,
        .runtime_action_requested = true,
        .runtime_action_executed = true,
        .isolation_executed = true,
        .reconnect_executed = true,
        .abort_executed = true,
        .adapter_call_executed = true,
        .release_gate_action_executed = true,
    };

    const auto outcome = scau::coupling::core::make_fault_controller_passive_action_outcome(audit);

    EXPECT_TRUE(outcome.audit.scheduler_control_enabled);
    EXPECT_TRUE(outcome.audit.runtime_action_requested);
    EXPECT_TRUE(outcome.audit.runtime_action_executed);
    EXPECT_TRUE(outcome.audit.isolation_executed);
    EXPECT_TRUE(outcome.audit.reconnect_executed);
    EXPECT_TRUE(outcome.audit.abort_executed);
    EXPECT_TRUE(outcome.audit.adapter_call_executed);
    EXPECT_TRUE(outcome.audit.release_gate_action_executed);
    EXPECT_EQ(
        outcome.outcome_kind,
        scau::coupling::core::FaultControllerPassiveActionOutcomeKind::operator_review_required);
    EXPECT_FALSE(outcome.scheduler_control_available);
    EXPECT_FALSE(outcome.scheduler_control_used);
    EXPECT_FALSE(outcome.adapter_boundary_available);
    EXPECT_FALSE(outcome.adapter_call_attempted);
    EXPECT_FALSE(outcome.adapter_call_succeeded);
    EXPECT_FALSE(outcome.adapter_call_failed);
    EXPECT_FALSE(outcome.runtime_action_requested);
    EXPECT_FALSE(outcome.runtime_action_attempted);
    EXPECT_FALSE(outcome.runtime_action_executed);
    EXPECT_FALSE(outcome.runtime_action_skipped);
    EXPECT_FALSE(outcome.runtime_action_failed);
    EXPECT_TRUE(outcome.operator_review_required);
    EXPECT_FALSE(outcome.rollback_required);
    EXPECT_FALSE(outcome.replay_required);
    EXPECT_FALSE(outcome.mass_audit_required);
    EXPECT_FALSE(outcome.release_gate_action_executed);
}

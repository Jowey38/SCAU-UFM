#include <gtest/gtest.h>

#include "coupling/core/state.hpp"

namespace {

scau::coupling::core::FaultControllerBlockedAction make_blocked_action(
    scau::coupling::core::FaultControllerProposedActionState action_state) {
    const scau::coupling::core::FaultControllerProposedAction action{.state = action_state};
    const auto observation = scau::coupling::core::observe_mock_scheduler_fault_action(action);
    const auto consumption = scau::coupling::core::consume_mock_scheduler_fault_action(observation);
    const auto classification = scau::coupling::core::classify_fault_controller_passive_state(consumption);
    const auto transition = scau::coupling::core::make_fault_controller_passive_transition(
        scau::coupling::core::FaultControllerPassiveStateLabel::running,
        classification);
    const auto audit = scau::coupling::core::make_fault_controller_passive_action_audit_record(transition);
    const auto outcome = scau::coupling::core::make_fault_controller_passive_action_outcome(audit);
    return scau::coupling::core::make_fault_controller_blocked_action(outcome);
}

}  // namespace

TEST(CouplingFaultControllerSchedulerControlRequest, NominalBlockedActionProducesNotRequestedPassiveRequest) {
    const auto blocked = make_blocked_action(
        scau::coupling::core::FaultControllerProposedActionState::continue_run);

    const auto request = scau::coupling::core::make_fault_controller_scheduler_control_request(blocked);

    EXPECT_EQ(
        request.requested_kind,
        scau::coupling::core::FaultControllerSchedulerControlKind::none);
    EXPECT_EQ(
        request.status,
        scau::coupling::core::FaultControllerSchedulerControlStatus::not_requested);
    EXPECT_EQ(
        request.reason,
        scau::coupling::core::FaultControllerSchedulerControlReason::no_scheduler_control_requested);
    EXPECT_EQ(
        request.blocked_action.reason,
        scau::coupling::core::FaultControllerBlockedActionReason::no_action_requested);
    EXPECT_EQ(
        request.outcome.outcome_kind,
        scau::coupling::core::FaultControllerPassiveActionOutcomeKind::not_requested);
    EXPECT_FALSE(request.scheduler_control_boundary_available);
    EXPECT_FALSE(request.threshold_evidence_complete);
    EXPECT_FALSE(request.operator_approved);
    EXPECT_FALSE(request.rollback_context_available);
    EXPECT_FALSE(request.replay_policy_available);
    EXPECT_FALSE(request.mass_audit_policy_available);
    EXPECT_FALSE(request.scheduler_control_allowed);
    EXPECT_FALSE(request.scheduler_control_used);
    EXPECT_FALSE(request.adapter_call_allowed);
    EXPECT_FALSE(request.exchange_requests_paused);
    EXPECT_FALSE(request.target_engine_request_skipped);
    EXPECT_FALSE(request.replay_held);
    EXPECT_FALSE(request.mass_audit_forced);
    EXPECT_FALSE(request.scheduler_state_mutated);
    EXPECT_FALSE(request.release_gate_action_executed);
}

TEST(CouplingFaultControllerSchedulerControlRequest, ReviewBlockedActionProducesObserveOnlyBoundaryAbsentRequest) {
    const auto blocked = make_blocked_action(
        scau::coupling::core::FaultControllerProposedActionState::review_required);

    const auto request = scau::coupling::core::make_fault_controller_scheduler_control_request(blocked);

    EXPECT_EQ(
        request.requested_kind,
        scau::coupling::core::FaultControllerSchedulerControlKind::observe_only);
    EXPECT_EQ(
        request.status,
        scau::coupling::core::FaultControllerSchedulerControlStatus::blocked_boundary_absent);
    EXPECT_EQ(
        request.reason,
        scau::coupling::core::FaultControllerSchedulerControlReason::scheduler_control_boundary_absent);
    EXPECT_EQ(
        request.blocked_action.reason,
        scau::coupling::core::FaultControllerBlockedActionReason::operator_review_required);
    EXPECT_EQ(
        request.outcome.outcome_kind,
        scau::coupling::core::FaultControllerPassiveActionOutcomeKind::operator_review_required);
    EXPECT_TRUE(request.outcome.operator_review_required);
    EXPECT_FALSE(request.scheduler_control_boundary_available);
    EXPECT_FALSE(request.scheduler_control_allowed);
    EXPECT_FALSE(request.scheduler_control_used);
    EXPECT_FALSE(request.adapter_call_allowed);
    EXPECT_FALSE(request.exchange_requests_paused);
    EXPECT_FALSE(request.target_engine_request_skipped);
    EXPECT_FALSE(request.replay_held);
    EXPECT_FALSE(request.mass_audit_forced);
    EXPECT_FALSE(request.scheduler_state_mutated);
    EXPECT_FALSE(request.release_gate_action_executed);
}

TEST(CouplingFaultControllerSchedulerControlRequest, RequestDoesNotPromoteBlockedActionOrOutcomeFlags) {
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
    const scau::coupling::core::FaultControllerBlockedAction blocked{
        .outcome = outcome,
        .reason = scau::coupling::core::FaultControllerBlockedActionReason::operator_review_required,
        .execution_allowed = true,
        .scheduler_control_allowed = true,
        .adapter_call_allowed = true,
        .isolation_allowed = true,
        .reconnect_allowed = true,
        .abort_allowed = true,
        .release_gate_action_allowed = true,
    };

    const auto request = scau::coupling::core::make_fault_controller_scheduler_control_request(blocked);

    EXPECT_TRUE(request.outcome.scheduler_control_available);
    EXPECT_TRUE(request.outcome.scheduler_control_used);
    EXPECT_TRUE(request.outcome.adapter_boundary_available);
    EXPECT_TRUE(request.outcome.adapter_call_attempted);
    EXPECT_TRUE(request.outcome.adapter_call_succeeded);
    EXPECT_TRUE(request.outcome.adapter_call_failed);
    EXPECT_TRUE(request.outcome.runtime_action_requested);
    EXPECT_TRUE(request.outcome.runtime_action_attempted);
    EXPECT_TRUE(request.outcome.runtime_action_executed);
    EXPECT_TRUE(request.outcome.runtime_action_skipped);
    EXPECT_TRUE(request.outcome.runtime_action_failed);
    EXPECT_TRUE(request.outcome.rollback_required);
    EXPECT_TRUE(request.outcome.replay_required);
    EXPECT_TRUE(request.outcome.mass_audit_required);
    EXPECT_TRUE(request.outcome.release_gate_action_executed);
    EXPECT_TRUE(request.blocked_action.execution_allowed);
    EXPECT_TRUE(request.blocked_action.scheduler_control_allowed);
    EXPECT_TRUE(request.blocked_action.adapter_call_allowed);
    EXPECT_TRUE(request.blocked_action.isolation_allowed);
    EXPECT_TRUE(request.blocked_action.reconnect_allowed);
    EXPECT_TRUE(request.blocked_action.abort_allowed);
    EXPECT_TRUE(request.blocked_action.release_gate_action_allowed);
    EXPECT_FALSE(request.scheduler_control_boundary_available);
    EXPECT_FALSE(request.threshold_evidence_complete);
    EXPECT_FALSE(request.operator_approved);
    EXPECT_FALSE(request.rollback_context_available);
    EXPECT_FALSE(request.replay_policy_available);
    EXPECT_FALSE(request.mass_audit_policy_available);
    EXPECT_FALSE(request.scheduler_control_allowed);
    EXPECT_FALSE(request.scheduler_control_used);
    EXPECT_FALSE(request.adapter_call_allowed);
    EXPECT_FALSE(request.exchange_requests_paused);
    EXPECT_FALSE(request.target_engine_request_skipped);
    EXPECT_FALSE(request.replay_held);
    EXPECT_FALSE(request.mass_audit_forced);
    EXPECT_FALSE(request.scheduler_state_mutated);
    EXPECT_FALSE(request.release_gate_action_executed);
}

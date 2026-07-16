#include <gtest/gtest.h>

#include "coupling/core/state.hpp"

namespace {

scau::coupling::core::FaultControllerPassiveTransition make_transition(
    scau::coupling::core::FaultControllerProposedActionState action_state) {
    const scau::coupling::core::FaultControllerProposedAction action{.state = action_state};
    const auto observation = scau::coupling::core::observe_mock_scheduler_fault_action(action);
    const auto consumption = scau::coupling::core::consume_mock_scheduler_fault_action(observation);
    const auto classification = scau::coupling::core::classify_fault_controller_passive_state(consumption);
    return scau::coupling::core::make_fault_controller_passive_transition(
        scau::coupling::core::FaultControllerPassiveStateLabel::running,
        classification);
}

}  // namespace

TEST(CouplingFaultControllerPassiveActionAudit, NominalTransitionProducesNoActionAudit) {
    const auto transition = make_transition(
        scau::coupling::core::FaultControllerProposedActionState::continue_run);

    const auto audit = scau::coupling::core::make_fault_controller_passive_action_audit_record(transition);

    EXPECT_EQ(audit.action_kind, scau::coupling::core::FaultControllerPassiveActionAuditKind::none);
    EXPECT_EQ(
        audit.action_stage,
        scau::coupling::core::FaultControllerPassiveActionAuditStage::transition_recorded);
    EXPECT_EQ(
        audit.reason,
        scau::coupling::core::FaultControllerPassiveActionAuditReason::nominal_no_action);
    EXPECT_EQ(audit.transition.next_state, scau::coupling::core::FaultControllerPassiveStateLabel::running);
    EXPECT_FALSE(audit.scheduler_control_enabled);
    EXPECT_FALSE(audit.runtime_action_requested);
    EXPECT_FALSE(audit.runtime_action_executed);
    EXPECT_FALSE(audit.isolation_executed);
    EXPECT_FALSE(audit.reconnect_executed);
    EXPECT_FALSE(audit.abort_executed);
    EXPECT_FALSE(audit.adapter_call_executed);
    EXPECT_FALSE(audit.release_gate_action_executed);
}

TEST(CouplingFaultControllerPassiveActionAudit, ReviewRequiredTransitionProducesOperatorReviewAudit) {
    const auto transition = make_transition(
        scau::coupling::core::FaultControllerProposedActionState::review_required);

    const auto audit = scau::coupling::core::make_fault_controller_passive_action_audit_record(transition);

    EXPECT_EQ(audit.action_kind, scau::coupling::core::FaultControllerPassiveActionAuditKind::operator_review);
    EXPECT_EQ(
        audit.action_stage,
        scau::coupling::core::FaultControllerPassiveActionAuditStage::transition_recorded);
    EXPECT_EQ(
        audit.reason,
        scau::coupling::core::FaultControllerPassiveActionAuditReason::review_required_only);
    EXPECT_EQ(
        audit.transition.next_state,
        scau::coupling::core::FaultControllerPassiveStateLabel::review_required);
    EXPECT_FALSE(audit.scheduler_control_enabled);
    EXPECT_FALSE(audit.runtime_action_requested);
    EXPECT_FALSE(audit.runtime_action_executed);
    EXPECT_FALSE(audit.isolation_executed);
    EXPECT_FALSE(audit.reconnect_executed);
    EXPECT_FALSE(audit.abort_executed);
    EXPECT_FALSE(audit.adapter_call_executed);
    EXPECT_FALSE(audit.release_gate_action_executed);
}

TEST(CouplingFaultControllerPassiveActionAudit, AuditDoesNotPromoteTransitionControlFlags) {
    const scau::coupling::core::FaultControllerPassiveTransition transition{
        .next_state = scau::coupling::core::FaultControllerPassiveStateLabel::review_required,
        .reason = scau::coupling::core::FaultControllerPassiveTransitionReason::fault_detected_review_required,
        .isolation_requested = true,
        .reconnect_requested = true,
        .abort_requested = true,
        .runtime_action_executed = true,
        .scheduler_control_enabled = true,
    };

    const auto audit = scau::coupling::core::make_fault_controller_passive_action_audit_record(transition);

    EXPECT_TRUE(audit.transition.isolation_requested);
    EXPECT_TRUE(audit.transition.reconnect_requested);
    EXPECT_TRUE(audit.transition.abort_requested);
    EXPECT_TRUE(audit.transition.runtime_action_executed);
    EXPECT_TRUE(audit.transition.scheduler_control_enabled);
    EXPECT_EQ(audit.action_kind, scau::coupling::core::FaultControllerPassiveActionAuditKind::operator_review);
    EXPECT_FALSE(audit.scheduler_control_enabled);
    EXPECT_FALSE(audit.runtime_action_requested);
    EXPECT_FALSE(audit.runtime_action_executed);
    EXPECT_FALSE(audit.isolation_executed);
    EXPECT_FALSE(audit.reconnect_executed);
    EXPECT_FALSE(audit.abort_executed);
    EXPECT_FALSE(audit.adapter_call_executed);
    EXPECT_FALSE(audit.release_gate_action_executed);
}

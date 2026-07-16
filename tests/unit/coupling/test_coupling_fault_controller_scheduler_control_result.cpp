#include <gtest/gtest.h>

#include "coupling/core/state.hpp"

namespace {

scau::coupling::core::FaultControllerSchedulerControlRequest make_request(
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
    const auto blocked = scau::coupling::core::make_fault_controller_blocked_action(outcome);
    return scau::coupling::core::make_fault_controller_scheduler_control_request(blocked);
}

}  // namespace

TEST(CouplingFaultControllerSchedulerControlResult, NominalRequestProducesNotRequestedPassiveResult) {
    const auto request = make_request(
        scau::coupling::core::FaultControllerProposedActionState::continue_run);

    const auto result = scau::coupling::core::make_fault_controller_scheduler_control_result(request);

    EXPECT_EQ(
        result.attempted_kind,
        scau::coupling::core::FaultControllerSchedulerControlKind::none);
    EXPECT_EQ(
        result.status,
        scau::coupling::core::FaultControllerSchedulerControlResultStatus::not_requested);
    EXPECT_EQ(
        result.reason,
        scau::coupling::core::FaultControllerSchedulerControlResultReason::no_scheduler_control_requested);
    EXPECT_EQ(
        result.request.status,
        scau::coupling::core::FaultControllerSchedulerControlStatus::not_requested);
    EXPECT_FALSE(result.scheduler_control_boundary_available);
    EXPECT_FALSE(result.threshold_evidence_complete);
    EXPECT_FALSE(result.operator_approved);
    EXPECT_FALSE(result.rollback_context_available);
    EXPECT_FALSE(result.replay_policy_available);
    EXPECT_FALSE(result.mass_audit_policy_available);
    EXPECT_FALSE(result.scheduler_control_used);
    EXPECT_FALSE(result.exchange_requests_paused);
    EXPECT_FALSE(result.target_engine_request_skipped);
    EXPECT_FALSE(result.replay_held);
    EXPECT_FALSE(result.mass_audit_forced);
    EXPECT_FALSE(result.scheduler_state_mutated);
    EXPECT_FALSE(result.adapter_call_attempted);
    EXPECT_FALSE(result.adapter_call_succeeded);
    EXPECT_FALSE(result.adapter_call_failed);
    EXPECT_FALSE(result.rollback_required);
    EXPECT_FALSE(result.replay_required);
    EXPECT_FALSE(result.mass_audit_required);
    EXPECT_FALSE(result.operator_review_required);
    EXPECT_FALSE(result.release_gate_action_executed);
}

TEST(CouplingFaultControllerSchedulerControlResult, ReviewRequestProducesBoundaryAbsentPassiveResult) {
    const auto request = make_request(
        scau::coupling::core::FaultControllerProposedActionState::review_required);

    const auto result = scau::coupling::core::make_fault_controller_scheduler_control_result(request);

    EXPECT_EQ(
        result.attempted_kind,
        scau::coupling::core::FaultControllerSchedulerControlKind::observe_only);
    EXPECT_EQ(
        result.status,
        scau::coupling::core::FaultControllerSchedulerControlResultStatus::blocked_boundary_absent);
    EXPECT_EQ(
        result.reason,
        scau::coupling::core::FaultControllerSchedulerControlResultReason::scheduler_control_boundary_absent);
    EXPECT_EQ(
        result.request.status,
        scau::coupling::core::FaultControllerSchedulerControlStatus::blocked_boundary_absent);
    EXPECT_TRUE(result.operator_review_required);
    EXPECT_FALSE(result.scheduler_control_boundary_available);
    EXPECT_FALSE(result.scheduler_control_used);
    EXPECT_FALSE(result.exchange_requests_paused);
    EXPECT_FALSE(result.target_engine_request_skipped);
    EXPECT_FALSE(result.replay_held);
    EXPECT_FALSE(result.mass_audit_forced);
    EXPECT_FALSE(result.scheduler_state_mutated);
    EXPECT_FALSE(result.adapter_call_attempted);
    EXPECT_FALSE(result.adapter_call_succeeded);
    EXPECT_FALSE(result.adapter_call_failed);
    EXPECT_FALSE(result.rollback_required);
    EXPECT_FALSE(result.replay_required);
    EXPECT_FALSE(result.mass_audit_required);
    EXPECT_FALSE(result.release_gate_action_executed);
}

TEST(CouplingFaultControllerSchedulerControlResult, ResultDoesNotPromoteRequestEffectFlags) {
    scau::coupling::core::FaultControllerSchedulerControlRequest request{
        .outcome = {
            .operator_review_required = true,
            .rollback_required = true,
            .replay_required = true,
            .mass_audit_required = true,
            .release_gate_action_executed = true,
        },
        .requested_kind = scau::coupling::core::FaultControllerSchedulerControlKind::observe_only,
        .status = scau::coupling::core::FaultControllerSchedulerControlStatus::blocked_boundary_absent,
        .reason = scau::coupling::core::FaultControllerSchedulerControlReason::scheduler_control_boundary_absent,
        .target_engine_id = "hostile_engine",
        .scheduler_control_boundary_available = true,
        .threshold_evidence_complete = true,
        .operator_approved = true,
        .rollback_context_available = true,
        .replay_policy_available = true,
        .mass_audit_policy_available = true,
        .scheduler_control_allowed = true,
        .scheduler_control_used = true,
        .adapter_call_allowed = true,
        .exchange_requests_paused = true,
        .target_engine_request_skipped = true,
        .replay_held = true,
        .mass_audit_forced = true,
        .scheduler_state_mutated = true,
        .release_gate_action_executed = true,
    };

    const auto result = scau::coupling::core::make_fault_controller_scheduler_control_result(request);

    EXPECT_EQ(result.request.target_engine_id, "hostile_engine");
    EXPECT_TRUE(result.request.scheduler_control_allowed);
    EXPECT_TRUE(result.request.scheduler_control_used);
    EXPECT_TRUE(result.request.adapter_call_allowed);
    EXPECT_TRUE(result.request.exchange_requests_paused);
    EXPECT_TRUE(result.request.target_engine_request_skipped);
    EXPECT_TRUE(result.request.replay_held);
    EXPECT_TRUE(result.request.mass_audit_forced);
    EXPECT_TRUE(result.request.scheduler_state_mutated);
    EXPECT_TRUE(result.request.release_gate_action_executed);
    EXPECT_TRUE(result.scheduler_control_boundary_available);
    EXPECT_TRUE(result.threshold_evidence_complete);
    EXPECT_TRUE(result.operator_approved);
    EXPECT_TRUE(result.rollback_context_available);
    EXPECT_TRUE(result.replay_policy_available);
    EXPECT_TRUE(result.mass_audit_policy_available);
    EXPECT_FALSE(result.scheduler_control_used);
    EXPECT_FALSE(result.exchange_requests_paused);
    EXPECT_FALSE(result.target_engine_request_skipped);
    EXPECT_FALSE(result.replay_held);
    EXPECT_FALSE(result.mass_audit_forced);
    EXPECT_FALSE(result.scheduler_state_mutated);
    EXPECT_FALSE(result.adapter_call_attempted);
    EXPECT_FALSE(result.adapter_call_succeeded);
    EXPECT_FALSE(result.adapter_call_failed);
    EXPECT_FALSE(result.rollback_required);
    EXPECT_FALSE(result.replay_required);
    EXPECT_FALSE(result.mass_audit_required);
    EXPECT_TRUE(result.operator_review_required);
    EXPECT_FALSE(result.release_gate_action_executed);
}

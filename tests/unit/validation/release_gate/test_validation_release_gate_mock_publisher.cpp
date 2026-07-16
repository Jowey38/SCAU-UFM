#include <gtest/gtest.h>

#include "validation/release_gate/mock_publisher.hpp"

namespace {

using scau::coupling::core::FaultControllerSchedulerExecutableControlKind;
using scau::coupling::core::FaultControllerSchedulerPhase;
using scau::coupling::core::FaultControllerSchedulerReleaseGateActionCompletionReason;
using scau::coupling::core::FaultControllerSchedulerReleaseGateActionCompletionRecord;
using scau::coupling::core::FaultControllerSchedulerReleaseGateActionCompletionStatus;
using scau::coupling::core::describe_fault_controller_scheduler_phase;
using scau::validation::release_gate::MockReleaseGatePublishedActionKind;
using scau::validation::release_gate::MockReleaseGatePublisherReason;
using scau::validation::release_gate::MockReleaseGatePublisherRequest;
using scau::validation::release_gate::MockReleaseGatePublisherStatus;
using scau::validation::release_gate::publish_mock_release_gate_action;

FaultControllerSchedulerReleaseGateActionCompletionRecord make_completion() {
    return {
        .status = FaultControllerSchedulerReleaseGateActionCompletionStatus::complete,
        .reason = FaultControllerSchedulerReleaseGateActionCompletionReason::action_emission_complete,
        .requested_control_kind = FaultControllerSchedulerExecutableControlKind::pause_before_phase,
        .target_phase = describe_fault_controller_scheduler_phase(FaultControllerSchedulerPhase::exchange_request_preparation),
        .mutation_generation_before = 7U,
        .mutation_generation_after = 8U,
        .evidence_record_id = "runtime-evidence:pause",
        .policy_reference_id = "release-gate-policy:fixture",
        .action_reference_id = "release-gate-action:pause",
        .action_transaction_id = "action-transaction:pause",
        .review_required = false,
        .merge_blocked = true,
        .release_blocked = false,
        .abort_required = false,
        .action_completed = true,
        .action_failed = false,
        .action_dry_run_only = false,
        .external_write_attempted = true,
        .adapter_call_attempted = false,
    };
}

MockReleaseGatePublisherRequest make_request() {
    return {
        .completion = make_completion(),
        .publisher_boundary_available = true,
        .operator_approved = true,
        .project_policy_reference_id = "release-gate-policy:fixture",
        .release_target_reference_id = "release-target:mock",
        .idempotency_key = "publisher-idempotency:pause",
        .action_transaction_id = "publisher-transaction:pause",
        .payload_reference_id = "publisher-payload:pause",
        .payload_content_hash = "sha256:publisher-pause",
        .publish_dry_run = false,
        .publish_commit = true,
        .publisher_sink_succeeded = true,
    };
}

void expect_blocked(
    const scau::validation::release_gate::MockReleaseGatePublisherResult& result,
    MockReleaseGatePublisherReason reason) {
    EXPECT_EQ(result.status, MockReleaseGatePublisherStatus::blocked);
    EXPECT_EQ(result.reason, reason);
    EXPECT_EQ(result.action_kind, MockReleaseGatePublishedActionKind::none);
    EXPECT_FALSE(result.publish_attempted);
    EXPECT_FALSE(result.publish_dry_run_recorded);
    EXPECT_FALSE(result.publish_succeeded);
    EXPECT_FALSE(result.publish_failed);
    EXPECT_FALSE(result.external_publish_performed);
    EXPECT_TRUE(result.action_reference_id.empty());
}

}  // namespace

TEST(ValidationReleaseGateMockPublisher, BlocksIncompleteAndMissingPublisherBoundary) {
    auto request = make_request();
    request.completion.action_completed = false;
    request.completion.status = FaultControllerSchedulerReleaseGateActionCompletionStatus::incomplete;
    expect_blocked(
        publish_mock_release_gate_action(request),
        MockReleaseGatePublisherReason::completion_not_complete);

    request = make_request();
    request.publisher_boundary_available = false;
    expect_blocked(
        publish_mock_release_gate_action(request),
        MockReleaseGatePublisherReason::publisher_boundary_absent);
}

TEST(ValidationReleaseGateMockPublisher, BlocksMissingApprovalPolicyTargetAndReferences) {
    auto request = make_request();
    request.operator_approved = false;
    expect_blocked(
        publish_mock_release_gate_action(request),
        MockReleaseGatePublisherReason::operator_approval_missing);

    request = make_request();
    request.project_policy_reference_id = "";
    expect_blocked(
        publish_mock_release_gate_action(request),
        MockReleaseGatePublisherReason::project_policy_missing);

    request = make_request();
    request.release_target_reference_id = "";
    expect_blocked(
        publish_mock_release_gate_action(request),
        MockReleaseGatePublisherReason::release_target_missing);

    request = make_request();
    request.idempotency_key = "";
    expect_blocked(
        publish_mock_release_gate_action(request),
        MockReleaseGatePublisherReason::idempotency_key_missing);

    request = make_request();
    request.action_transaction_id = "";
    expect_blocked(
        publish_mock_release_gate_action(request),
        MockReleaseGatePublisherReason::action_transaction_missing);

    request = make_request();
    request.payload_reference_id = "";
    expect_blocked(
        publish_mock_release_gate_action(request),
        MockReleaseGatePublisherReason::payload_reference_missing);

    request = make_request();
    request.payload_content_hash = "";
    expect_blocked(
        publish_mock_release_gate_action(request),
        MockReleaseGatePublisherReason::payload_hash_missing);
}

TEST(ValidationReleaseGateMockPublisher, BlocksMissingAndConflictingPublishModes) {
    auto request = make_request();
    request.publish_commit = false;
    expect_blocked(
        publish_mock_release_gate_action(request),
        MockReleaseGatePublisherReason::publish_mode_missing);

    request = make_request();
    request.publish_dry_run = true;
    request.publish_commit = true;
    expect_blocked(
        publish_mock_release_gate_action(request),
        MockReleaseGatePublisherReason::conflicting_publish_mode);
}

TEST(ValidationReleaseGateMockPublisher, ReviewRequiredForReviewCompletionAndContradictoryFlags) {
    auto request = make_request();
    request.completion.action_completed = false;
    request.completion.status = FaultControllerSchedulerReleaseGateActionCompletionStatus::review_required;
    auto result = publish_mock_release_gate_action(request);
    EXPECT_EQ(result.status, MockReleaseGatePublisherStatus::review_required);
    EXPECT_EQ(result.reason, MockReleaseGatePublisherReason::completion_requires_review);
    EXPECT_TRUE(result.review_required);
    EXPECT_FALSE(result.publish_attempted);
    EXPECT_FALSE(result.external_publish_performed);

    request = make_request();
    request.completion.review_required = true;
    request.completion.merge_blocked = true;
    result = publish_mock_release_gate_action(request);
    EXPECT_EQ(result.status, MockReleaseGatePublisherStatus::review_required);
    EXPECT_EQ(result.reason, MockReleaseGatePublisherReason::unsupported_action_combination);
    EXPECT_TRUE(result.review_required);
    EXPECT_FALSE(result.publish_attempted);
    EXPECT_FALSE(result.external_publish_performed);
}

TEST(ValidationReleaseGateMockPublisher, DryRunRecordsWithoutExternalPublish) {
    auto request = make_request();
    request.publish_dry_run = true;
    request.publish_commit = false;
    request.publisher_sink_succeeded = false;
    const auto result = publish_mock_release_gate_action(request);

    EXPECT_EQ(result.status, MockReleaseGatePublisherStatus::dry_run_recorded);
    EXPECT_EQ(result.reason, MockReleaseGatePublisherReason::dry_run_recorded);
    EXPECT_EQ(result.action_kind, MockReleaseGatePublishedActionKind::block_merge);
    EXPECT_EQ(result.action_reference_id, "release-gate-action:pause");
    EXPECT_TRUE(result.publish_dry_run_recorded);
    EXPECT_FALSE(result.publish_attempted);
    EXPECT_FALSE(result.publish_succeeded);
    EXPECT_FALSE(result.publish_failed);
    EXPECT_FALSE(result.external_publish_performed);
}

TEST(ValidationReleaseGateMockPublisher, CommitRecordsSuppliedPublishSuccessWithoutRealExternalPublish) {
    const auto result = publish_mock_release_gate_action(make_request());

    EXPECT_EQ(result.status, MockReleaseGatePublisherStatus::published);
    EXPECT_EQ(result.reason, MockReleaseGatePublisherReason::published_block_merge);
    EXPECT_EQ(result.action_kind, MockReleaseGatePublishedActionKind::block_merge);
    EXPECT_EQ(result.requested_control_kind, FaultControllerSchedulerExecutableControlKind::pause_before_phase);
    EXPECT_EQ(result.target_phase.phase, FaultControllerSchedulerPhase::exchange_request_preparation);
    EXPECT_EQ(result.mutation_generation_after, result.mutation_generation_before + 1U);
    EXPECT_EQ(result.evidence_record_id, "runtime-evidence:pause");
    EXPECT_EQ(result.policy_reference_id, "release-gate-policy:fixture");
    EXPECT_EQ(result.release_target_reference_id, "release-target:mock");
    EXPECT_EQ(result.idempotency_key, "publisher-idempotency:pause");
    EXPECT_EQ(result.payload_reference_id, "publisher-payload:pause");
    EXPECT_TRUE(result.merge_blocked);
    EXPECT_TRUE(result.publish_attempted);
    EXPECT_TRUE(result.publish_succeeded);
    EXPECT_FALSE(result.publish_failed);
    EXPECT_FALSE(result.external_publish_performed);
    EXPECT_FALSE(result.adapter_call_attempted);
}

TEST(ValidationReleaseGateMockPublisher, CommitFailureRecordsFailureWithoutRealExternalPublish) {
    auto request = make_request();
    request.publisher_sink_succeeded = false;
    const auto result = publish_mock_release_gate_action(request);

    EXPECT_EQ(result.status, MockReleaseGatePublisherStatus::failed);
    EXPECT_EQ(result.reason, MockReleaseGatePublisherReason::publish_failed);
    EXPECT_TRUE(result.publish_attempted);
    EXPECT_FALSE(result.publish_succeeded);
    EXPECT_TRUE(result.publish_failed);
    EXPECT_FALSE(result.external_publish_performed);
}

TEST(ValidationReleaseGateMockPublisher, PassNoActionPublishesNoBlockingAction) {
    auto request = make_request();
    request.completion.review_required = false;
    request.completion.merge_blocked = false;
    request.completion.release_blocked = false;
    request.completion.abort_required = false;
    const auto result = publish_mock_release_gate_action(request);

    EXPECT_EQ(result.status, MockReleaseGatePublisherStatus::published);
    EXPECT_EQ(result.reason, MockReleaseGatePublisherReason::pass_no_action);
    EXPECT_EQ(result.action_kind, MockReleaseGatePublishedActionKind::none);
    EXPECT_FALSE(result.review_required);
    EXPECT_FALSE(result.merge_blocked);
    EXPECT_FALSE(result.release_blocked);
    EXPECT_FALSE(result.abort_required);
    EXPECT_FALSE(result.external_publish_performed);
}

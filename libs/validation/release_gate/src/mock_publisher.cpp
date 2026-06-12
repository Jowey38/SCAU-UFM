#include "validation/release_gate/mock_publisher.hpp"

namespace scau::validation::release_gate {

namespace {

MockReleaseGatePublishedActionKind action_kind_from_completion(
    const FaultControllerSchedulerReleaseGateActionCompletionRecord& completion) {
    if (completion.abort_required) {
        return MockReleaseGatePublishedActionKind::abort;
    }
    if (completion.release_blocked) {
        return MockReleaseGatePublishedActionKind::block_release;
    }
    if (completion.merge_blocked) {
        return MockReleaseGatePublishedActionKind::block_merge;
    }
    if (completion.review_required) {
        return MockReleaseGatePublishedActionKind::review_required;
    }
    return MockReleaseGatePublishedActionKind::none;
}

bool has_contradictory_action_flags(
    const FaultControllerSchedulerReleaseGateActionCompletionRecord& completion) {
    const auto action_count = static_cast<int>(completion.review_required)
        + static_cast<int>(completion.merge_blocked)
        + static_cast<int>(completion.release_blocked)
        + static_cast<int>(completion.abort_required);
    return action_count > 1;
}

MockReleaseGatePublisherReason published_reason_for_action(MockReleaseGatePublishedActionKind action_kind) {
    switch (action_kind) {
    case MockReleaseGatePublishedActionKind::none:
        return MockReleaseGatePublisherReason::pass_no_action;
    case MockReleaseGatePublishedActionKind::review_required:
        return MockReleaseGatePublisherReason::published_review_required;
    case MockReleaseGatePublishedActionKind::block_merge:
        return MockReleaseGatePublisherReason::published_block_merge;
    case MockReleaseGatePublishedActionKind::block_release:
        return MockReleaseGatePublisherReason::published_block_release;
    case MockReleaseGatePublishedActionKind::abort:
        return MockReleaseGatePublisherReason::published_abort;
    }
    return MockReleaseGatePublisherReason::unsupported_action_combination;
}

bool has_contradictory_payload_flags(const MockReleaseGatePublisherPayloadRecord& payload) {
    const auto action_count = static_cast<int>(payload.review_required)
        + static_cast<int>(payload.merge_blocked)
        + static_cast<int>(payload.release_blocked)
        + static_cast<int>(payload.abort_required);
    return action_count > 1;
}

MockReleaseGatePublisherReason publisher_reason_from_payload_action(MockReleaseGatePublishedActionKind action_kind) {
    return published_reason_for_action(action_kind);
}

}  // namespace

MockReleaseGatePublisherResult publish_mock_release_gate_action(
    const MockReleaseGatePublisherRequest& request) {
    auto action_kind = action_kind_from_completion(request.completion);
    auto reason = published_reason_for_action(action_kind);
    if (!request.completion.action_completed) {
        reason = request.completion.status == scau::coupling::core::FaultControllerSchedulerReleaseGateActionCompletionStatus::review_required
            ? MockReleaseGatePublisherReason::completion_requires_review
            : MockReleaseGatePublisherReason::completion_not_complete;
    } else if (request.completion.action_failed) {
        reason = MockReleaseGatePublisherReason::action_failed;
    } else if (!request.publisher_boundary_available) {
        reason = MockReleaseGatePublisherReason::publisher_boundary_absent;
    } else if (!request.operator_approved) {
        reason = MockReleaseGatePublisherReason::operator_approval_missing;
    } else if (request.project_policy_reference_id.empty()) {
        reason = MockReleaseGatePublisherReason::project_policy_missing;
    } else if (request.release_target_reference_id.empty()) {
        reason = MockReleaseGatePublisherReason::release_target_missing;
    } else if (request.idempotency_key.empty()) {
        reason = MockReleaseGatePublisherReason::idempotency_key_missing;
    } else if (request.action_transaction_id.empty()) {
        reason = MockReleaseGatePublisherReason::action_transaction_missing;
    } else if (request.payload_reference_id.empty()) {
        reason = MockReleaseGatePublisherReason::payload_reference_missing;
    } else if (request.payload_content_hash.empty()) {
        reason = MockReleaseGatePublisherReason::payload_hash_missing;
    } else if (has_contradictory_action_flags(request.completion)) {
        reason = MockReleaseGatePublisherReason::unsupported_action_combination;
    } else if (!request.publish_dry_run && !request.publish_commit) {
        reason = MockReleaseGatePublisherReason::publish_mode_missing;
    } else if (request.publish_dry_run && request.publish_commit) {
        reason = MockReleaseGatePublisherReason::conflicting_publish_mode;
    } else if (request.publish_dry_run) {
        reason = MockReleaseGatePublisherReason::dry_run_recorded;
    } else if (!request.publisher_sink_succeeded) {
        reason = MockReleaseGatePublisherReason::publish_failed;
    }

    auto status = MockReleaseGatePublisherStatus::blocked;
    if (reason == MockReleaseGatePublisherReason::dry_run_recorded) {
        status = MockReleaseGatePublisherStatus::dry_run_recorded;
    } else if (reason == MockReleaseGatePublisherReason::publish_failed) {
        status = MockReleaseGatePublisherStatus::failed;
    } else if (reason == MockReleaseGatePublisherReason::completion_requires_review
        || reason == MockReleaseGatePublisherReason::unsupported_action_combination) {
        status = MockReleaseGatePublisherStatus::review_required;
    } else if (reason == MockReleaseGatePublisherReason::pass_no_action
        || reason == MockReleaseGatePublisherReason::published_review_required
        || reason == MockReleaseGatePublisherReason::published_block_merge
        || reason == MockReleaseGatePublisherReason::published_block_release
        || reason == MockReleaseGatePublisherReason::published_abort) {
        status = MockReleaseGatePublisherStatus::published;
    }

    const bool publish_attempted = request.publish_commit
        && status != MockReleaseGatePublisherStatus::blocked
        && status != MockReleaseGatePublisherStatus::review_required;
    return MockReleaseGatePublisherResult{
        .status = status,
        .reason = reason,
        .action_kind = status == MockReleaseGatePublisherStatus::blocked ? MockReleaseGatePublishedActionKind::none : action_kind,
        .requested_control_kind = request.completion.requested_control_kind,
        .target_phase = request.completion.target_phase,
        .mutation_generation_before = request.completion.mutation_generation_before,
        .mutation_generation_after = request.completion.mutation_generation_after,
        .evidence_record_id = request.completion.evidence_record_id,
        .policy_reference_id = request.project_policy_reference_id,
        .action_reference_id = status == MockReleaseGatePublisherStatus::blocked ? "" : request.completion.action_reference_id,
        .action_transaction_id = status == MockReleaseGatePublisherStatus::blocked ? "" : request.action_transaction_id,
        .release_target_reference_id = request.release_target_reference_id,
        .idempotency_key = request.idempotency_key,
        .payload_reference_id = request.payload_reference_id,
        .payload_content_hash = request.payload_content_hash,
        .review_required = request.completion.review_required || status == MockReleaseGatePublisherStatus::review_required,
        .merge_blocked = request.completion.merge_blocked,
        .release_blocked = request.completion.release_blocked,
        .abort_required = request.completion.abort_required,
        .publish_attempted = publish_attempted,
        .publish_dry_run_recorded = status == MockReleaseGatePublisherStatus::dry_run_recorded,
        .publish_succeeded = status == MockReleaseGatePublisherStatus::published && request.publish_commit,
        .publish_failed = status == MockReleaseGatePublisherStatus::failed,
        .external_publish_performed = false,
        .adapter_call_attempted = request.completion.adapter_call_attempted,
    };
}

MockReleaseGatePublisherPayloadRecord prepare_mock_release_gate_publisher_payload(
    const MockReleaseGatePublisherPayloadRequest& request) {
    auto reason = request.action_kind == MockReleaseGatePublishedActionKind::none
        ? MockReleaseGatePublisherPayloadReason::pass_no_action_payload_prepared
        : MockReleaseGatePublisherPayloadReason::payload_prepared;
    if (!request.completion.action_completed) {
        reason = request.completion.status == scau::coupling::core::FaultControllerSchedulerReleaseGateActionCompletionStatus::review_required
            ? MockReleaseGatePublisherPayloadReason::completion_requires_review
            : MockReleaseGatePublisherPayloadReason::completion_not_complete;
    } else if (request.schema_name.empty()) {
        reason = MockReleaseGatePublisherPayloadReason::schema_name_missing;
    } else if (request.schema_version.empty()) {
        reason = MockReleaseGatePublisherPayloadReason::schema_version_missing;
    } else if (request.payload_record_id.empty()) {
        reason = MockReleaseGatePublisherPayloadReason::payload_record_missing;
    } else if (request.release_target_reference_id.empty()) {
        reason = MockReleaseGatePublisherPayloadReason::release_target_missing;
    } else if (request.project_policy_reference_id.empty()) {
        reason = MockReleaseGatePublisherPayloadReason::project_policy_missing;
    } else if (request.idempotency_key.empty()) {
        reason = MockReleaseGatePublisherPayloadReason::idempotency_key_missing;
    } else if (request.payload_content_hash.empty()) {
        reason = MockReleaseGatePublisherPayloadReason::payload_hash_missing;
    } else if (!request.payload_reference_available) {
        reason = MockReleaseGatePublisherPayloadReason::payload_reference_unavailable;
    } else if (has_contradictory_action_flags(request.completion)) {
        reason = MockReleaseGatePublisherPayloadReason::unsupported_action_combination;
    } else if (request.action_kind != action_kind_from_completion(request.completion)) {
        reason = MockReleaseGatePublisherPayloadReason::action_kind_mismatch;
    } else if (request.action_kind != MockReleaseGatePublishedActionKind::none && request.action_reference_id.empty()) {
        reason = MockReleaseGatePublisherPayloadReason::action_reference_missing;
    } else if (request.action_kind != MockReleaseGatePublishedActionKind::none && request.action_transaction_id.empty()) {
        reason = MockReleaseGatePublisherPayloadReason::action_transaction_missing;
    }

    auto status = MockReleaseGatePublisherPayloadStatus::blocked;
    if (reason == MockReleaseGatePublisherPayloadReason::payload_prepared
        || reason == MockReleaseGatePublisherPayloadReason::pass_no_action_payload_prepared) {
        status = MockReleaseGatePublisherPayloadStatus::prepared;
    } else if (reason == MockReleaseGatePublisherPayloadReason::completion_requires_review
        || reason == MockReleaseGatePublisherPayloadReason::unsupported_action_combination) {
        status = MockReleaseGatePublisherPayloadStatus::review_required;
    }

    const bool payload_prepared = status == MockReleaseGatePublisherPayloadStatus::prepared;
    return MockReleaseGatePublisherPayloadRecord{
        .status = status,
        .reason = reason,
        .action_kind = payload_prepared ? request.action_kind : MockReleaseGatePublishedActionKind::none,
        .schema_name = request.schema_name,
        .schema_version = request.schema_version,
        .payload_record_id = payload_prepared ? request.payload_record_id : "",
        .requested_control_kind = request.completion.requested_control_kind,
        .target_phase = request.completion.target_phase,
        .mutation_generation_before = request.completion.mutation_generation_before,
        .mutation_generation_after = request.completion.mutation_generation_after,
        .evidence_record_id = request.completion.evidence_record_id,
        .policy_reference_id = request.project_policy_reference_id,
        .action_reference_id = payload_prepared ? request.action_reference_id : "",
        .action_transaction_id = payload_prepared ? request.action_transaction_id : "",
        .release_target_reference_id = request.release_target_reference_id,
        .idempotency_key = request.idempotency_key,
        .payload_content_hash = request.payload_content_hash,
        .review_required = request.completion.review_required || status == MockReleaseGatePublisherPayloadStatus::review_required,
        .merge_blocked = request.completion.merge_blocked,
        .release_blocked = request.completion.release_blocked,
        .abort_required = request.completion.abort_required,
        .payload_prepared = payload_prepared,
        .external_publish_performed = false,
        .adapter_call_attempted = request.completion.adapter_call_attempted,
    };
}

MockReleaseGatePublisherResult publish_mock_release_gate_payload(
    const MockReleaseGatePayloadPublishRequest& request) {
    auto reason = publisher_reason_from_payload_action(request.payload.action_kind);
    if (!request.payload.payload_prepared || request.payload.status != MockReleaseGatePublisherPayloadStatus::prepared) {
        reason = request.payload.status == MockReleaseGatePublisherPayloadStatus::review_required
            ? MockReleaseGatePublisherReason::completion_requires_review
            : MockReleaseGatePublisherReason::payload_reference_missing;
    } else if (request.payload.payload_record_id.empty()) {
        reason = MockReleaseGatePublisherReason::payload_reference_missing;
    } else if (request.payload.release_target_reference_id.empty()) {
        reason = MockReleaseGatePublisherReason::release_target_missing;
    } else if (request.payload.idempotency_key.empty()) {
        reason = MockReleaseGatePublisherReason::idempotency_key_missing;
    } else if (request.payload.payload_content_hash.empty()) {
        reason = MockReleaseGatePublisherReason::payload_hash_missing;
    } else if (has_contradictory_payload_flags(request.payload)) {
        reason = MockReleaseGatePublisherReason::unsupported_action_combination;
    } else if (!request.publish_dry_run && !request.publish_commit) {
        reason = MockReleaseGatePublisherReason::publish_mode_missing;
    } else if (request.publish_dry_run && request.publish_commit) {
        reason = MockReleaseGatePublisherReason::conflicting_publish_mode;
    } else if (request.publish_dry_run) {
        reason = MockReleaseGatePublisherReason::dry_run_recorded;
    } else if (!request.publisher_sink_succeeded) {
        reason = MockReleaseGatePublisherReason::publish_failed;
    }

    auto status = MockReleaseGatePublisherStatus::blocked;
    if (reason == MockReleaseGatePublisherReason::dry_run_recorded) {
        status = MockReleaseGatePublisherStatus::dry_run_recorded;
    } else if (reason == MockReleaseGatePublisherReason::publish_failed) {
        status = MockReleaseGatePublisherStatus::failed;
    } else if (reason == MockReleaseGatePublisherReason::completion_requires_review
        || reason == MockReleaseGatePublisherReason::unsupported_action_combination) {
        status = MockReleaseGatePublisherStatus::review_required;
    } else if (reason == MockReleaseGatePublisherReason::pass_no_action
        || reason == MockReleaseGatePublisherReason::published_review_required
        || reason == MockReleaseGatePublisherReason::published_block_merge
        || reason == MockReleaseGatePublisherReason::published_block_release
        || reason == MockReleaseGatePublisherReason::published_abort) {
        status = MockReleaseGatePublisherStatus::published;
    }

    const bool publish_attempted = request.publish_commit
        && status != MockReleaseGatePublisherStatus::blocked
        && status != MockReleaseGatePublisherStatus::review_required;
    return MockReleaseGatePublisherResult{
        .status = status,
        .reason = reason,
        .action_kind = status == MockReleaseGatePublisherStatus::blocked ? MockReleaseGatePublishedActionKind::none : request.payload.action_kind,
        .requested_control_kind = request.payload.requested_control_kind,
        .target_phase = request.payload.target_phase,
        .mutation_generation_before = request.payload.mutation_generation_before,
        .mutation_generation_after = request.payload.mutation_generation_after,
        .evidence_record_id = request.payload.evidence_record_id,
        .policy_reference_id = request.payload.policy_reference_id,
        .action_reference_id = status == MockReleaseGatePublisherStatus::blocked ? "" : request.payload.action_reference_id,
        .action_transaction_id = status == MockReleaseGatePublisherStatus::blocked ? "" : request.payload.action_transaction_id,
        .release_target_reference_id = request.payload.release_target_reference_id,
        .idempotency_key = request.payload.idempotency_key,
        .payload_reference_id = request.payload.payload_record_id,
        .payload_content_hash = request.payload.payload_content_hash,
        .review_required = request.payload.review_required || status == MockReleaseGatePublisherStatus::review_required,
        .merge_blocked = request.payload.merge_blocked,
        .release_blocked = request.payload.release_blocked,
        .abort_required = request.payload.abort_required,
        .publish_attempted = publish_attempted,
        .publish_dry_run_recorded = status == MockReleaseGatePublisherStatus::dry_run_recorded,
        .publish_succeeded = status == MockReleaseGatePublisherStatus::published && request.publish_commit,
        .publish_failed = status == MockReleaseGatePublisherStatus::failed,
        .external_publish_performed = false,
        .adapter_call_attempted = request.payload.adapter_call_attempted,
    };
}

MockReleaseGatePublisherChainAggregateRecord make_mock_release_gate_publisher_chain_aggregate(
    const MockReleaseGatePublisherChainAggregateRequest& request) {
    auto reason = MockReleaseGatePublisherChainAggregateReason::aggregate_complete;
    if (!request.payload.payload_prepared || request.payload.status == MockReleaseGatePublisherPayloadStatus::blocked) {
        reason = MockReleaseGatePublisherChainAggregateReason::payload_not_prepared;
    } else if (request.payload.status == MockReleaseGatePublisherPayloadStatus::review_required) {
        reason = MockReleaseGatePublisherChainAggregateReason::payload_requires_review;
    } else if (request.publish_result.status == MockReleaseGatePublisherStatus::blocked
        || request.publish_result.status == MockReleaseGatePublisherStatus::not_attempted) {
        reason = MockReleaseGatePublisherChainAggregateReason::publish_not_completed;
    } else if (request.publish_result.status == MockReleaseGatePublisherStatus::dry_run_recorded) {
        reason = MockReleaseGatePublisherChainAggregateReason::publish_dry_run_recorded;
    } else if (request.publish_result.status == MockReleaseGatePublisherStatus::failed) {
        reason = MockReleaseGatePublisherChainAggregateReason::publish_failed;
    } else if (request.publish_result.status == MockReleaseGatePublisherStatus::review_required) {
        reason = MockReleaseGatePublisherChainAggregateReason::aggregate_review_required;
    } else if (request.payload.payload_record_id != request.publish_result.payload_reference_id) {
        reason = MockReleaseGatePublisherChainAggregateReason::payload_record_mismatch;
    } else if (request.payload.schema_name != request.expected_schema_name) {
        reason = MockReleaseGatePublisherChainAggregateReason::schema_name_mismatch;
    } else if (request.payload.schema_version != request.expected_schema_version) {
        reason = MockReleaseGatePublisherChainAggregateReason::schema_version_mismatch;
    } else if (request.payload.release_target_reference_id != request.expected_release_target_reference_id
        || request.payload.release_target_reference_id != request.publish_result.release_target_reference_id) {
        reason = MockReleaseGatePublisherChainAggregateReason::release_target_mismatch;
    } else if (request.payload.idempotency_key != request.expected_idempotency_key
        || request.payload.idempotency_key != request.publish_result.idempotency_key) {
        reason = MockReleaseGatePublisherChainAggregateReason::idempotency_key_mismatch;
    } else if (request.payload.payload_content_hash.empty()
        || request.payload.payload_content_hash != request.publish_result.payload_content_hash) {
        reason = MockReleaseGatePublisherChainAggregateReason::payload_hash_missing;
    } else if (request.payload.action_kind != request.publish_result.action_kind) {
        reason = MockReleaseGatePublisherChainAggregateReason::action_kind_mismatch;
    } else if (request.payload.mutation_generation_before != request.publish_result.mutation_generation_before
        || request.payload.mutation_generation_after != request.publish_result.mutation_generation_after) {
        reason = MockReleaseGatePublisherChainAggregateReason::mutation_generation_mismatch;
    } else if (request.payload.review_required != request.publish_result.review_required
        || request.payload.merge_blocked != request.publish_result.merge_blocked
        || request.payload.release_blocked != request.publish_result.release_blocked
        || request.payload.abort_required != request.publish_result.abort_required) {
        reason = MockReleaseGatePublisherChainAggregateReason::action_flag_mismatch;
    } else if (request.payload.external_publish_performed || request.publish_result.external_publish_performed) {
        reason = MockReleaseGatePublisherChainAggregateReason::external_publish_detected;
    } else if (request.payload.adapter_call_attempted || request.publish_result.adapter_call_attempted) {
        reason = MockReleaseGatePublisherChainAggregateReason::adapter_call_detected;
    }

    auto status = MockReleaseGatePublisherChainAggregateStatus::incomplete;
    if (reason == MockReleaseGatePublisherChainAggregateReason::aggregate_complete) {
        status = MockReleaseGatePublisherChainAggregateStatus::complete;
    } else if (reason == MockReleaseGatePublisherChainAggregateReason::publish_dry_run_recorded) {
        status = MockReleaseGatePublisherChainAggregateStatus::dry_run_recorded;
    } else if (reason == MockReleaseGatePublisherChainAggregateReason::publish_failed) {
        status = MockReleaseGatePublisherChainAggregateStatus::failed;
    } else if (reason == MockReleaseGatePublisherChainAggregateReason::payload_requires_review
        || reason == MockReleaseGatePublisherChainAggregateReason::aggregate_review_required
        || reason == MockReleaseGatePublisherChainAggregateReason::external_publish_detected
        || reason == MockReleaseGatePublisherChainAggregateReason::adapter_call_detected) {
        status = MockReleaseGatePublisherChainAggregateStatus::review_required;
    }

    return MockReleaseGatePublisherChainAggregateRecord{
        .status = status,
        .reason = reason,
        .correlation_id = request.correlation_id,
        .payload_status = request.payload.status,
        .payload_reason = request.payload.reason,
        .publisher_status = request.publish_result.status,
        .publisher_reason = request.publish_result.reason,
        .action_kind = request.payload.action_kind,
        .schema_name = request.payload.schema_name,
        .schema_version = request.payload.schema_version,
        .payload_record_id = status == MockReleaseGatePublisherChainAggregateStatus::incomplete ? "" : request.payload.payload_record_id,
        .evidence_record_id = request.payload.evidence_record_id,
        .policy_reference_id = request.payload.policy_reference_id,
        .action_reference_id = request.payload.action_reference_id,
        .release_target_reference_id = request.payload.release_target_reference_id,
        .idempotency_key = request.payload.idempotency_key,
        .payload_content_hash = request.payload.payload_content_hash,
        .requested_control_kind = request.payload.requested_control_kind,
        .target_phase = request.payload.target_phase,
        .mutation_generation_before = request.payload.mutation_generation_before,
        .mutation_generation_after = request.payload.mutation_generation_after,
        .review_required = request.payload.review_required || status == MockReleaseGatePublisherChainAggregateStatus::review_required,
        .merge_blocked = request.payload.merge_blocked,
        .release_blocked = request.payload.release_blocked,
        .abort_required = request.payload.abort_required,
        .payload_prepared = request.payload.payload_prepared,
        .publish_attempted = request.publish_result.publish_attempted,
        .publish_succeeded = status == MockReleaseGatePublisherChainAggregateStatus::complete && request.publish_result.publish_succeeded,
        .publish_failed = status == MockReleaseGatePublisherChainAggregateStatus::failed && request.publish_result.publish_failed,
        .external_publish_performed = request.payload.external_publish_performed || request.publish_result.external_publish_performed,
        .adapter_call_attempted = request.payload.adapter_call_attempted || request.publish_result.adapter_call_attempted,
    };
}

MockSandboxSinkResponse submit_mock_sandbox_sink_request(const MockSandboxSinkRequest& request) {
    auto status = MockSandboxSinkStatus::committed;
    auto reason = MockSandboxSinkReason::payload_accepted;
    if (!request.credential_available) {
        status = MockSandboxSinkStatus::rejected;
        reason = MockSandboxSinkReason::missing_credential;
    } else if (!request.credential_valid) {
        status = MockSandboxSinkStatus::rejected;
        reason = MockSandboxSinkReason::invalid_credential;
    } else if (!request.payload_schema_valid) {
        status = MockSandboxSinkStatus::rejected;
        reason = MockSandboxSinkReason::payload_schema_invalid;
    } else if (!request.payload_hash_matches) {
        status = MockSandboxSinkStatus::rejected;
        reason = MockSandboxSinkReason::payload_hash_mismatch;
    } else if (!request.action_kind_supported) {
        status = MockSandboxSinkStatus::rejected;
        reason = MockSandboxSinkReason::unsupported_action_kind;
    } else if (!request.release_target_known) {
        status = MockSandboxSinkStatus::rejected;
        reason = MockSandboxSinkReason::release_target_unknown;
    } else if (!request.policy_reference_known) {
        status = MockSandboxSinkStatus::rejected;
        reason = MockSandboxSinkReason::policy_reference_unknown;
    } else if (request.duplicate_idempotency_key && request.duplicate_payload_hash_matches) {
        status = MockSandboxSinkStatus::duplicate_idempotent;
        reason = MockSandboxSinkReason::duplicate_idempotent;
    } else if (request.duplicate_idempotency_key && !request.duplicate_payload_hash_matches) {
        status = MockSandboxSinkStatus::duplicate_conflict;
        reason = MockSandboxSinkReason::duplicate_conflict;
    } else if (!request.sink_available) {
        status = MockSandboxSinkStatus::transient_failure;
        reason = MockSandboxSinkReason::sink_unavailable;
    } else if (request.sink_times_out) {
        status = MockSandboxSinkStatus::timeout;
        reason = MockSandboxSinkReason::sink_timeout;
    } else if (request.sink_internal_error) {
        status = MockSandboxSinkStatus::permanent_failure;
        reason = MockSandboxSinkReason::sink_internal_error;
    } else if (request.dry_run && !request.commit) {
        status = MockSandboxSinkStatus::dry_run_recorded;
        reason = MockSandboxSinkReason::dry_run_only;
    } else if (!request.dry_run && !request.commit) {
        status = MockSandboxSinkStatus::review_required;
        reason = MockSandboxSinkReason::operator_review_required;
    } else if (request.dry_run && request.commit) {
        status = MockSandboxSinkStatus::review_required;
        reason = MockSandboxSinkReason::operator_review_required;
    }

    const bool dry_run_recorded = status == MockSandboxSinkStatus::dry_run_recorded;
    const bool commit_succeeded = status == MockSandboxSinkStatus::committed
        || status == MockSandboxSinkStatus::duplicate_idempotent;
    const bool commit_attempted = request.commit
        && !dry_run_recorded
        && status != MockSandboxSinkStatus::review_required;
    const bool commit_failed = status == MockSandboxSinkStatus::rejected
        || status == MockSandboxSinkStatus::duplicate_conflict
        || status == MockSandboxSinkStatus::transient_failure
        || status == MockSandboxSinkStatus::permanent_failure
        || status == MockSandboxSinkStatus::timeout;
    return MockSandboxSinkResponse{
        .status = status,
        .reason = reason,
        .sink_transaction_id = request.sink_transaction_id,
        .idempotency_key = request.idempotency_key,
        .accepted_payload_record_id = commit_failed || status == MockSandboxSinkStatus::review_required ? "" : request.payload_record_id,
        .accepted_payload_content_hash = commit_failed || status == MockSandboxSinkStatus::review_required ? "" : request.payload_content_hash,
        .observed_action_kind = commit_failed || status == MockSandboxSinkStatus::review_required ? MockReleaseGatePublishedActionKind::none : request.action_kind,
        .dry_run_recorded = dry_run_recorded,
        .commit_attempted = commit_attempted,
        .commit_succeeded = commit_succeeded,
        .commit_failed = commit_failed,
        .duplicate_detected = status == MockSandboxSinkStatus::duplicate_idempotent || status == MockSandboxSinkStatus::duplicate_conflict,
        .operator_review_required = status == MockSandboxSinkStatus::review_required || commit_failed,
        .sink_diagnostic_reference_id = "mock-sandbox-sink:diagnostic",
    };
}

MockSandboxPublisherAdapterResult publish_mock_sandbox_payload(
    const MockSandboxPublisherAdapterRequest& request) {
    auto reason = MockSandboxPublisherAdapterReason::published;
    if (!request.payload.payload_prepared || request.payload.status != MockReleaseGatePublisherPayloadStatus::prepared) {
        reason = MockSandboxPublisherAdapterReason::payload_not_prepared;
    } else if (request.sink_transaction_id.empty()) {
        reason = MockSandboxPublisherAdapterReason::sink_transaction_missing;
    } else if (request.credential_reference_id.empty()) {
        reason = MockSandboxPublisherAdapterReason::credential_reference_missing;
    } else if (request.operator_approval_reference_id.empty()) {
        reason = MockSandboxPublisherAdapterReason::operator_approval_missing;
    } else if (request.payload.payload_record_id.empty()) {
        reason = MockSandboxPublisherAdapterReason::payload_record_missing;
    } else if (request.payload.schema_name.empty() || request.payload.schema_version.empty()) {
        reason = MockSandboxPublisherAdapterReason::schema_missing;
    } else if (request.payload.payload_content_hash.empty()) {
        reason = MockSandboxPublisherAdapterReason::payload_hash_missing;
    } else if (request.payload.release_target_reference_id.empty()) {
        reason = MockSandboxPublisherAdapterReason::release_target_missing;
    } else if (request.payload.policy_reference_id.empty()) {
        reason = MockSandboxPublisherAdapterReason::policy_reference_missing;
    } else if (request.payload.idempotency_key.empty()) {
        reason = MockSandboxPublisherAdapterReason::idempotency_key_missing;
    } else if (!request.dry_run && !request.commit) {
        reason = MockSandboxPublisherAdapterReason::publish_mode_missing;
    } else if (request.dry_run && request.commit) {
        reason = MockSandboxPublisherAdapterReason::conflicting_publish_mode;
    }

    MockSandboxSinkRequest sink_request{};
    MockSandboxSinkResponse sink_response{};
    if (reason == MockSandboxPublisherAdapterReason::published) {
        sink_request = MockSandboxSinkRequest{
            .sink_transaction_id = request.sink_transaction_id,
            .idempotency_key = request.payload.idempotency_key,
            .dry_run = request.dry_run,
            .commit = request.commit,
            .payload_record_id = request.payload.payload_record_id,
            .payload_schema_name = request.payload.schema_name,
            .payload_schema_version = request.payload.schema_version,
            .action_kind = request.payload.action_kind,
            .release_target_reference_id = request.payload.release_target_reference_id,
            .policy_reference_id = request.payload.policy_reference_id,
            .evidence_record_id = request.payload.evidence_record_id,
            .payload_content_hash = request.payload.payload_content_hash,
            .payload_body_reference = request.payload.payload_record_id,
            .credential_reference_id = request.credential_reference_id,
            .operator_approval_reference_id = request.operator_approval_reference_id,
            .credential_available = request.credential_available,
            .credential_valid = request.credential_valid,
            .payload_schema_valid = request.payload_schema_valid,
            .payload_hash_matches = request.payload_hash_matches,
            .action_kind_supported = request.action_kind_supported,
            .release_target_known = request.release_target_known,
            .policy_reference_known = request.policy_reference_known,
            .duplicate_idempotency_key = request.duplicate_idempotency_key,
            .duplicate_payload_hash_matches = request.duplicate_payload_hash_matches,
            .sink_available = request.sink_available,
            .sink_times_out = request.sink_times_out,
            .sink_internal_error = request.sink_internal_error,
        };
        sink_response = submit_mock_sandbox_sink_request(sink_request);
        if (sink_response.sink_transaction_id != sink_request.sink_transaction_id) {
            reason = MockSandboxPublisherAdapterReason::sink_transaction_mismatch;
        } else if (sink_response.idempotency_key != sink_request.idempotency_key) {
            reason = MockSandboxPublisherAdapterReason::sink_idempotency_mismatch;
        } else if ((sink_response.status == MockSandboxSinkStatus::committed
                || sink_response.status == MockSandboxSinkStatus::duplicate_idempotent
                || sink_response.status == MockSandboxSinkStatus::dry_run_recorded)
            && sink_response.accepted_payload_record_id != sink_request.payload_record_id) {
            reason = MockSandboxPublisherAdapterReason::sink_payload_mismatch;
        } else if ((sink_response.status == MockSandboxSinkStatus::committed
                || sink_response.status == MockSandboxSinkStatus::duplicate_idempotent
                || sink_response.status == MockSandboxSinkStatus::dry_run_recorded)
            && sink_response.accepted_payload_content_hash != sink_request.payload_content_hash) {
            reason = MockSandboxPublisherAdapterReason::sink_hash_mismatch;
        } else if ((sink_response.status == MockSandboxSinkStatus::committed
                || sink_response.status == MockSandboxSinkStatus::duplicate_idempotent
                || sink_response.status == MockSandboxSinkStatus::dry_run_recorded)
            && sink_response.observed_action_kind != sink_request.action_kind) {
            reason = MockSandboxPublisherAdapterReason::sink_action_mismatch;
        } else if (sink_response.status == MockSandboxSinkStatus::rejected) {
            reason = MockSandboxPublisherAdapterReason::sink_rejected;
        } else if (sink_response.status == MockSandboxSinkStatus::duplicate_conflict) {
            reason = MockSandboxPublisherAdapterReason::sink_duplicate_conflict;
        } else if (sink_response.status == MockSandboxSinkStatus::transient_failure) {
            reason = MockSandboxPublisherAdapterReason::sink_transient_failure;
        } else if (sink_response.status == MockSandboxSinkStatus::permanent_failure) {
            reason = MockSandboxPublisherAdapterReason::sink_permanent_failure;
        } else if (sink_response.status == MockSandboxSinkStatus::timeout) {
            reason = MockSandboxPublisherAdapterReason::sink_timeout;
        } else if (sink_response.status == MockSandboxSinkStatus::review_required) {
            reason = MockSandboxPublisherAdapterReason::sink_review_required;
        } else if (request.commit && sink_response.dry_run_recorded) {
            reason = MockSandboxPublisherAdapterReason::sink_review_required;
        } else if (request.dry_run && sink_response.commit_succeeded) {
            reason = MockSandboxPublisherAdapterReason::sink_review_required;
        } else if (sink_response.status == MockSandboxSinkStatus::dry_run_recorded) {
            reason = MockSandboxPublisherAdapterReason::dry_run_recorded;
        } else if (sink_response.status == MockSandboxSinkStatus::duplicate_idempotent) {
            reason = MockSandboxPublisherAdapterReason::duplicate_idempotent;
        }
    }

    auto status = MockSandboxPublisherAdapterStatus::blocked;
    if (reason == MockSandboxPublisherAdapterReason::published
        || reason == MockSandboxPublisherAdapterReason::duplicate_idempotent) {
        status = MockSandboxPublisherAdapterStatus::published;
    } else if (reason == MockSandboxPublisherAdapterReason::dry_run_recorded) {
        status = MockSandboxPublisherAdapterStatus::dry_run_recorded;
    } else if (reason == MockSandboxPublisherAdapterReason::sink_rejected
        || reason == MockSandboxPublisherAdapterReason::sink_duplicate_conflict
        || reason == MockSandboxPublisherAdapterReason::sink_transient_failure
        || reason == MockSandboxPublisherAdapterReason::sink_permanent_failure
        || reason == MockSandboxPublisherAdapterReason::sink_timeout) {
        status = MockSandboxPublisherAdapterStatus::failed;
    } else if (reason == MockSandboxPublisherAdapterReason::sink_review_required
        || reason == MockSandboxPublisherAdapterReason::sink_transaction_mismatch
        || reason == MockSandboxPublisherAdapterReason::sink_idempotency_mismatch
        || reason == MockSandboxPublisherAdapterReason::sink_payload_mismatch
        || reason == MockSandboxPublisherAdapterReason::sink_hash_mismatch
        || reason == MockSandboxPublisherAdapterReason::sink_action_mismatch) {
        status = MockSandboxPublisherAdapterStatus::review_required;
    }

    return MockSandboxPublisherAdapterResult{
        .status = status,
        .reason = reason,
        .payload = request.payload,
        .sink_request = sink_request,
        .sink_response = sink_response,
        .action_kind = status == MockSandboxPublisherAdapterStatus::blocked ? MockReleaseGatePublishedActionKind::none : request.payload.action_kind,
        .payload_record_id = status == MockSandboxPublisherAdapterStatus::blocked ? "" : request.payload.payload_record_id,
        .sink_transaction_id = sink_request.sink_transaction_id,
        .idempotency_key = sink_request.idempotency_key,
        .payload_content_hash = request.payload.payload_content_hash,
        .dry_run_recorded = status == MockSandboxPublisherAdapterStatus::dry_run_recorded,
        .commit_attempted = sink_response.commit_attempted,
        .commit_succeeded = status == MockSandboxPublisherAdapterStatus::published && sink_response.commit_succeeded,
        .commit_failed = status == MockSandboxPublisherAdapterStatus::failed,
        .duplicate_detected = sink_response.duplicate_detected,
        .review_required = status == MockSandboxPublisherAdapterStatus::review_required || sink_response.operator_review_required,
        .external_publish_performed = false,
        .adapter_call_attempted = false,
    };
}

MockSandboxPublisherStageRecord consume_mock_sandbox_publisher_adapter_result(
    const MockSandboxPublisherAdapterResult& result) {
    auto status = MockSandboxPublisherStageRecordStatus::blocked;
    auto reason = MockSandboxPublisherStageRecordReason::pre_submit_blocked;
    switch (result.status) {
    case MockSandboxPublisherAdapterStatus::not_attempted:
        status = MockSandboxPublisherStageRecordStatus::blocked;
        reason = MockSandboxPublisherStageRecordReason::not_attempted_default;
        break;
    case MockSandboxPublisherAdapterStatus::blocked:
        status = MockSandboxPublisherStageRecordStatus::blocked;
        reason = MockSandboxPublisherStageRecordReason::pre_submit_blocked;
        break;
    case MockSandboxPublisherAdapterStatus::dry_run_recorded:
        status = MockSandboxPublisherStageRecordStatus::recorded;
        reason = MockSandboxPublisherStageRecordReason::dry_run_recorded;
        break;
    case MockSandboxPublisherAdapterStatus::published:
        status = MockSandboxPublisherStageRecordStatus::published;
        reason = result.reason == MockSandboxPublisherAdapterReason::duplicate_idempotent
            ? MockSandboxPublisherStageRecordReason::duplicate_idempotent_published
            : MockSandboxPublisherStageRecordReason::commit_published;
        break;
    case MockSandboxPublisherAdapterStatus::failed:
        status = MockSandboxPublisherStageRecordStatus::failed;
        reason = result.reason == MockSandboxPublisherAdapterReason::sink_duplicate_conflict
            ? MockSandboxPublisherStageRecordReason::duplicate_conflict_failed
            : MockSandboxPublisherStageRecordReason::sink_failed;
        break;
    case MockSandboxPublisherAdapterStatus::review_required:
        status = MockSandboxPublisherStageRecordStatus::review_required;
        reason = result.reason == MockSandboxPublisherAdapterReason::sink_review_required
            ? MockSandboxPublisherStageRecordReason::sink_review_required
            : MockSandboxPublisherStageRecordReason::metadata_mismatch_review;
        break;
    }

    return MockSandboxPublisherStageRecord{
        .status = status,
        .reason = reason,
        .adapter_status = result.status,
        .adapter_reason = result.reason,
        .action_kind = result.action_kind,
        .payload_record_id = result.payload_record_id,
        .payload_schema_name = result.payload.schema_name,
        .payload_schema_version = result.payload.schema_version,
        .release_target_reference_id = result.payload.release_target_reference_id,
        .policy_reference_id = result.payload.policy_reference_id,
        .evidence_record_id = result.payload.evidence_record_id,
        .sink_transaction_id = result.sink_transaction_id,
        .idempotency_key = result.idempotency_key,
        .payload_content_hash = result.payload_content_hash,
        .dry_run = status == MockSandboxPublisherStageRecordStatus::recorded,
        .commit = result.commit_attempted,
        .publish_attempted = result.commit_attempted,
        .publish_recorded = result.dry_run_recorded,
        .publish_succeeded = result.commit_succeeded,
        .publish_failed = result.commit_failed,
        .duplicate_detected = result.duplicate_detected,
        .review_required = result.review_required
            || status == MockSandboxPublisherStageRecordStatus::review_required,
        .external_publish_performed = false,
        .adapter_call_attempted = false,
        .scheduler_control_used = false,
        .release_gate_action_executed = false,
    };
}

namespace {

MockSandboxPublisherStageAggregateReason aggregate_reason_for_stage(
    MockSandboxPublisherStageRecordStatus status) {
    switch (status) {
    case MockSandboxPublisherStageRecordStatus::blocked:
        return MockSandboxPublisherStageAggregateReason::stage_blocked;
    case MockSandboxPublisherStageRecordStatus::recorded:
        return MockSandboxPublisherStageAggregateReason::stage_dry_run_recorded;
    case MockSandboxPublisherStageRecordStatus::published:
        return MockSandboxPublisherStageAggregateReason::aggregate_complete;
    case MockSandboxPublisherStageRecordStatus::failed:
        return MockSandboxPublisherStageAggregateReason::stage_failed;
    case MockSandboxPublisherStageRecordStatus::review_required:
        return MockSandboxPublisherStageAggregateReason::stage_review_required;
    }
    return MockSandboxPublisherStageAggregateReason::stage_review_required;
}

MockSandboxPublisherStageAggregateStatus aggregate_status_for_reason(
    MockSandboxPublisherStageAggregateReason reason) {
    switch (reason) {
    case MockSandboxPublisherStageAggregateReason::aggregate_complete:
        return MockSandboxPublisherStageAggregateStatus::complete;
    case MockSandboxPublisherStageAggregateReason::stage_dry_run_recorded:
        return MockSandboxPublisherStageAggregateStatus::dry_run_recorded;
    case MockSandboxPublisherStageAggregateReason::stage_failed:
        return MockSandboxPublisherStageAggregateStatus::failed;
    case MockSandboxPublisherStageAggregateReason::stage_blocked:
    case MockSandboxPublisherStageAggregateReason::correlation_id_missing:
        return MockSandboxPublisherStageAggregateStatus::incomplete;
    case MockSandboxPublisherStageAggregateReason::adapter_stage_status_mismatch:
    case MockSandboxPublisherStageAggregateReason::adapter_stage_reason_mismatch:
    case MockSandboxPublisherStageAggregateReason::action_kind_mismatch:
    case MockSandboxPublisherStageAggregateReason::payload_record_mismatch:
    case MockSandboxPublisherStageAggregateReason::schema_name_mismatch:
    case MockSandboxPublisherStageAggregateReason::schema_version_mismatch:
    case MockSandboxPublisherStageAggregateReason::release_target_mismatch:
    case MockSandboxPublisherStageAggregateReason::policy_reference_mismatch:
    case MockSandboxPublisherStageAggregateReason::evidence_record_mismatch:
    case MockSandboxPublisherStageAggregateReason::sink_transaction_mismatch:
    case MockSandboxPublisherStageAggregateReason::idempotency_key_mismatch:
    case MockSandboxPublisherStageAggregateReason::payload_hash_missing:
    case MockSandboxPublisherStageAggregateReason::duplicate_flag_mismatch:
    case MockSandboxPublisherStageAggregateReason::review_required_flag_mismatch:
    case MockSandboxPublisherStageAggregateReason::publish_flag_mismatch:
    case MockSandboxPublisherStageAggregateReason::external_publish_detected:
    case MockSandboxPublisherStageAggregateReason::adapter_call_detected:
    case MockSandboxPublisherStageAggregateReason::scheduler_control_detected:
    case MockSandboxPublisherStageAggregateReason::release_gate_action_detected:
    case MockSandboxPublisherStageAggregateReason::stage_review_required:
        return MockSandboxPublisherStageAggregateStatus::review_required;
    }
    return MockSandboxPublisherStageAggregateStatus::review_required;
}

}  // namespace

MockSandboxPublisherStageAggregateRecord make_mock_sandbox_publisher_stage_aggregate(
    const MockSandboxPublisherStageAggregateRequest& request) {
    auto reason = aggregate_reason_for_stage(request.stage.status);
    if (request.correlation_id.empty()) {
        reason = MockSandboxPublisherStageAggregateReason::correlation_id_missing;
    } else if (request.adapter.status != request.stage.adapter_status) {
        reason = MockSandboxPublisherStageAggregateReason::adapter_stage_status_mismatch;
    } else if (request.adapter.reason != request.stage.adapter_reason) {
        reason = MockSandboxPublisherStageAggregateReason::adapter_stage_reason_mismatch;
    } else if (request.adapter.action_kind != request.stage.action_kind) {
        reason = MockSandboxPublisherStageAggregateReason::action_kind_mismatch;
    } else if (request.adapter.payload_record_id != request.stage.payload_record_id) {
        reason = MockSandboxPublisherStageAggregateReason::payload_record_mismatch;
    } else if (request.adapter.sink_transaction_id != request.stage.sink_transaction_id) {
        reason = MockSandboxPublisherStageAggregateReason::sink_transaction_mismatch;
    } else if (request.adapter.duplicate_detected != request.stage.duplicate_detected) {
        reason = MockSandboxPublisherStageAggregateReason::duplicate_flag_mismatch;
    } else if (request.adapter.review_required != request.stage.review_required
        && request.stage.status != MockSandboxPublisherStageRecordStatus::review_required) {
        reason = MockSandboxPublisherStageAggregateReason::review_required_flag_mismatch;
    } else if (request.adapter.dry_run_recorded != request.stage.publish_recorded
        || request.adapter.commit_attempted != request.stage.publish_attempted
        || request.adapter.commit_succeeded != request.stage.publish_succeeded
        || request.adapter.commit_failed != request.stage.publish_failed) {
        reason = MockSandboxPublisherStageAggregateReason::publish_flag_mismatch;
    } else if (request.adapter.external_publish_performed || request.stage.external_publish_performed) {
        reason = MockSandboxPublisherStageAggregateReason::external_publish_detected;
    } else if (request.adapter.adapter_call_attempted || request.stage.adapter_call_attempted) {
        reason = MockSandboxPublisherStageAggregateReason::adapter_call_detected;
    } else if (request.stage.scheduler_control_used) {
        reason = MockSandboxPublisherStageAggregateReason::scheduler_control_detected;
    } else if (request.stage.release_gate_action_executed) {
        reason = MockSandboxPublisherStageAggregateReason::release_gate_action_detected;
    } else if (request.stage.status == MockSandboxPublisherStageRecordStatus::published) {
        if (request.adapter.status != MockSandboxPublisherAdapterStatus::published
            || !request.stage.publish_succeeded
            || !request.adapter.commit_succeeded) {
            reason = MockSandboxPublisherStageAggregateReason::publish_flag_mismatch;
        } else if (request.stage.payload_schema_name != request.expected_schema_name) {
            reason = MockSandboxPublisherStageAggregateReason::schema_name_mismatch;
        } else if (request.stage.payload_schema_version != request.expected_schema_version) {
            reason = MockSandboxPublisherStageAggregateReason::schema_version_mismatch;
        } else if (request.stage.release_target_reference_id != request.expected_release_target_reference_id) {
            reason = MockSandboxPublisherStageAggregateReason::release_target_mismatch;
        } else if (request.stage.policy_reference_id != request.expected_policy_reference_id) {
            reason = MockSandboxPublisherStageAggregateReason::policy_reference_mismatch;
        } else if (request.stage.evidence_record_id != request.expected_evidence_record_id) {
            reason = MockSandboxPublisherStageAggregateReason::evidence_record_mismatch;
        } else if (request.stage.idempotency_key != request.expected_idempotency_key) {
            reason = MockSandboxPublisherStageAggregateReason::idempotency_key_mismatch;
        } else if (request.stage.payload_content_hash.empty()) {
            reason = MockSandboxPublisherStageAggregateReason::payload_hash_missing;
        }
    }

    const auto status = aggregate_status_for_reason(reason);
    return MockSandboxPublisherStageAggregateRecord{
        .status = status,
        .reason = reason,
        .correlation_id = request.correlation_id,
        .adapter_status = request.adapter.status,
        .adapter_reason = request.adapter.reason,
        .stage_status = request.stage.status,
        .stage_reason = request.stage.reason,
        .action_kind = request.stage.action_kind,
        .payload_record_id = request.stage.payload_record_id,
        .payload_schema_name = request.stage.payload_schema_name,
        .payload_schema_version = request.stage.payload_schema_version,
        .release_target_reference_id = request.stage.release_target_reference_id,
        .policy_reference_id = request.stage.policy_reference_id,
        .evidence_record_id = request.stage.evidence_record_id,
        .sink_transaction_id = request.stage.sink_transaction_id,
        .idempotency_key = request.stage.idempotency_key,
        .payload_content_hash = request.stage.payload_content_hash,
        .dry_run = request.stage.dry_run,
        .commit = request.stage.commit,
        .publish_attempted = request.stage.publish_attempted,
        .publish_recorded = request.stage.publish_recorded,
        .publish_succeeded = request.stage.publish_succeeded,
        .publish_failed = request.stage.publish_failed,
        .duplicate_detected = request.stage.duplicate_detected,
        .review_required = request.stage.review_required || status == MockSandboxPublisherStageAggregateStatus::review_required,
        .external_publish_performed = false,
        .adapter_call_attempted = false,
        .scheduler_control_used = false,
        .release_gate_action_executed = false,
    };
}

namespace {

MockSandboxPublisherFinalChainReason final_chain_reason_for_aggregate(
    MockSandboxPublisherStageAggregateStatus status) {
    switch (status) {
    case MockSandboxPublisherStageAggregateStatus::incomplete:
        return MockSandboxPublisherFinalChainReason::aggregate_incomplete;
    case MockSandboxPublisherStageAggregateStatus::dry_run_recorded:
        return MockSandboxPublisherFinalChainReason::aggregate_dry_run_recorded;
    case MockSandboxPublisherStageAggregateStatus::complete:
        return MockSandboxPublisherFinalChainReason::final_chain_complete;
    case MockSandboxPublisherStageAggregateStatus::failed:
        return MockSandboxPublisherFinalChainReason::aggregate_failed;
    case MockSandboxPublisherStageAggregateStatus::review_required:
        return MockSandboxPublisherFinalChainReason::aggregate_review_required;
    }
    return MockSandboxPublisherFinalChainReason::aggregate_review_required;
}

MockSandboxPublisherFinalChainStatus final_chain_status_for_reason(
    MockSandboxPublisherFinalChainReason reason) {
    switch (reason) {
    case MockSandboxPublisherFinalChainReason::final_chain_complete:
        return MockSandboxPublisherFinalChainStatus::complete;
    case MockSandboxPublisherFinalChainReason::aggregate_dry_run_recorded:
        return MockSandboxPublisherFinalChainStatus::dry_run_recorded;
    case MockSandboxPublisherFinalChainReason::aggregate_failed:
        return MockSandboxPublisherFinalChainStatus::failed;
    case MockSandboxPublisherFinalChainReason::aggregate_incomplete:
    case MockSandboxPublisherFinalChainReason::chain_correlation_id_missing:
    case MockSandboxPublisherFinalChainReason::aggregate_correlation_id_missing:
        return MockSandboxPublisherFinalChainStatus::incomplete;
    case MockSandboxPublisherFinalChainReason::aggregate_correlation_mismatch:
    case MockSandboxPublisherFinalChainReason::schema_name_mismatch:
    case MockSandboxPublisherFinalChainReason::schema_version_mismatch:
    case MockSandboxPublisherFinalChainReason::release_target_mismatch:
    case MockSandboxPublisherFinalChainReason::policy_reference_mismatch:
    case MockSandboxPublisherFinalChainReason::evidence_record_mismatch:
    case MockSandboxPublisherFinalChainReason::idempotency_key_mismatch:
    case MockSandboxPublisherFinalChainReason::payload_hash_missing:
    case MockSandboxPublisherFinalChainReason::payload_hash_mismatch:
    case MockSandboxPublisherFinalChainReason::publish_flag_mismatch:
    case MockSandboxPublisherFinalChainReason::review_required_flag_mismatch:
    case MockSandboxPublisherFinalChainReason::external_publish_detected:
    case MockSandboxPublisherFinalChainReason::adapter_call_detected:
    case MockSandboxPublisherFinalChainReason::scheduler_control_detected:
    case MockSandboxPublisherFinalChainReason::release_gate_action_detected:
    case MockSandboxPublisherFinalChainReason::aggregate_review_required:
        return MockSandboxPublisherFinalChainStatus::review_required;
    }
    return MockSandboxPublisherFinalChainStatus::review_required;
}

}  // namespace

MockSandboxPublisherFinalChainRecord make_mock_sandbox_publisher_final_chain_record(
    const MockSandboxPublisherFinalChainRequest& request) {
    auto reason = final_chain_reason_for_aggregate(request.aggregate.status);
    if (request.final_chain_correlation_id.empty()) {
        reason = MockSandboxPublisherFinalChainReason::chain_correlation_id_missing;
    } else if (request.aggregate.correlation_id.empty()) {
        reason = MockSandboxPublisherFinalChainReason::aggregate_correlation_id_missing;
    } else if (request.aggregate.correlation_id != request.expected_aggregate_correlation_id) {
        reason = MockSandboxPublisherFinalChainReason::aggregate_correlation_mismatch;
    } else if (request.aggregate.external_publish_performed) {
        reason = MockSandboxPublisherFinalChainReason::external_publish_detected;
    } else if (request.aggregate.adapter_call_attempted) {
        reason = MockSandboxPublisherFinalChainReason::adapter_call_detected;
    } else if (request.aggregate.scheduler_control_used) {
        reason = MockSandboxPublisherFinalChainReason::scheduler_control_detected;
    } else if (request.aggregate.release_gate_action_executed) {
        reason = MockSandboxPublisherFinalChainReason::release_gate_action_detected;
    } else if (request.aggregate.status == MockSandboxPublisherStageAggregateStatus::complete) {
        if (!request.aggregate.publish_succeeded || request.aggregate.publish_failed) {
            reason = MockSandboxPublisherFinalChainReason::publish_flag_mismatch;
        } else if (request.aggregate.review_required) {
            reason = MockSandboxPublisherFinalChainReason::review_required_flag_mismatch;
        } else if (request.aggregate.payload_schema_name != request.expected_schema_name) {
            reason = MockSandboxPublisherFinalChainReason::schema_name_mismatch;
        } else if (request.aggregate.payload_schema_version != request.expected_schema_version) {
            reason = MockSandboxPublisherFinalChainReason::schema_version_mismatch;
        } else if (request.aggregate.release_target_reference_id != request.expected_release_target_reference_id) {
            reason = MockSandboxPublisherFinalChainReason::release_target_mismatch;
        } else if (request.aggregate.policy_reference_id != request.expected_policy_reference_id) {
            reason = MockSandboxPublisherFinalChainReason::policy_reference_mismatch;
        } else if (request.aggregate.evidence_record_id != request.expected_evidence_record_id) {
            reason = MockSandboxPublisherFinalChainReason::evidence_record_mismatch;
        } else if (request.aggregate.idempotency_key != request.expected_idempotency_key) {
            reason = MockSandboxPublisherFinalChainReason::idempotency_key_mismatch;
        } else if (request.aggregate.payload_content_hash.empty()) {
            reason = MockSandboxPublisherFinalChainReason::payload_hash_missing;
        } else if (request.aggregate.payload_content_hash != request.expected_payload_content_hash) {
            reason = MockSandboxPublisherFinalChainReason::payload_hash_mismatch;
        }
    }

    const auto status = final_chain_status_for_reason(reason);
    return MockSandboxPublisherFinalChainRecord{
        .status = status,
        .reason = reason,
        .final_chain_correlation_id = request.final_chain_correlation_id,
        .aggregate_correlation_id = request.aggregate.correlation_id,
        .aggregate_status = request.aggregate.status,
        .aggregate_reason = request.aggregate.reason,
        .adapter_status = request.aggregate.adapter_status,
        .adapter_reason = request.aggregate.adapter_reason,
        .stage_status = request.aggregate.stage_status,
        .stage_reason = request.aggregate.stage_reason,
        .action_kind = request.aggregate.action_kind,
        .payload_record_id = request.aggregate.payload_record_id,
        .payload_schema_name = request.aggregate.payload_schema_name,
        .payload_schema_version = request.aggregate.payload_schema_version,
        .release_target_reference_id = request.aggregate.release_target_reference_id,
        .policy_reference_id = request.aggregate.policy_reference_id,
        .evidence_record_id = request.aggregate.evidence_record_id,
        .sink_transaction_id = request.aggregate.sink_transaction_id,
        .idempotency_key = request.aggregate.idempotency_key,
        .payload_content_hash = request.aggregate.payload_content_hash,
        .dry_run = request.aggregate.dry_run,
        .commit = request.aggregate.commit,
        .publish_attempted = request.aggregate.publish_attempted,
        .publish_recorded = request.aggregate.publish_recorded,
        .publish_succeeded = request.aggregate.publish_succeeded,
        .publish_failed = request.aggregate.publish_failed,
        .duplicate_detected = request.aggregate.duplicate_detected,
        .review_required = request.aggregate.review_required || status == MockSandboxPublisherFinalChainStatus::review_required,
        .external_publish_performed = false,
        .adapter_call_attempted = false,
        .scheduler_control_used = false,
        .release_gate_action_executed = false,
    };
}

ControlledNonProductionPublisherSinkResponse submit_controlled_non_production_publisher_sink_request(
    const ControlledNonProductionPublisherSinkRequest& request) {
    auto reason = ControlledNonProductionPublisherSinkReason::request_accepted;
    if (!request.sink_boundary_available) {
        reason = ControlledNonProductionPublisherSinkReason::boundary_unavailable;
    } else if (request.sandbox_environment != "non_production") {
        reason = ControlledNonProductionPublisherSinkReason::non_production_environment_missing;
    } else if (request.dry_run && request.commit) {
        reason = ControlledNonProductionPublisherSinkReason::conflicting_publish_mode;
    } else if (!request.dry_run && !request.commit) {
        reason = ControlledNonProductionPublisherSinkReason::publish_mode_missing;
    } else if (request.sink_transaction_id.empty()) {
        reason = ControlledNonProductionPublisherSinkReason::sink_transaction_missing;
    } else if (request.idempotency_key.empty()) {
        reason = ControlledNonProductionPublisherSinkReason::idempotency_key_missing;
    } else if (request.payload_record_id.empty() || request.payload_reference_id.empty()) {
        reason = ControlledNonProductionPublisherSinkReason::payload_reference_missing;
    } else if (request.payload_schema_name.empty() || request.payload_schema_version.empty()) {
        reason = ControlledNonProductionPublisherSinkReason::payload_schema_missing;
    } else if (!request.payload_schema_valid) {
        reason = ControlledNonProductionPublisherSinkReason::payload_schema_invalid;
    } else if (request.payload_content_hash.empty()) {
        reason = ControlledNonProductionPublisherSinkReason::payload_hash_missing;
    } else if (!request.payload_hash_matches) {
        reason = ControlledNonProductionPublisherSinkReason::payload_hash_mismatch;
    } else if (!request.action_kind_supported) {
        reason = ControlledNonProductionPublisherSinkReason::unsupported_action_kind;
    } else if (request.release_target_reference_id.empty() || !request.release_target_known) {
        reason = ControlledNonProductionPublisherSinkReason::unknown_release_target;
    } else if (request.policy_reference_id.empty() || !request.policy_reference_known) {
        reason = ControlledNonProductionPublisherSinkReason::unknown_policy_reference;
    } else if (request.credential_reference_id.empty()) {
        reason = ControlledNonProductionPublisherSinkReason::credential_reference_missing;
    } else if (!request.credential_available) {
        reason = ControlledNonProductionPublisherSinkReason::credential_unavailable;
    } else if (!request.credential_valid) {
        reason = ControlledNonProductionPublisherSinkReason::credential_invalid;
    } else if (request.operator_approval_reference_id.empty() || !request.operator_approval_valid) {
        reason = ControlledNonProductionPublisherSinkReason::operator_approval_missing;
    } else if (request.release_gate_action_requested) {
        reason = ControlledNonProductionPublisherSinkReason::release_gate_action_requested;
    } else if (request.scheduler_control_requested) {
        reason = ControlledNonProductionPublisherSinkReason::scheduler_control_requested;
    } else if (request.rollback_requested || request.replay_requested || request.mass_audit_requested) {
        reason = ControlledNonProductionPublisherSinkReason::runtime_control_requested;
    } else if (request.duplicate_idempotency_key && !request.duplicate_payload_hash_matches) {
        reason = ControlledNonProductionPublisherSinkReason::duplicate_conflict;
    } else if (request.duplicate_idempotency_key) {
        reason = ControlledNonProductionPublisherSinkReason::duplicate_accepted;
    } else if (request.sink_times_out) {
        reason = ControlledNonProductionPublisherSinkReason::timeout;
    } else if (request.sink_transient_failure) {
        reason = ControlledNonProductionPublisherSinkReason::transient_sink_unavailable;
    } else if (request.sink_permanent_failure) {
        reason = ControlledNonProductionPublisherSinkReason::permanent_sink_failure;
    } else if (request.sink_requires_review) {
        reason = ControlledNonProductionPublisherSinkReason::review_required_by_sink;
    } else if (request.dry_run) {
        reason = ControlledNonProductionPublisherSinkReason::dry_run_recorded;
    }

    auto status = ControlledNonProductionPublisherSinkStatus::not_attempted;
    switch (reason) {
    case ControlledNonProductionPublisherSinkReason::dry_run_recorded:
        status = ControlledNonProductionPublisherSinkStatus::dry_run_recorded;
        break;
    case ControlledNonProductionPublisherSinkReason::request_accepted:
        status = ControlledNonProductionPublisherSinkStatus::accepted;
        break;
    case ControlledNonProductionPublisherSinkReason::duplicate_accepted:
        status = ControlledNonProductionPublisherSinkStatus::duplicate_idempotent;
        break;
    case ControlledNonProductionPublisherSinkReason::duplicate_conflict:
        status = ControlledNonProductionPublisherSinkStatus::duplicate_conflict;
        break;
    case ControlledNonProductionPublisherSinkReason::timeout:
        status = ControlledNonProductionPublisherSinkStatus::timeout;
        break;
    case ControlledNonProductionPublisherSinkReason::transient_sink_unavailable:
        status = ControlledNonProductionPublisherSinkStatus::transient_failure;
        break;
    case ControlledNonProductionPublisherSinkReason::permanent_sink_failure:
        status = ControlledNonProductionPublisherSinkStatus::permanent_failure;
        break;
    case ControlledNonProductionPublisherSinkReason::review_required_by_sink:
        status = ControlledNonProductionPublisherSinkStatus::review_required;
        break;
    case ControlledNonProductionPublisherSinkReason::boundary_unavailable:
        status = ControlledNonProductionPublisherSinkStatus::not_attempted;
        break;
    default:
        status = ControlledNonProductionPublisherSinkStatus::rejected;
        break;
    }

    const bool accepted = status == ControlledNonProductionPublisherSinkStatus::accepted
        || status == ControlledNonProductionPublisherSinkStatus::duplicate_idempotent;
    const bool failed = status == ControlledNonProductionPublisherSinkStatus::rejected
        || status == ControlledNonProductionPublisherSinkStatus::duplicate_conflict
        || status == ControlledNonProductionPublisherSinkStatus::transient_failure
        || status == ControlledNonProductionPublisherSinkStatus::permanent_failure
        || status == ControlledNonProductionPublisherSinkStatus::timeout;
    return ControlledNonProductionPublisherSinkResponse{
        .status = status,
        .reason = reason,
        .sink_transaction_id = request.sink_transaction_id,
        .idempotency_key = request.idempotency_key,
        .accepted_payload_record_id = accepted || status == ControlledNonProductionPublisherSinkStatus::dry_run_recorded
            ? request.payload_record_id
            : "",
        .accepted_payload_content_hash = accepted || status == ControlledNonProductionPublisherSinkStatus::dry_run_recorded
            ? request.payload_content_hash
            : "",
        .observed_action_kind = accepted || status == ControlledNonProductionPublisherSinkStatus::dry_run_recorded
            ? request.action_kind
            : MockReleaseGatePublishedActionKind::none,
        .observed_release_target_reference_id = accepted || status == ControlledNonProductionPublisherSinkStatus::dry_run_recorded
            ? request.release_target_reference_id
            : "",
        .observed_policy_reference_id = accepted || status == ControlledNonProductionPublisherSinkStatus::dry_run_recorded
            ? request.policy_reference_id
            : "",
        .audit_correlation_id = request.audit_correlation_id,
        .sink_diagnostic_reference_id = request.audit_correlation_id.empty()
            ? ""
            : request.audit_correlation_id + ":diagnostic",
        .sink_audit_record_reference_id = request.audit_correlation_id.empty()
            ? ""
            : request.audit_correlation_id + ":sink-audit",
        .dry_run_recorded = status == ControlledNonProductionPublisherSinkStatus::dry_run_recorded,
        .commit_attempted = request.commit && status != ControlledNonProductionPublisherSinkStatus::not_attempted,
        .commit_accepted = status == ControlledNonProductionPublisherSinkStatus::accepted
            || status == ControlledNonProductionPublisherSinkStatus::duplicate_idempotent,
        .commit_rejected = failed,
        .duplicate_detected = request.duplicate_idempotency_key,
        .duplicate_accepted = status == ControlledNonProductionPublisherSinkStatus::duplicate_idempotent,
        .operator_review_required = status == ControlledNonProductionPublisherSinkStatus::review_required
            || status == ControlledNonProductionPublisherSinkStatus::duplicate_conflict
            || status == ControlledNonProductionPublisherSinkStatus::permanent_failure,
        .retry_recommended = status == ControlledNonProductionPublisherSinkStatus::transient_failure
            || status == ControlledNonProductionPublisherSinkStatus::timeout,
        .external_publish_performed = false,
        .release_gate_action_executed = false,
        .scheduler_control_used = false,
        .runtime_control_executed = false,
    };
}

}  // namespace scau::validation::release_gate

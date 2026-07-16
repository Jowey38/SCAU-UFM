#pragma once

#include <string>

#include "coupling/core/state.hpp"

namespace scau::validation::release_gate {

using scau::coupling::core::FaultControllerSchedulerReleaseGateActionCompletionRecord;
using scau::coupling::core::FaultControllerSchedulerExecutableControlKind;
using scau::coupling::core::FaultControllerSchedulerPhaseDescriptor;

enum class MockReleaseGatePublisherStatus {
    not_attempted,
    blocked,
    dry_run_recorded,
    published,
    failed,
    review_required,
};

enum class MockReleaseGatePublisherReason {
    completion_not_complete,
    completion_requires_review,
    action_failed,
    publisher_boundary_absent,
    operator_approval_missing,
    project_policy_missing,
    release_target_missing,
    idempotency_key_missing,
    action_transaction_missing,
    payload_reference_missing,
    payload_hash_missing,
    unsupported_action_combination,
    publish_mode_missing,
    conflicting_publish_mode,
    dry_run_recorded,
    pass_no_action,
    published_review_required,
    published_block_merge,
    published_block_release,
    published_abort,
    publish_failed,
};

enum class MockReleaseGatePublishedActionKind {
    none,
    review_required,
    block_merge,
    block_release,
    abort,
};

struct MockReleaseGatePublisherRequest {
    FaultControllerSchedulerReleaseGateActionCompletionRecord completion{};
    bool publisher_boundary_available{false};
    bool operator_approved{false};
    std::string project_policy_reference_id{};
    std::string release_target_reference_id{};
    std::string idempotency_key{};
    std::string action_transaction_id{};
    std::string payload_reference_id{};
    std::string payload_content_hash{};
    bool publish_dry_run{false};
    bool publish_commit{false};
    bool publisher_sink_succeeded{false};
};

struct MockReleaseGatePublisherResult {
    MockReleaseGatePublisherStatus status{MockReleaseGatePublisherStatus::not_attempted};
    MockReleaseGatePublisherReason reason{MockReleaseGatePublisherReason::completion_not_complete};
    MockReleaseGatePublishedActionKind action_kind{MockReleaseGatePublishedActionKind::none};
    FaultControllerSchedulerExecutableControlKind requested_control_kind{FaultControllerSchedulerExecutableControlKind::none};
    FaultControllerSchedulerPhaseDescriptor target_phase{};
    std::size_t mutation_generation_before{0U};
    std::size_t mutation_generation_after{0U};
    std::string evidence_record_id{};
    std::string policy_reference_id{};
    std::string action_reference_id{};
    std::string action_transaction_id{};
    std::string release_target_reference_id{};
    std::string idempotency_key{};
    std::string payload_reference_id{};
    std::string payload_content_hash{};
    bool review_required{false};
    bool merge_blocked{false};
    bool release_blocked{false};
    bool abort_required{false};
    bool publish_attempted{false};
    bool publish_dry_run_recorded{false};
    bool publish_succeeded{false};
    bool publish_failed{false};
    bool external_publish_performed{false};
    bool adapter_call_attempted{false};
};

enum class MockReleaseGatePublisherPayloadStatus {
    not_prepared,
    blocked,
    prepared,
    review_required,
};

enum class MockReleaseGatePublisherPayloadReason {
    completion_not_complete,
    completion_requires_review,
    schema_name_missing,
    schema_version_missing,
    payload_record_missing,
    release_target_missing,
    project_policy_missing,
    action_reference_missing,
    action_transaction_missing,
    idempotency_key_missing,
    payload_hash_missing,
    payload_reference_unavailable,
    action_kind_mismatch,
    unsupported_action_combination,
    payload_prepared,
    pass_no_action_payload_prepared,
    review_required,
};

struct MockReleaseGatePublisherPayloadRequest {
    FaultControllerSchedulerReleaseGateActionCompletionRecord completion{};
    MockReleaseGatePublishedActionKind action_kind{MockReleaseGatePublishedActionKind::none};
    std::string schema_name{};
    std::string schema_version{};
    std::string payload_record_id{};
    std::string release_target_reference_id{};
    std::string project_policy_reference_id{};
    std::string action_reference_id{};
    std::string action_transaction_id{};
    std::string idempotency_key{};
    std::string payload_content_hash{};
    bool payload_reference_available{false};
};

struct MockReleaseGatePublisherPayloadRecord {
    MockReleaseGatePublisherPayloadStatus status{MockReleaseGatePublisherPayloadStatus::not_prepared};
    MockReleaseGatePublisherPayloadReason reason{MockReleaseGatePublisherPayloadReason::completion_not_complete};
    MockReleaseGatePublishedActionKind action_kind{MockReleaseGatePublishedActionKind::none};
    std::string schema_name{};
    std::string schema_version{};
    std::string payload_record_id{};
    FaultControllerSchedulerExecutableControlKind requested_control_kind{FaultControllerSchedulerExecutableControlKind::none};
    FaultControllerSchedulerPhaseDescriptor target_phase{};
    std::size_t mutation_generation_before{0U};
    std::size_t mutation_generation_after{0U};
    std::string evidence_record_id{};
    std::string policy_reference_id{};
    std::string action_reference_id{};
    std::string action_transaction_id{};
    std::string release_target_reference_id{};
    std::string idempotency_key{};
    std::string payload_content_hash{};
    bool review_required{false};
    bool merge_blocked{false};
    bool release_blocked{false};
    bool abort_required{false};
    bool payload_prepared{false};
    bool external_publish_performed{false};
    bool adapter_call_attempted{false};
};

enum class MockReleaseGatePayloadPublishReason {
    payload_not_prepared,
    payload_requires_review,
    payload_record_missing,
    release_target_missing,
    idempotency_key_missing,
    payload_hash_missing,
    unsupported_action_combination,
    publish_mode_missing,
    conflicting_publish_mode,
    dry_run_recorded,
    pass_no_action,
    published_review_required,
    published_block_merge,
    published_block_release,
    published_abort,
    publish_failed,
};

struct MockReleaseGatePayloadPublishRequest {
    MockReleaseGatePublisherPayloadRecord payload{};
    bool publish_dry_run{false};
    bool publish_commit{false};
    bool publisher_sink_succeeded{false};
};

enum class MockReleaseGatePublisherChainAggregateStatus {
    incomplete,
    dry_run_recorded,
    complete,
    failed,
    review_required,
};

enum class MockReleaseGatePublisherChainAggregateReason {
    payload_not_prepared,
    payload_requires_review,
    publish_not_completed,
    publish_dry_run_recorded,
    publish_failed,
    payload_record_mismatch,
    schema_name_mismatch,
    schema_version_mismatch,
    release_target_mismatch,
    idempotency_key_mismatch,
    payload_hash_missing,
    action_kind_mismatch,
    mutation_generation_mismatch,
    action_flag_mismatch,
    external_publish_detected,
    adapter_call_detected,
    aggregate_complete,
    aggregate_review_required,
};

struct MockReleaseGatePublisherChainAggregateRequest {
    MockReleaseGatePublisherPayloadRecord payload{};
    MockReleaseGatePublisherResult publish_result{};
    std::string correlation_id{};
    std::string expected_schema_name{};
    std::string expected_schema_version{};
    std::string expected_release_target_reference_id{};
    std::string expected_idempotency_key{};
};

struct MockReleaseGatePublisherChainAggregateRecord {
    MockReleaseGatePublisherChainAggregateStatus status{MockReleaseGatePublisherChainAggregateStatus::incomplete};
    MockReleaseGatePublisherChainAggregateReason reason{MockReleaseGatePublisherChainAggregateReason::payload_not_prepared};
    std::string correlation_id{};
    MockReleaseGatePublisherPayloadStatus payload_status{MockReleaseGatePublisherPayloadStatus::not_prepared};
    MockReleaseGatePublisherPayloadReason payload_reason{MockReleaseGatePublisherPayloadReason::completion_not_complete};
    MockReleaseGatePublisherStatus publisher_status{MockReleaseGatePublisherStatus::not_attempted};
    MockReleaseGatePublisherReason publisher_reason{MockReleaseGatePublisherReason::completion_not_complete};
    MockReleaseGatePublishedActionKind action_kind{MockReleaseGatePublishedActionKind::none};
    std::string schema_name{};
    std::string schema_version{};
    std::string payload_record_id{};
    std::string evidence_record_id{};
    std::string policy_reference_id{};
    std::string action_reference_id{};
    std::string release_target_reference_id{};
    std::string idempotency_key{};
    std::string payload_content_hash{};
    FaultControllerSchedulerExecutableControlKind requested_control_kind{FaultControllerSchedulerExecutableControlKind::none};
    FaultControllerSchedulerPhaseDescriptor target_phase{};
    std::size_t mutation_generation_before{0U};
    std::size_t mutation_generation_after{0U};
    bool review_required{false};
    bool merge_blocked{false};
    bool release_blocked{false};
    bool abort_required{false};
    bool payload_prepared{false};
    bool publish_attempted{false};
    bool publish_succeeded{false};
    bool publish_failed{false};
    bool external_publish_performed{false};
    bool adapter_call_attempted{false};
};

enum class MockSandboxSinkStatus {
    not_attempted,
    dry_run_recorded,
    committed,
    rejected,
    duplicate_idempotent,
    duplicate_conflict,
    transient_failure,
    permanent_failure,
    timeout,
    review_required,
};

enum class MockSandboxSinkReason {
    payload_accepted,
    dry_run_only,
    missing_credential,
    invalid_credential,
    payload_schema_invalid,
    payload_hash_mismatch,
    unsupported_action_kind,
    release_target_unknown,
    policy_reference_unknown,
    duplicate_idempotent,
    duplicate_conflict,
    sink_unavailable,
    sink_timeout,
    sink_internal_error,
    operator_review_required,
};

struct MockSandboxSinkRequest {
    std::string sink_transaction_id{};
    std::string idempotency_key{};
    bool dry_run{false};
    bool commit{false};
    std::string payload_record_id{};
    std::string payload_schema_name{};
    std::string payload_schema_version{};
    MockReleaseGatePublishedActionKind action_kind{MockReleaseGatePublishedActionKind::none};
    std::string release_target_reference_id{};
    std::string policy_reference_id{};
    std::string evidence_record_id{};
    std::string payload_content_hash{};
    std::string payload_body_reference{};
    std::string credential_reference_id{};
    std::string operator_approval_reference_id{};
    bool credential_available{false};
    bool credential_valid{false};
    bool payload_schema_valid{false};
    bool payload_hash_matches{false};
    bool action_kind_supported{false};
    bool release_target_known{false};
    bool policy_reference_known{false};
    bool duplicate_idempotency_key{false};
    bool duplicate_payload_hash_matches{false};
    bool sink_available{false};
    bool sink_times_out{false};
    bool sink_internal_error{false};
};

struct MockSandboxSinkResponse {
    MockSandboxSinkStatus status{MockSandboxSinkStatus::not_attempted};
    MockSandboxSinkReason reason{MockSandboxSinkReason::payload_accepted};
    std::string sink_transaction_id{};
    std::string idempotency_key{};
    std::string accepted_payload_record_id{};
    std::string accepted_payload_content_hash{};
    MockReleaseGatePublishedActionKind observed_action_kind{MockReleaseGatePublishedActionKind::none};
    bool dry_run_recorded{false};
    bool commit_attempted{false};
    bool commit_succeeded{false};
    bool commit_failed{false};
    bool duplicate_detected{false};
    bool operator_review_required{false};
    std::string sink_diagnostic_reference_id{};
};

enum class MockSandboxPublisherAdapterStatus {
    not_attempted,
    blocked,
    dry_run_recorded,
    published,
    failed,
    review_required,
};

enum class MockSandboxPublisherAdapterReason {
    payload_not_prepared,
    sink_transaction_missing,
    credential_reference_missing,
    operator_approval_missing,
    payload_record_missing,
    schema_missing,
    payload_hash_missing,
    release_target_missing,
    policy_reference_missing,
    idempotency_key_missing,
    publish_mode_missing,
    conflicting_publish_mode,
    sink_transaction_mismatch,
    sink_idempotency_mismatch,
    sink_payload_mismatch,
    sink_hash_mismatch,
    sink_action_mismatch,
    sink_rejected,
    sink_duplicate_conflict,
    sink_transient_failure,
    sink_permanent_failure,
    sink_timeout,
    sink_review_required,
    dry_run_recorded,
    published,
    duplicate_idempotent,
};

struct MockSandboxPublisherAdapterRequest {
    MockReleaseGatePublisherPayloadRecord payload{};
    bool dry_run{false};
    bool commit{false};
    std::string sink_transaction_id{};
    std::string credential_reference_id{};
    std::string operator_approval_reference_id{};
    bool credential_available{false};
    bool credential_valid{false};
    bool payload_schema_valid{false};
    bool payload_hash_matches{false};
    bool action_kind_supported{false};
    bool release_target_known{false};
    bool policy_reference_known{false};
    bool duplicate_idempotency_key{false};
    bool duplicate_payload_hash_matches{false};
    bool sink_available{false};
    bool sink_times_out{false};
    bool sink_internal_error{false};
};

struct MockSandboxPublisherAdapterResult {
    MockSandboxPublisherAdapterStatus status{MockSandboxPublisherAdapterStatus::not_attempted};
    MockSandboxPublisherAdapterReason reason{MockSandboxPublisherAdapterReason::payload_not_prepared};
    MockReleaseGatePublisherPayloadRecord payload{};
    MockSandboxSinkRequest sink_request{};
    MockSandboxSinkResponse sink_response{};
    MockReleaseGatePublishedActionKind action_kind{MockReleaseGatePublishedActionKind::none};
    std::string payload_record_id{};
    std::string sink_transaction_id{};
    std::string idempotency_key{};
    std::string payload_content_hash{};
    bool dry_run_recorded{false};
    bool commit_attempted{false};
    bool commit_succeeded{false};
    bool commit_failed{false};
    bool duplicate_detected{false};
    bool review_required{false};
    bool external_publish_performed{false};
    bool adapter_call_attempted{false};
};

enum class MockSandboxPublisherStageRecordStatus {
    blocked,
    recorded,
    published,
    failed,
    review_required,
};

enum class MockSandboxPublisherStageRecordReason {
    not_attempted_default,
    pre_submit_blocked,
    dry_run_recorded,
    commit_published,
    duplicate_idempotent_published,
    sink_failed,
    duplicate_conflict_failed,
    metadata_mismatch_review,
    sink_review_required,
};

struct MockSandboxPublisherStageRecord {
    MockSandboxPublisherStageRecordStatus status{MockSandboxPublisherStageRecordStatus::blocked};
    MockSandboxPublisherStageRecordReason reason{MockSandboxPublisherStageRecordReason::pre_submit_blocked};
    MockSandboxPublisherAdapterStatus adapter_status{MockSandboxPublisherAdapterStatus::not_attempted};
    MockSandboxPublisherAdapterReason adapter_reason{MockSandboxPublisherAdapterReason::payload_not_prepared};
    MockReleaseGatePublishedActionKind action_kind{MockReleaseGatePublishedActionKind::none};
    std::string payload_record_id{};
    std::string payload_schema_name{};
    std::string payload_schema_version{};
    std::string release_target_reference_id{};
    std::string policy_reference_id{};
    std::string evidence_record_id{};
    std::string sink_transaction_id{};
    std::string idempotency_key{};
    std::string payload_content_hash{};
    bool dry_run{false};
    bool commit{false};
    bool publish_attempted{false};
    bool publish_recorded{false};
    bool publish_succeeded{false};
    bool publish_failed{false};
    bool duplicate_detected{false};
    bool review_required{false};
    bool external_publish_performed{false};
    bool adapter_call_attempted{false};
    bool scheduler_control_used{false};
    bool release_gate_action_executed{false};
};

enum class MockSandboxPublisherStageAggregateStatus {
    incomplete,
    dry_run_recorded,
    complete,
    failed,
    review_required,
};

enum class MockSandboxPublisherStageAggregateReason {
    correlation_id_missing,
    adapter_stage_status_mismatch,
    adapter_stage_reason_mismatch,
    action_kind_mismatch,
    payload_record_mismatch,
    schema_name_mismatch,
    schema_version_mismatch,
    release_target_mismatch,
    policy_reference_mismatch,
    evidence_record_mismatch,
    sink_transaction_mismatch,
    idempotency_key_mismatch,
    payload_hash_missing,
    duplicate_flag_mismatch,
    review_required_flag_mismatch,
    publish_flag_mismatch,
    external_publish_detected,
    adapter_call_detected,
    scheduler_control_detected,
    release_gate_action_detected,
    stage_blocked,
    stage_dry_run_recorded,
    stage_failed,
    stage_review_required,
    aggregate_complete,
};

struct MockSandboxPublisherStageAggregateRequest {
    MockSandboxPublisherAdapterResult adapter{};
    MockSandboxPublisherStageRecord stage{};
    std::string correlation_id{};
    std::string expected_schema_name{};
    std::string expected_schema_version{};
    std::string expected_release_target_reference_id{};
    std::string expected_policy_reference_id{};
    std::string expected_evidence_record_id{};
    std::string expected_idempotency_key{};
};

struct MockSandboxPublisherStageAggregateRecord {
    MockSandboxPublisherStageAggregateStatus status{MockSandboxPublisherStageAggregateStatus::incomplete};
    MockSandboxPublisherStageAggregateReason reason{MockSandboxPublisherStageAggregateReason::correlation_id_missing};
    std::string correlation_id{};
    MockSandboxPublisherAdapterStatus adapter_status{MockSandboxPublisherAdapterStatus::not_attempted};
    MockSandboxPublisherAdapterReason adapter_reason{MockSandboxPublisherAdapterReason::payload_not_prepared};
    MockSandboxPublisherStageRecordStatus stage_status{MockSandboxPublisherStageRecordStatus::blocked};
    MockSandboxPublisherStageRecordReason stage_reason{MockSandboxPublisherStageRecordReason::pre_submit_blocked};
    MockReleaseGatePublishedActionKind action_kind{MockReleaseGatePublishedActionKind::none};
    std::string payload_record_id{};
    std::string payload_schema_name{};
    std::string payload_schema_version{};
    std::string release_target_reference_id{};
    std::string policy_reference_id{};
    std::string evidence_record_id{};
    std::string sink_transaction_id{};
    std::string idempotency_key{};
    std::string payload_content_hash{};
    bool dry_run{false};
    bool commit{false};
    bool publish_attempted{false};
    bool publish_recorded{false};
    bool publish_succeeded{false};
    bool publish_failed{false};
    bool duplicate_detected{false};
    bool review_required{false};
    bool external_publish_performed{false};
    bool adapter_call_attempted{false};
    bool scheduler_control_used{false};
    bool release_gate_action_executed{false};
};

enum class MockSandboxPublisherFinalChainStatus {
    incomplete,
    dry_run_recorded,
    complete,
    failed,
    review_required,
};

enum class MockSandboxPublisherFinalChainReason {
    chain_correlation_id_missing,
    aggregate_correlation_id_missing,
    aggregate_correlation_mismatch,
    schema_name_mismatch,
    schema_version_mismatch,
    release_target_mismatch,
    policy_reference_mismatch,
    evidence_record_mismatch,
    idempotency_key_mismatch,
    payload_hash_missing,
    payload_hash_mismatch,
    publish_flag_mismatch,
    review_required_flag_mismatch,
    external_publish_detected,
    adapter_call_detected,
    scheduler_control_detected,
    release_gate_action_detected,
    aggregate_incomplete,
    aggregate_dry_run_recorded,
    aggregate_failed,
    aggregate_review_required,
    final_chain_complete,
};

struct MockSandboxPublisherFinalChainRequest {
    MockSandboxPublisherStageAggregateRecord aggregate{};
    std::string final_chain_correlation_id{};
    std::string expected_aggregate_correlation_id{};
    std::string expected_schema_name{};
    std::string expected_schema_version{};
    std::string expected_release_target_reference_id{};
    std::string expected_policy_reference_id{};
    std::string expected_evidence_record_id{};
    std::string expected_idempotency_key{};
    std::string expected_payload_content_hash{};
};

struct MockSandboxPublisherFinalChainRecord {
    MockSandboxPublisherFinalChainStatus status{MockSandboxPublisherFinalChainStatus::incomplete};
    MockSandboxPublisherFinalChainReason reason{MockSandboxPublisherFinalChainReason::chain_correlation_id_missing};
    std::string final_chain_correlation_id{};
    std::string aggregate_correlation_id{};
    MockSandboxPublisherStageAggregateStatus aggregate_status{MockSandboxPublisherStageAggregateStatus::incomplete};
    MockSandboxPublisherStageAggregateReason aggregate_reason{MockSandboxPublisherStageAggregateReason::correlation_id_missing};
    MockSandboxPublisherAdapterStatus adapter_status{MockSandboxPublisherAdapterStatus::not_attempted};
    MockSandboxPublisherAdapterReason adapter_reason{MockSandboxPublisherAdapterReason::payload_not_prepared};
    MockSandboxPublisherStageRecordStatus stage_status{MockSandboxPublisherStageRecordStatus::blocked};
    MockSandboxPublisherStageRecordReason stage_reason{MockSandboxPublisherStageRecordReason::pre_submit_blocked};
    MockReleaseGatePublishedActionKind action_kind{MockReleaseGatePublishedActionKind::none};
    std::string payload_record_id{};
    std::string payload_schema_name{};
    std::string payload_schema_version{};
    std::string release_target_reference_id{};
    std::string policy_reference_id{};
    std::string evidence_record_id{};
    std::string sink_transaction_id{};
    std::string idempotency_key{};
    std::string payload_content_hash{};
    bool dry_run{false};
    bool commit{false};
    bool publish_attempted{false};
    bool publish_recorded{false};
    bool publish_succeeded{false};
    bool publish_failed{false};
    bool duplicate_detected{false};
    bool review_required{false};
    bool external_publish_performed{false};
    bool adapter_call_attempted{false};
    bool scheduler_control_used{false};
    bool release_gate_action_executed{false};
};

enum class ControlledNonProductionPublisherSinkStatus {
    not_attempted,
    dry_run_recorded,
    accepted,
    rejected,
    duplicate_idempotent,
    duplicate_conflict,
    transient_failure,
    permanent_failure,
    timeout,
    review_required,
};

enum class ControlledNonProductionPublisherSinkReason {
    boundary_unavailable,
    non_production_environment_missing,
    unsupported_action_kind,
    unknown_release_target,
    unknown_policy_reference,
    credential_reference_missing,
    credential_unavailable,
    credential_invalid,
    operator_approval_missing,
    payload_reference_missing,
    payload_schema_missing,
    payload_schema_invalid,
    payload_hash_missing,
    payload_hash_mismatch,
    idempotency_key_missing,
    sink_transaction_missing,
    conflicting_publish_mode,
    publish_mode_missing,
    release_gate_action_requested,
    scheduler_control_requested,
    runtime_control_requested,
    duplicate_accepted,
    duplicate_conflict,
    timeout,
    transient_sink_unavailable,
    permanent_sink_failure,
    review_required_by_sink,
    dry_run_recorded,
    request_accepted,
};

struct ControlledNonProductionPublisherSinkRequest {
    std::string sink_transaction_id{};
    std::string sandbox_environment{};
    std::string idempotency_key{};
    std::string payload_record_id{};
    std::string payload_schema_name{};
    std::string payload_schema_version{};
    std::string payload_reference_id{};
    std::string payload_content_hash{};
    MockReleaseGatePublishedActionKind action_kind{MockReleaseGatePublishedActionKind::none};
    std::string release_target_reference_id{};
    std::string policy_reference_id{};
    std::string evidence_record_id{};
    std::string credential_reference_id{};
    std::string operator_approval_reference_id{};
    std::string audit_correlation_id{};
    bool dry_run{false};
    bool commit{false};
    bool sink_boundary_available{false};
    bool release_target_known{false};
    bool policy_reference_known{false};
    bool credential_available{false};
    bool credential_valid{false};
    bool operator_approval_valid{false};
    bool payload_schema_valid{false};
    bool payload_hash_matches{false};
    bool action_kind_supported{false};
    bool duplicate_idempotency_key{false};
    bool duplicate_payload_hash_matches{false};
    bool sink_times_out{false};
    bool sink_transient_failure{false};
    bool sink_permanent_failure{false};
    bool sink_requires_review{false};
    bool release_gate_action_requested{false};
    bool scheduler_control_requested{false};
    bool rollback_requested{false};
    bool replay_requested{false};
    bool mass_audit_requested{false};
};

struct ControlledNonProductionPublisherSinkResponse {
    ControlledNonProductionPublisherSinkStatus status{ControlledNonProductionPublisherSinkStatus::not_attempted};
    ControlledNonProductionPublisherSinkReason reason{ControlledNonProductionPublisherSinkReason::boundary_unavailable};
    std::string sink_transaction_id{};
    std::string idempotency_key{};
    std::string accepted_payload_record_id{};
    std::string accepted_payload_content_hash{};
    MockReleaseGatePublishedActionKind observed_action_kind{MockReleaseGatePublishedActionKind::none};
    std::string observed_release_target_reference_id{};
    std::string observed_policy_reference_id{};
    std::string audit_correlation_id{};
    std::string sink_diagnostic_reference_id{};
    std::string sink_audit_record_reference_id{};
    bool dry_run_recorded{false};
    bool commit_attempted{false};
    bool commit_accepted{false};
    bool commit_rejected{false};
    bool duplicate_detected{false};
    bool duplicate_accepted{false};
    bool operator_review_required{false};
    bool retry_recommended{false};
    bool external_publish_performed{false};
    bool release_gate_action_executed{false};
    bool scheduler_control_used{false};
    bool runtime_control_executed{false};
};

[[nodiscard]] MockReleaseGatePublisherResult publish_mock_release_gate_action(
    const MockReleaseGatePublisherRequest& request);
[[nodiscard]] MockReleaseGatePublisherPayloadRecord prepare_mock_release_gate_publisher_payload(
    const MockReleaseGatePublisherPayloadRequest& request);
[[nodiscard]] MockReleaseGatePublisherResult publish_mock_release_gate_payload(
    const MockReleaseGatePayloadPublishRequest& request);
[[nodiscard]] MockReleaseGatePublisherChainAggregateRecord make_mock_release_gate_publisher_chain_aggregate(
    const MockReleaseGatePublisherChainAggregateRequest& request);
[[nodiscard]] MockSandboxSinkResponse submit_mock_sandbox_sink_request(
    const MockSandboxSinkRequest& request);
[[nodiscard]] MockSandboxPublisherAdapterResult publish_mock_sandbox_payload(
    const MockSandboxPublisherAdapterRequest& request);
[[nodiscard]] MockSandboxPublisherStageRecord consume_mock_sandbox_publisher_adapter_result(
    const MockSandboxPublisherAdapterResult& result);
[[nodiscard]] MockSandboxPublisherStageAggregateRecord make_mock_sandbox_publisher_stage_aggregate(
    const MockSandboxPublisherStageAggregateRequest& request);
[[nodiscard]] MockSandboxPublisherFinalChainRecord make_mock_sandbox_publisher_final_chain_record(
    const MockSandboxPublisherFinalChainRequest& request);
[[nodiscard]] ControlledNonProductionPublisherSinkResponse submit_controlled_non_production_publisher_sink_request(
    const ControlledNonProductionPublisherSinkRequest& request);

}  // namespace scau::validation::release_gate

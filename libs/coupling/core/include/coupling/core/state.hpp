#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace scau::coupling::core {

struct MassDeficitAccount {
    double volume{0.0};
};

enum class SharedExchangeEngine {
    drainage,
    river,
};

struct SharedExchangeEndpoint {
    SharedExchangeEngine engine{SharedExchangeEngine::drainage};
    std::size_t node_id{0U};
};

struct SharedExchangeEndpointDeficit {
    SharedExchangeEndpoint endpoint{};
    MassDeficitAccount mass_deficit_account{};
};

struct ExchangeCellState {
    double volume{0.0};
    MassDeficitAccount mass_deficit_account{};
    double phi_t{0.0};
    double h{0.0};
    double area{0.0};
    std::vector<SharedExchangeEndpointDeficit> shared_deficit_accounts{};
};

struct FlowLimit {
    double v_limit{0.0};
    double q_limit{0.0};
};

[[nodiscard]] FlowLimit compute_flow_limit(const ExchangeCellState& cell, double dt_sub);

struct SharedExchangeIntent {
    SharedExchangeEndpoint endpoint{};
    double q_request{0.0};
    double priority_weight{1.0};
};

struct ExchangeRequest {
    double q_request{0.0};
    double dt_sub{0.0};
};

struct ExchangeDecision {
    double q_granted{0.0};
    double v_granted{0.0};
    double q_repay{0.0};
    double v_repay{0.0};
    double v_unmet{0.0};
};

struct SharedExchangeDecision {
    SharedExchangeEndpoint endpoint{};
    ExchangeDecision exchange{};
    FlowLimit allocated_limit{};
    double priority_weight{1.0};
};

struct MockCouplingSchedulerStepResult {
    std::vector<SharedExchangeDecision> decisions{};
};

struct EngineReport {
    bool healthy{true};
    std::string engine_id{};
    std::string error_code{};
    double elapsed_time{0.0};
};

enum class EngineHealthStatus {
    healthy,
    unhealthy,
};

struct EngineHealthDiagnostic {
    EngineHealthStatus status{EngineHealthStatus::healthy};
    std::string engine_id{};
    std::string error_code{};
    double elapsed_time{0.0};
};

enum class EngineHealthAggregateStatus {
    all_healthy,
    any_unhealthy,
};

struct EngineHealthAggregate {
    EngineHealthAggregateStatus status{EngineHealthAggregateStatus::all_healthy};
    std::size_t report_count{0U};
    std::size_t unhealthy_count{0U};
};

enum class FaultControllerDiagnosticStatus {
    nominal,
    fault_detected,
};

struct FaultControllerDiagnostic {
    FaultControllerDiagnosticStatus status{FaultControllerDiagnosticStatus::nominal};
    EngineHealthAggregate health{};
    bool should_isolate{false};
    bool should_reconnect{false};
    bool should_abort{false};
};

enum class FaultControllerProposedActionState {
    continue_run,
    review_required,
};

struct FaultControllerProposedAction {
    FaultControllerDiagnostic diagnostic{};
    FaultControllerProposedActionState state{FaultControllerProposedActionState::continue_run};
    bool execute_isolation{false};
    bool execute_reconnect{false};
    bool execute_abort{false};
};

struct MockCouplingSchedulerFaultObservation {
    FaultControllerProposedAction action{};
    FaultControllerProposedActionState observed_state{FaultControllerProposedActionState::continue_run};
    bool observed_review_required{false};
    bool executed_isolation{false};
    bool executed_reconnect{false};
    bool executed_abort{false};
};

struct MockCouplingSchedulerFaultConsumption {
    MockCouplingSchedulerFaultObservation observation{};
    FaultControllerProposedActionState consumed_state{FaultControllerProposedActionState::continue_run};
    bool review_required_consumed{false};
    bool exchange_scheduling_allowed{true};
    bool replay_allowed{true};
    bool audit_allowed{true};
    bool executed_isolation{false};
    bool executed_reconnect{false};
    bool executed_abort{false};
};

enum class FaultControllerPassiveStateLabel {
    running,
    review_required,
};

struct FaultControllerPassiveStateClassification {
    MockCouplingSchedulerFaultConsumption consumption{};
    FaultControllerPassiveStateLabel state{FaultControllerPassiveStateLabel::running};
    bool scheduler_control_enabled{false};
    bool isolation_enabled{false};
    bool reconnect_enabled{false};
    bool abort_enabled{false};
};

enum class FaultControllerPassiveTransitionReason {
    nominal_health,
    fault_detected_review_required,
};

struct FaultControllerPassiveTransition {
    FaultControllerPassiveStateClassification classification{};
    FaultControllerPassiveStateLabel previous_state{FaultControllerPassiveStateLabel::running};
    FaultControllerPassiveStateLabel next_state{FaultControllerPassiveStateLabel::running};
    FaultControllerPassiveTransitionReason reason{FaultControllerPassiveTransitionReason::nominal_health};
    bool isolation_requested{false};
    bool reconnect_requested{false};
    bool abort_requested{false};
    bool runtime_action_executed{false};
    bool scheduler_control_enabled{false};
};

enum class FaultControllerPassiveActionAuditKind {
    none,
    operator_review,
};

enum class FaultControllerPassiveActionAuditStage {
    transition_recorded,
};

enum class FaultControllerPassiveActionAuditReason {
    nominal_no_action,
    review_required_only,
};

struct FaultControllerPassiveActionAuditRecord {
    FaultControllerPassiveTransition transition{};
    FaultControllerPassiveActionAuditKind action_kind{FaultControllerPassiveActionAuditKind::none};
    FaultControllerPassiveActionAuditStage action_stage{FaultControllerPassiveActionAuditStage::transition_recorded};
    FaultControllerPassiveActionAuditReason reason{FaultControllerPassiveActionAuditReason::nominal_no_action};
    bool scheduler_control_enabled{false};
    bool runtime_action_requested{false};
    bool runtime_action_executed{false};
    bool isolation_executed{false};
    bool reconnect_executed{false};
    bool abort_executed{false};
    bool adapter_call_executed{false};
    bool release_gate_action_executed{false};
};

enum class FaultControllerPassiveActionOutcomeKind {
    not_requested,
    operator_review_required,
};

enum class FaultControllerPassiveActionOutcomeReason {
    nominal_no_action,
    review_required_only,
};

struct FaultControllerPassiveActionOutcome {
    FaultControllerPassiveActionAuditRecord audit{};
    FaultControllerPassiveActionOutcomeKind outcome_kind{FaultControllerPassiveActionOutcomeKind::not_requested};
    FaultControllerPassiveActionOutcomeReason reason{FaultControllerPassiveActionOutcomeReason::nominal_no_action};
    bool scheduler_control_available{false};
    bool scheduler_control_used{false};
    bool adapter_boundary_available{false};
    bool adapter_call_attempted{false};
    bool adapter_call_succeeded{false};
    bool adapter_call_failed{false};
    bool runtime_action_requested{false};
    bool runtime_action_attempted{false};
    bool runtime_action_executed{false};
    bool runtime_action_skipped{false};
    bool runtime_action_failed{false};
    bool operator_review_required{false};
    bool rollback_required{false};
    bool replay_required{false};
    bool mass_audit_required{false};
    bool release_gate_action_executed{false};
};

enum class FaultControllerBlockedActionReason {
    no_action_requested,
    operator_review_required,
};

struct FaultControllerBlockedAction {
    FaultControllerPassiveActionOutcome outcome{};
    FaultControllerBlockedActionReason reason{FaultControllerBlockedActionReason::no_action_requested};
    bool execution_allowed{false};
    bool scheduler_control_allowed{false};
    bool adapter_call_allowed{false};
    bool isolation_allowed{false};
    bool reconnect_allowed{false};
    bool abort_allowed{false};
    bool release_gate_action_allowed{false};
};

enum class FaultControllerSchedulerControlKind {
    none,
    observe_only,
};

enum class FaultControllerSchedulerControlStatus {
    not_requested,
    blocked_boundary_absent,
};

enum class FaultControllerSchedulerControlReason {
    no_scheduler_control_requested,
    scheduler_control_boundary_absent,
};

struct FaultControllerSchedulerControlRequest {
    FaultControllerPassiveActionOutcome outcome{};
    FaultControllerBlockedAction blocked_action{};
    FaultControllerSchedulerControlKind requested_kind{FaultControllerSchedulerControlKind::none};
    FaultControllerSchedulerControlStatus status{FaultControllerSchedulerControlStatus::not_requested};
    FaultControllerSchedulerControlReason reason{FaultControllerSchedulerControlReason::no_scheduler_control_requested};
    std::string target_engine_id{};
    bool scheduler_control_boundary_available{false};
    bool threshold_evidence_complete{false};
    bool operator_approved{false};
    bool rollback_context_available{false};
    bool replay_policy_available{false};
    bool mass_audit_policy_available{false};
    bool scheduler_control_allowed{false};
    bool scheduler_control_used{false};
    bool adapter_call_allowed{false};
    bool exchange_requests_paused{false};
    bool target_engine_request_skipped{false};
    bool replay_held{false};
    bool mass_audit_forced{false};
    bool scheduler_state_mutated{false};
    bool release_gate_action_executed{false};
};

enum class FaultControllerSchedulerControlResultStatus {
    not_requested,
    blocked_boundary_absent,
};

enum class FaultControllerSchedulerControlResultReason {
    no_scheduler_control_requested,
    scheduler_control_boundary_absent,
};

struct FaultControllerSchedulerControlResult {
    FaultControllerSchedulerControlRequest request{};
    FaultControllerSchedulerControlKind attempted_kind{FaultControllerSchedulerControlKind::none};
    FaultControllerSchedulerControlResultStatus status{FaultControllerSchedulerControlResultStatus::not_requested};
    FaultControllerSchedulerControlResultReason reason{FaultControllerSchedulerControlResultReason::no_scheduler_control_requested};
    std::string phase_before_control{};
    std::string phase_after_control{};
    std::string target_engine_id{};
    bool scheduler_control_boundary_available{false};
    bool threshold_evidence_complete{false};
    bool operator_approved{false};
    bool rollback_context_available{false};
    bool replay_policy_available{false};
    bool mass_audit_policy_available{false};
    bool scheduler_control_used{false};
    bool exchange_requests_paused{false};
    bool target_engine_request_skipped{false};
    bool replay_held{false};
    bool mass_audit_forced{false};
    bool scheduler_state_mutated{false};
    bool adapter_call_attempted{false};
    bool adapter_call_succeeded{false};
    bool adapter_call_failed{false};
    bool rollback_required{false};
    bool replay_required{false};
    bool mass_audit_required{false};
    bool operator_review_required{false};
    bool release_gate_action_executed{false};
};

enum class FaultControllerSchedulerPhase {
    health_collection,
    health_classification,
    health_aggregation,
    fault_diagnostic_derivation,
    proposed_action_derivation,
    fault_action_observation,
    fault_action_consumption,
    passive_state_classification,
    passive_transition_creation,
    passive_action_audit_creation,
    passive_action_outcome_creation,
    blocked_action_creation,
    scheduler_control_request_creation,
    scheduler_control_result_creation,
    exchange_request_preparation,
    shared_cell_arbitration,
    exchange_acceptance,
    replay,
    mass_audit,
    post_step_reporting,
};

enum class FaultControllerSchedulerPhaseCategory {
    passive_provenance,
    scheduling_conservation,
};

enum class FaultControllerSchedulerConservationImpact {
    none,
    known_no_change,
    unknown,
};

struct FaultControllerSchedulerPhaseDescriptor {
    FaultControllerSchedulerPhase phase{FaultControllerSchedulerPhase::health_collection};
    FaultControllerSchedulerPhaseCategory category{FaultControllerSchedulerPhaseCategory::passive_provenance};
    std::string phase_name{"health_collection"};
    std::string category_name{"passive_provenance"};
    bool executable_control_allowed{false};
    bool pause_allowed{false};
    bool skip_allowed{false};
    bool replay_hold_allowed{false};
    bool mass_audit_force_allowed{false};
    bool scheduler_state_mutation_allowed{false};
    bool adapter_call_allowed{false};
    bool release_gate_action_allowed{false};
};

struct FaultControllerSchedulerExchangeRequestState {
    std::string request_id{};
    std::string target_engine_id{};
    FaultControllerSchedulerPhaseDescriptor target_phase{};
    bool request_enabled{true};
    bool request_paused{false};
    bool request_skipped{false};
    double priority_weight{1.0};
    double requested_q{0.0};
    double requested_volume{0.0};
    FaultControllerSchedulerConservationImpact expected_conservation_impact{FaultControllerSchedulerConservationImpact::unknown};
    std::string audit_reference_id{};
};

struct FaultControllerSchedulerReplayState {
    bool replay_pending{false};
    bool replay_held{false};
    std::size_t pending_event_count{0U};
    std::string rollback_context_id{};
    std::string resume_condition_id{};
    std::string audit_reference_id{};
};

struct FaultControllerSchedulerMassAuditState {
    bool mass_audit_pending{false};
    bool mass_audit_forced{false};
    double baseline_total_mass{0.0};
    double current_total_mass{0.0};
    double conservation_residual{0.0};
    std::string conservation_status{"conserved"};
    std::string audit_reference_id{};
};

struct FaultControllerSchedulerStepState {
    bool step_active{true};
    bool step_aborted{false};
    std::string abort_reason_id{};
    bool rollback_required{false};
    bool replay_required{false};
    bool mass_audit_required{false};
    bool review_required{false};
    std::string protocol_state{"RUNNING"};
    std::string audit_reference_id{};
};

struct FaultControllerSchedulerMutableState {
    FaultControllerSchedulerPhaseDescriptor current_phase{};
    FaultControllerSchedulerPhaseDescriptor next_phase{};
    std::vector<FaultControllerSchedulerExchangeRequestState> exchange_requests{};
    FaultControllerSchedulerReplayState replay_state{};
    FaultControllerSchedulerMassAuditState mass_audit_state{};
    FaultControllerSchedulerStepState step_state{};
    std::size_t mutation_generation{0U};
    std::string rollback_marker_id{};
    std::string audit_marker_id{};
};

enum class FaultControllerSchedulerEvidenceCompletenessStatus {
    incomplete,
    complete,
};

enum class FaultControllerSchedulerEvidenceCompletenessReason {
    no_scheduler_control_requested,
    scheduler_control_boundary_absent,
    threshold_evidence_missing,
    operator_review_pending,
    rollback_context_missing,
    replay_policy_missing,
    mass_audit_policy_missing,
};

struct FaultControllerSchedulerEvidenceCompleteness {
    FaultControllerSchedulerPhaseDescriptor phase{};
    FaultControllerSchedulerControlResult control_result{};
    FaultControllerSchedulerEvidenceCompletenessStatus status{FaultControllerSchedulerEvidenceCompletenessStatus::incomplete};
    FaultControllerSchedulerEvidenceCompletenessReason reason{FaultControllerSchedulerEvidenceCompletenessReason::no_scheduler_control_requested};
    bool passive_provenance_complete{false};
    bool scheduler_control_boundary_available{false};
    bool threshold_evidence_complete{false};
    bool operator_approved{false};
    bool rollback_context_available{false};
    bool replay_policy_available{false};
    bool mass_audit_policy_available{false};
    bool scheduler_control_allowed{false};
    bool scheduler_control_used{false};
    bool exchange_requests_paused{false};
    bool target_engine_request_skipped{false};
    bool replay_held{false};
    bool mass_audit_forced{false};
    bool scheduler_state_mutated{false};
    bool adapter_call_attempted{false};
    bool release_gate_action_executed{false};
};

enum class FaultControllerSchedulerTargetValidationStatus {
    invalid,
    valid,
};

enum class FaultControllerSchedulerTargetValidationReason {
    no_scheduler_control_requested,
    target_phase_missing,
    target_engine_missing,
    target_engine_not_schedulable,
    conservation_impact_unknown,
    contradictory_action_flags,
};

struct FaultControllerSchedulerTargetValidation {
    FaultControllerSchedulerEvidenceCompleteness evidence{};
    FaultControllerSchedulerPhaseDescriptor target_phase{};
    FaultControllerSchedulerTargetValidationStatus status{FaultControllerSchedulerTargetValidationStatus::invalid};
    FaultControllerSchedulerTargetValidationReason reason{FaultControllerSchedulerTargetValidationReason::no_scheduler_control_requested};
    std::string target_engine_id{};
    bool target_phase_specified{false};
    bool target_engine_specified{false};
    bool target_engine_schedulable{false};
    FaultControllerSchedulerConservationImpact conservation_impact{FaultControllerSchedulerConservationImpact::unknown};
    bool contradictory_flags{false};
    bool scheduler_control_allowed{false};
    bool scheduler_control_used{false};
    bool exchange_requests_paused{false};
    bool target_engine_request_skipped{false};
    bool replay_held{false};
    bool mass_audit_forced{false};
    bool scheduler_state_mutated{false};
    bool adapter_call_attempted{false};
    bool release_gate_action_executed{false};
};

enum class FaultControllerSchedulerOrderingEvidenceStatus {
    invalid,
    valid,
};

enum class FaultControllerSchedulerOrderingEvidenceReason {
    no_scheduler_control_requested,
    ordering_evidence_not_required,
    ordering_evidence_missing,
    ordering_evidence_present,
};

struct FaultControllerSchedulerOrderingEvidenceValidation {
    FaultControllerSchedulerTargetValidation target_validation{};
    FaultControllerSchedulerOrderingEvidenceStatus status{FaultControllerSchedulerOrderingEvidenceStatus::invalid};
    FaultControllerSchedulerOrderingEvidenceReason reason{FaultControllerSchedulerOrderingEvidenceReason::no_scheduler_control_requested};
    bool ordering_evidence_expected{false};
    bool ordering_evidence_present{false};
    bool scheduler_control_allowed{false};
    bool scheduler_control_used{false};
    bool exchange_requests_paused{false};
    bool target_engine_request_skipped{false};
    bool replay_held{false};
    bool mass_audit_forced{false};
    bool scheduler_state_mutated{false};
    bool adapter_call_attempted{false};
    bool release_gate_action_executed{false};
};

enum class FaultControllerSchedulerThresholdEvidenceStatus {
    invalid,
    valid,
};

enum class FaultControllerSchedulerThresholdEvidenceReason {
    no_scheduler_control_requested,
    threshold_evidence_missing,
    threshold_evidence_present,
};

struct FaultControllerSchedulerThresholdEvidenceValidation {
    FaultControllerSchedulerOrderingEvidenceValidation ordering_evidence{};
    FaultControllerSchedulerThresholdEvidenceStatus status{FaultControllerSchedulerThresholdEvidenceStatus::invalid};
    FaultControllerSchedulerThresholdEvidenceReason reason{FaultControllerSchedulerThresholdEvidenceReason::no_scheduler_control_requested};
    bool threshold_evidence_expected{false};
    bool threshold_evidence_present{false};
    bool scheduler_control_allowed{false};
    bool scheduler_control_used{false};
    bool exchange_requests_paused{false};
    bool target_engine_request_skipped{false};
    bool replay_held{false};
    bool mass_audit_forced{false};
    bool scheduler_state_mutated{false};
    bool adapter_call_attempted{false};
    bool release_gate_action_executed{false};
};

enum class FaultControllerSchedulerOperatorApprovalStatus {
    invalid,
    valid,
};

enum class FaultControllerSchedulerOperatorApprovalReason {
    no_scheduler_control_requested,
    operator_approval_not_required,
    operator_approval_pending,
    operator_approval_present,
};

struct FaultControllerSchedulerOperatorApprovalValidation {
    FaultControllerSchedulerThresholdEvidenceStatus threshold_evidence_status{FaultControllerSchedulerThresholdEvidenceStatus::invalid};
    FaultControllerSchedulerThresholdEvidenceReason threshold_evidence_reason{FaultControllerSchedulerThresholdEvidenceReason::no_scheduler_control_requested};
    FaultControllerSchedulerOperatorApprovalStatus status{FaultControllerSchedulerOperatorApprovalStatus::invalid};
    FaultControllerSchedulerOperatorApprovalReason reason{FaultControllerSchedulerOperatorApprovalReason::no_scheduler_control_requested};
    bool threshold_evidence_expected{false};
    bool threshold_evidence_present{false};
    bool operator_review_required{false};
    bool operator_approval_expected{false};
    bool operator_approval_present{false};
    bool scheduler_control_allowed{false};
    bool scheduler_control_used{false};
    bool exchange_requests_paused{false};
    bool target_engine_request_skipped{false};
    bool replay_held{false};
    bool mass_audit_forced{false};
    bool scheduler_state_mutated{false};
    bool adapter_call_attempted{false};
    bool release_gate_action_executed{false};
};

enum class FaultControllerSchedulerRollbackContextStatus {
    invalid,
    valid,
};

enum class FaultControllerSchedulerRollbackContextReason {
    no_scheduler_control_requested,
    rollback_context_missing,
    rollback_context_present,
};

struct FaultControllerSchedulerRollbackContextValidation {
    FaultControllerSchedulerOperatorApprovalStatus operator_approval_status{FaultControllerSchedulerOperatorApprovalStatus::invalid};
    FaultControllerSchedulerOperatorApprovalReason operator_approval_reason{FaultControllerSchedulerOperatorApprovalReason::no_scheduler_control_requested};
    FaultControllerSchedulerRollbackContextStatus status{FaultControllerSchedulerRollbackContextStatus::invalid};
    FaultControllerSchedulerRollbackContextReason reason{FaultControllerSchedulerRollbackContextReason::no_scheduler_control_requested};
    bool operator_approval_expected{false};
    bool operator_approval_present{false};
    bool rollback_context_expected{false};
    bool rollback_context_present{false};
    bool scheduler_control_allowed{false};
    bool scheduler_control_used{false};
    bool exchange_requests_paused{false};
    bool target_engine_request_skipped{false};
    bool replay_held{false};
    bool mass_audit_forced{false};
    bool scheduler_state_mutated{false};
    bool adapter_call_attempted{false};
    bool release_gate_action_executed{false};
};

enum class FaultControllerSchedulerReplayPolicyStatus {
    invalid,
    valid,
};

enum class FaultControllerSchedulerReplayPolicyReason {
    no_scheduler_control_requested,
    replay_policy_missing,
    replay_policy_present,
};

struct FaultControllerSchedulerReplayPolicyValidation {
    FaultControllerSchedulerRollbackContextStatus rollback_context_status{FaultControllerSchedulerRollbackContextStatus::invalid};
    FaultControllerSchedulerRollbackContextReason rollback_context_reason{FaultControllerSchedulerRollbackContextReason::no_scheduler_control_requested};
    FaultControllerSchedulerReplayPolicyStatus status{FaultControllerSchedulerReplayPolicyStatus::invalid};
    FaultControllerSchedulerReplayPolicyReason reason{FaultControllerSchedulerReplayPolicyReason::no_scheduler_control_requested};
    bool rollback_context_expected{false};
    bool rollback_context_present{false};
    bool replay_policy_expected{false};
    bool replay_policy_present{false};
    bool scheduler_control_allowed{false};
    bool scheduler_control_used{false};
    bool exchange_requests_paused{false};
    bool target_engine_request_skipped{false};
    bool replay_held{false};
    bool mass_audit_forced{false};
    bool scheduler_state_mutated{false};
    bool adapter_call_attempted{false};
    bool release_gate_action_executed{false};
};

enum class FaultControllerSchedulerMassAuditPolicyStatus {
    invalid,
    valid,
};

enum class FaultControllerSchedulerMassAuditPolicyReason {
    no_scheduler_control_requested,
    mass_audit_policy_missing,
    mass_audit_policy_present,
};

struct FaultControllerSchedulerMassAuditPolicyValidation {
    FaultControllerSchedulerReplayPolicyStatus replay_policy_status{FaultControllerSchedulerReplayPolicyStatus::invalid};
    FaultControllerSchedulerReplayPolicyReason replay_policy_reason{FaultControllerSchedulerReplayPolicyReason::no_scheduler_control_requested};
    FaultControllerSchedulerMassAuditPolicyStatus status{FaultControllerSchedulerMassAuditPolicyStatus::invalid};
    FaultControllerSchedulerMassAuditPolicyReason reason{FaultControllerSchedulerMassAuditPolicyReason::no_scheduler_control_requested};
    bool replay_policy_expected{false};
    bool replay_policy_present{false};
    bool mass_audit_policy_expected{false};
    bool mass_audit_policy_present{false};
    bool scheduler_control_allowed{false};
    bool scheduler_control_used{false};
    bool exchange_requests_paused{false};
    bool target_engine_request_skipped{false};
    bool replay_held{false};
    bool mass_audit_forced{false};
    bool scheduler_state_mutated{false};
    bool adapter_call_attempted{false};
    bool release_gate_action_executed{false};
};

enum class FaultControllerSchedulerExecutableControlKind {
    none,
    observe_only,
    pause_before_phase,
    skip_target_engine_request,
    hold_replay_until_review,
    force_mass_audit_before_continue,
    abort_scheduler_step,
};

enum class FaultControllerSchedulerPhaseControlCompatibilityStatus {
    incompatible,
    compatible,
};

enum class FaultControllerSchedulerPhaseControlCompatibilityReason {
    no_scheduler_control_requested,
    control_kind_missing,
    control_kind_supported,
    target_phase_not_controllable,
    control_kind_unsupported_for_phase,
};

struct FaultControllerSchedulerPhaseControlCompatibilityValidation {
    FaultControllerSchedulerMassAuditPolicyStatus mass_audit_policy_status{FaultControllerSchedulerMassAuditPolicyStatus::invalid};
    FaultControllerSchedulerMassAuditPolicyReason mass_audit_policy_reason{FaultControllerSchedulerMassAuditPolicyReason::no_scheduler_control_requested};
    FaultControllerSchedulerPhaseDescriptor target_phase{};
    FaultControllerSchedulerExecutableControlKind requested_control_kind{FaultControllerSchedulerExecutableControlKind::none};
    FaultControllerSchedulerPhaseControlCompatibilityStatus status{FaultControllerSchedulerPhaseControlCompatibilityStatus::incompatible};
    FaultControllerSchedulerPhaseControlCompatibilityReason reason{FaultControllerSchedulerPhaseControlCompatibilityReason::no_scheduler_control_requested};
    bool phase_control_expected{false};
    bool phase_control_supported{false};
    bool scheduler_control_allowed{false};
    bool scheduler_control_used{false};
    bool exchange_requests_paused{false};
    bool target_engine_request_skipped{false};
    bool replay_held{false};
    bool mass_audit_forced{false};
    bool scheduler_state_mutated{false};
    bool adapter_call_attempted{false};
    bool release_gate_action_executed{false};
};

enum class FaultControllerExecutableSchedulerControlStatus {
    blocked_boundary_absent,
    blocked_missing_execution_evidence,
    blocked_incompatible_phase_control,
    executed_noop,
};

enum class FaultControllerExecutableSchedulerControlReason {
    scheduler_control_boundary_absent,
    execution_evidence_missing,
    mutating_execution_evidence_missing,
    incompatible_phase_control,
    observe_only_noop,
};

struct FaultControllerExecutableSchedulerControlResult {
    FaultControllerSchedulerPhaseControlCompatibilityStatus compatibility_status{FaultControllerSchedulerPhaseControlCompatibilityStatus::incompatible};
    FaultControllerSchedulerPhaseControlCompatibilityReason compatibility_reason{FaultControllerSchedulerPhaseControlCompatibilityReason::no_scheduler_control_requested};
    FaultControllerSchedulerPhaseDescriptor target_phase{};
    FaultControllerSchedulerExecutableControlKind requested_control_kind{FaultControllerSchedulerExecutableControlKind::none};
    FaultControllerExecutableSchedulerControlStatus status{FaultControllerExecutableSchedulerControlStatus::blocked_boundary_absent};
    FaultControllerExecutableSchedulerControlReason reason{FaultControllerExecutableSchedulerControlReason::scheduler_control_boundary_absent};
    bool scheduler_control_boundary_available{false};
    bool execution_evidence_complete{false};
    bool scheduler_control_allowed{false};
    bool scheduler_control_used{false};
    bool exchange_requests_paused{false};
    bool target_engine_request_skipped{false};
    bool replay_held{false};
    bool mass_audit_forced{false};
    bool scheduler_state_mutated{false};
    bool adapter_call_attempted{false};
    bool release_gate_action_executed{false};
};

enum class FaultControllerPauseBeforePhaseEvidenceStatus {
    incomplete,
    complete,
};

enum class FaultControllerPauseBeforePhaseEvidenceReason {
    no_scheduler_control_requested,
    wrong_control_kind,
    target_phase_not_pause_compatible,
    phase_boundary_missing,
    pause_window_missing,
    resume_condition_missing,
    ordering_impact_missing,
    conservation_impact_missing,
    evidence_present,
};

struct FaultControllerPauseBeforePhaseExecutionEvidence {
    FaultControllerSchedulerPhaseControlCompatibilityStatus compatibility_status{FaultControllerSchedulerPhaseControlCompatibilityStatus::incompatible};
    FaultControllerSchedulerPhaseControlCompatibilityReason compatibility_reason{FaultControllerSchedulerPhaseControlCompatibilityReason::no_scheduler_control_requested};
    FaultControllerSchedulerPhaseDescriptor target_phase{};
    FaultControllerSchedulerExecutableControlKind requested_control_kind{FaultControllerSchedulerExecutableControlKind::none};
    FaultControllerPauseBeforePhaseEvidenceStatus status{FaultControllerPauseBeforePhaseEvidenceStatus::incomplete};
    FaultControllerPauseBeforePhaseEvidenceReason reason{FaultControllerPauseBeforePhaseEvidenceReason::no_scheduler_control_requested};
    bool phase_boundary_identified{false};
    bool pause_window_defined{false};
    bool resume_condition_defined{false};
    bool ordering_impact_reviewed{false};
    bool conservation_impact_reviewed{false};
    bool scheduler_control_allowed{false};
    bool scheduler_control_used{false};
    bool exchange_requests_paused{false};
    bool target_engine_request_skipped{false};
    bool replay_held{false};
    bool mass_audit_forced{false};
    bool scheduler_state_mutated{false};
    bool adapter_call_attempted{false};
    bool release_gate_action_executed{false};
};

enum class FaultControllerSkipTargetEngineEvidenceStatus {
    incomplete,
    complete,
};

enum class FaultControllerSkipTargetEngineEvidenceReason {
    no_scheduler_control_requested,
    wrong_control_kind,
    target_phase_not_skip_compatible,
    target_engine_missing,
    skip_scope_missing,
    ordering_impact_missing,
    conservation_impact_missing,
    replay_continuity_missing,
    mass_audit_continuity_missing,
    evidence_present,
};

struct FaultControllerSkipTargetEngineExecutionEvidence {
    FaultControllerSchedulerPhaseControlCompatibilityStatus compatibility_status{FaultControllerSchedulerPhaseControlCompatibilityStatus::incompatible};
    FaultControllerSchedulerPhaseControlCompatibilityReason compatibility_reason{FaultControllerSchedulerPhaseControlCompatibilityReason::no_scheduler_control_requested};
    FaultControllerSchedulerPhaseDescriptor target_phase{};
    FaultControllerSchedulerExecutableControlKind requested_control_kind{FaultControllerSchedulerExecutableControlKind::none};
    FaultControllerSkipTargetEngineEvidenceStatus status{FaultControllerSkipTargetEngineEvidenceStatus::incomplete};
    FaultControllerSkipTargetEngineEvidenceReason reason{FaultControllerSkipTargetEngineEvidenceReason::no_scheduler_control_requested};
    std::string target_engine_id{};
    bool target_engine_identified{false};
    bool skip_scope_defined{false};
    bool ordering_impact_reviewed{false};
    bool conservation_impact_reviewed{false};
    bool replay_continuity_reviewed{false};
    bool mass_audit_continuity_reviewed{false};
    bool scheduler_control_allowed{false};
    bool scheduler_control_used{false};
    bool exchange_requests_paused{false};
    bool target_engine_request_skipped{false};
    bool replay_held{false};
    bool mass_audit_forced{false};
    bool scheduler_state_mutated{false};
    bool adapter_call_attempted{false};
    bool release_gate_action_executed{false};
};

enum class FaultControllerHoldReplayEvidenceStatus {
    incomplete,
    complete,
};

enum class FaultControllerHoldReplayEvidenceReason {
    no_scheduler_control_requested,
    wrong_control_kind,
    target_phase_not_hold_compatible,
    replay_hold_scope_missing,
    pending_event_state_missing,
    rollback_context_link_missing,
    resume_condition_missing,
    ordering_impact_missing,
    conservation_impact_missing,
    mass_audit_continuity_missing,
    evidence_present,
};

struct FaultControllerHoldReplayExecutionEvidence {
    FaultControllerSchedulerPhaseControlCompatibilityStatus compatibility_status{FaultControllerSchedulerPhaseControlCompatibilityStatus::incompatible};
    FaultControllerSchedulerPhaseControlCompatibilityReason compatibility_reason{FaultControllerSchedulerPhaseControlCompatibilityReason::no_scheduler_control_requested};
    FaultControllerSchedulerPhaseDescriptor target_phase{};
    FaultControllerSchedulerExecutableControlKind requested_control_kind{FaultControllerSchedulerExecutableControlKind::none};
    FaultControllerHoldReplayEvidenceStatus status{FaultControllerHoldReplayEvidenceStatus::incomplete};
    FaultControllerHoldReplayEvidenceReason reason{FaultControllerHoldReplayEvidenceReason::no_scheduler_control_requested};
    bool replay_hold_scope_defined{false};
    bool pending_event_state_defined{false};
    bool rollback_context_linked{false};
    bool resume_condition_defined{false};
    bool ordering_impact_reviewed{false};
    bool conservation_impact_reviewed{false};
    bool mass_audit_continuity_reviewed{false};
    bool scheduler_control_allowed{false};
    bool scheduler_control_used{false};
    bool exchange_requests_paused{false};
    bool target_engine_request_skipped{false};
    bool replay_held{false};
    bool mass_audit_forced{false};
    bool scheduler_state_mutated{false};
    bool adapter_call_attempted{false};
    bool release_gate_action_executed{false};
};

enum class FaultControllerForceMassAuditEvidenceStatus {
    incomplete,
    complete,
};

enum class FaultControllerForceMassAuditEvidenceReason {
    no_scheduler_control_requested,
    wrong_control_kind,
    target_phase_not_audit_compatible,
    mass_audit_scope_missing,
    baseline_mass_state_missing,
    current_mass_state_missing,
    conservation_status_policy_missing,
    ordering_impact_missing,
    replay_continuity_missing,
    evidence_present,
};

struct FaultControllerForceMassAuditExecutionEvidence {
    FaultControllerSchedulerPhaseControlCompatibilityStatus compatibility_status{FaultControllerSchedulerPhaseControlCompatibilityStatus::incompatible};
    FaultControllerSchedulerPhaseControlCompatibilityReason compatibility_reason{FaultControllerSchedulerPhaseControlCompatibilityReason::no_scheduler_control_requested};
    FaultControllerSchedulerPhaseDescriptor target_phase{};
    FaultControllerSchedulerExecutableControlKind requested_control_kind{FaultControllerSchedulerExecutableControlKind::none};
    FaultControllerForceMassAuditEvidenceStatus status{FaultControllerForceMassAuditEvidenceStatus::incomplete};
    FaultControllerForceMassAuditEvidenceReason reason{FaultControllerForceMassAuditEvidenceReason::no_scheduler_control_requested};
    bool mass_audit_scope_defined{false};
    bool baseline_mass_state_preserved{false};
    bool current_mass_state_preserved{false};
    bool conservation_status_policy_defined{false};
    bool ordering_impact_reviewed{false};
    bool replay_continuity_reviewed{false};
    bool scheduler_control_allowed{false};
    bool scheduler_control_used{false};
    bool exchange_requests_paused{false};
    bool target_engine_request_skipped{false};
    bool replay_held{false};
    bool mass_audit_forced{false};
    bool scheduler_state_mutated{false};
    bool adapter_call_attempted{false};
    bool release_gate_action_executed{false};
};

enum class FaultControllerAbortSchedulerStepEvidenceStatus {
    incomplete,
    complete,
};

enum class FaultControllerAbortSchedulerStepEvidenceReason {
    no_scheduler_control_requested,
    wrong_control_kind,
    target_phase_not_abort_compatible,
    abort_scope_missing,
    rollback_context_link_missing,
    replay_policy_continuity_missing,
    mass_audit_policy_continuity_missing,
    ordering_impact_missing,
    conservation_impact_missing,
    release_gate_boundary_missing,
    evidence_present,
};

struct FaultControllerAbortSchedulerStepExecutionEvidence {
    FaultControllerSchedulerPhaseControlCompatibilityStatus compatibility_status{FaultControllerSchedulerPhaseControlCompatibilityStatus::incompatible};
    FaultControllerSchedulerPhaseControlCompatibilityReason compatibility_reason{FaultControllerSchedulerPhaseControlCompatibilityReason::no_scheduler_control_requested};
    FaultControllerSchedulerPhaseDescriptor target_phase{};
    FaultControllerSchedulerExecutableControlKind requested_control_kind{FaultControllerSchedulerExecutableControlKind::none};
    FaultControllerAbortSchedulerStepEvidenceStatus status{FaultControllerAbortSchedulerStepEvidenceStatus::incomplete};
    FaultControllerAbortSchedulerStepEvidenceReason reason{FaultControllerAbortSchedulerStepEvidenceReason::no_scheduler_control_requested};
    bool abort_scope_defined{false};
    bool rollback_context_linked{false};
    bool replay_policy_continuity_reviewed{false};
    bool mass_audit_policy_continuity_reviewed{false};
    bool ordering_impact_reviewed{false};
    bool conservation_impact_reviewed{false};
    bool release_gate_boundary_reviewed{false};
    bool scheduler_control_allowed{false};
    bool scheduler_control_used{false};
    bool exchange_requests_paused{false};
    bool target_engine_request_skipped{false};
    bool replay_held{false};
    bool mass_audit_forced{false};
    bool scheduler_state_mutated{false};
    bool scheduler_step_aborted{false};
    bool adapter_call_attempted{false};
    bool release_gate_action_executed{false};
};

enum class FaultControllerSchedulerMutationAuthorizationStatus {
    not_authorized,
    authorized,
};

enum class FaultControllerSchedulerMutationAuthorizationReason {
    no_scheduler_control_requested,
    authorization_boundary_absent,
    behavior_evidence_missing,
    behavior_evidence_incomplete,
    control_kind_mismatch,
    target_phase_mismatch,
    operator_mutation_approval_missing,
    runtime_mutation_disabled,
    rollback_plan_missing,
    replay_plan_missing,
    mass_audit_plan_missing,
    audit_sink_missing,
    release_gate_policy_missing,
    conservation_impact_not_bounded,
    authorization_present,
};

struct FaultControllerMutationAuthorizationRequest {
    FaultControllerSchedulerExecutableControlKind requested_control_kind{FaultControllerSchedulerExecutableControlKind::none};
    FaultControllerSchedulerPhaseDescriptor target_phase{};
    bool authorization_boundary_available{false};
    bool operator_mutation_approved{false};
    bool runtime_mutation_enabled{false};
    bool rollback_plan_available{false};
    bool replay_plan_available{false};
    bool mass_audit_plan_available{false};
    bool audit_sink_available{false};
    bool release_gate_policy_available{false};
    bool conservation_impact_bounded{false};
};

struct FaultControllerSchedulerMutationAuthorization {
    FaultControllerExecutableSchedulerControlResult executable_result{};
    FaultControllerSchedulerExecutableControlKind requested_control_kind{FaultControllerSchedulerExecutableControlKind::none};
    FaultControllerSchedulerPhaseDescriptor target_phase{};
    FaultControllerSchedulerMutationAuthorizationStatus status{FaultControllerSchedulerMutationAuthorizationStatus::not_authorized};
    FaultControllerSchedulerMutationAuthorizationReason reason{FaultControllerSchedulerMutationAuthorizationReason::no_scheduler_control_requested};
    bool operator_mutation_approved{false};
    bool runtime_mutation_enabled{false};
    bool rollback_plan_available{false};
    bool replay_plan_available{false};
    bool mass_audit_plan_available{false};
    bool audit_sink_available{false};
    bool release_gate_policy_available{false};
    bool conservation_impact_bounded{false};
    bool scheduler_control_allowed{false};
    bool scheduler_state_mutation_allowed{false};
    bool exchange_requests_pause_allowed{false};
    bool target_engine_request_skip_allowed{false};
    bool replay_hold_allowed{false};
    bool mass_audit_force_allowed{false};
    bool scheduler_step_abort_allowed{false};
    bool scheduler_control_used{false};
    bool exchange_requests_paused{false};
    bool target_engine_request_skipped{false};
    bool replay_held{false};
    bool mass_audit_forced{false};
    bool scheduler_step_aborted{false};
    bool adapter_call_attempted{false};
    bool release_gate_action_executed{false};
};

enum class FaultControllerSchedulerMutationExecutionAttemptStatus {
    not_attempted,
    attempt_recorded,
};

enum class FaultControllerSchedulerMutationExecutionAttemptReason {
    authorization_not_granted,
    execution_boundary_absent,
    pre_state_reference_missing,
    runtime_evidence_sink_missing,
    attempt_recorded,
};

struct FaultControllerSchedulerMutationExecutionAttemptRequest {
    bool execution_boundary_available{false};
    bool pre_state_reference_available{false};
    bool runtime_evidence_sink_available{false};
    std::string pre_step_phase_name{};
    std::size_t pre_step_pending_event_count{0U};
    std::size_t pre_step_exchange_request_count{0U};
    bool pre_step_replay_pending{false};
    double pre_step_mass_total{0.0};
    std::string pre_step_conservation_status{"conserved"};
};

struct FaultControllerSchedulerMutationExecutionAttempt {
    FaultControllerSchedulerMutationAuthorization authorization{};
    FaultControllerSchedulerMutationExecutionAttemptStatus status{FaultControllerSchedulerMutationExecutionAttemptStatus::not_attempted};
    FaultControllerSchedulerMutationExecutionAttemptReason reason{FaultControllerSchedulerMutationExecutionAttemptReason::authorization_not_granted};
    FaultControllerSchedulerExecutableControlKind requested_control_kind{FaultControllerSchedulerExecutableControlKind::none};
    FaultControllerSchedulerPhaseDescriptor target_phase{};
    bool execution_boundary_available{false};
    bool pre_state_reference_available{false};
    bool runtime_evidence_sink_available{false};
    std::string pre_step_phase_name{};
    std::size_t pre_step_pending_event_count{0U};
    std::size_t pre_step_exchange_request_count{0U};
    bool pre_step_replay_pending{false};
    double pre_step_mass_total{0.0};
    std::string pre_step_conservation_status{"conserved"};
    bool scheduler_control_attempted{false};
    bool exchange_requests_paused{false};
    bool target_engine_request_skipped{false};
    bool replay_held{false};
    bool mass_audit_forced{false};
    bool scheduler_step_aborted{false};
    bool adapter_call_attempted{false};
    bool release_gate_action_executed{false};
};

enum class FaultControllerSchedulerMutationPostconditionStatus {
    not_evaluated,
    verified_no_change,
    verified_mutation,
    failed_postcondition,
    review_required,
};

enum class FaultControllerSchedulerMutationPostconditionReason {
    execution_not_attempted,
    post_state_reference_missing,
    unexpected_phase_change,
    unexpected_exchange_request_change,
    unexpected_replay_change,
    unexpected_mass_audit_change,
    conservation_residual_unbounded,
    release_gate_policy_missing,
    verified_expected_no_change,
    verified_expected_mutation,
    review_required,
};

struct FaultControllerSchedulerMutationPostconditionRequest {
    bool post_state_reference_available{false};
    bool expected_mutation{false};
    bool phase_changed{false};
    bool exchange_request_changed{false};
    bool replay_changed{false};
    bool mass_audit_changed{false};
    double conservation_residual{0.0};
    bool conservation_residual_bounded{false};
    bool release_gate_policy_available{false};
    std::string post_step_phase_name{};
    std::size_t post_step_pending_event_count{0U};
    std::size_t post_step_exchange_request_count{0U};
    bool post_step_replay_pending{false};
    double post_step_mass_total{0.0};
};

struct FaultControllerSchedulerMutationPostconditionEvidence {
    FaultControllerSchedulerMutationExecutionAttempt attempt{};
    FaultControllerSchedulerMutationPostconditionStatus status{FaultControllerSchedulerMutationPostconditionStatus::not_evaluated};
    FaultControllerSchedulerMutationPostconditionReason reason{FaultControllerSchedulerMutationPostconditionReason::execution_not_attempted};
    FaultControllerSchedulerExecutableControlKind requested_control_kind{FaultControllerSchedulerExecutableControlKind::none};
    FaultControllerSchedulerPhaseDescriptor target_phase{};
    std::string pre_step_phase_name{};
    std::string post_step_phase_name{};
    std::size_t pre_step_pending_event_count{0U};
    std::size_t post_step_pending_event_count{0U};
    std::size_t pre_step_exchange_request_count{0U};
    std::size_t post_step_exchange_request_count{0U};
    bool pre_step_replay_pending{false};
    bool post_step_replay_pending{false};
    double pre_step_mass_total{0.0};
    double post_step_mass_total{0.0};
    double conservation_residual{0.0};
    bool conservation_residual_bounded{false};
    bool exchange_requests_paused{false};
    bool target_engine_request_skipped{false};
    bool replay_held{false};
    bool mass_audit_forced{false};
    bool scheduler_step_aborted{false};
    bool adapter_call_attempted{false};
    bool release_gate_action_executed{false};
    bool operator_review_required{false};
    std::string protocol_state{};
};

enum class FaultControllerSchedulerMutationRuntimeEvidenceWriteStatus {
    not_written,
    written,
    blocked,
};

enum class FaultControllerSchedulerMutationRuntimeEvidenceWriteReason {
    runtime_evidence_sink_missing,
    postcondition_missing,
    postcondition_failed,
    write_blocked_by_release_gate_policy,
    written,
};

struct FaultControllerSchedulerMutationRuntimeEvidenceWriteRecord {
    FaultControllerSchedulerMutationPostconditionEvidence postcondition{};
    FaultControllerSchedulerMutationRuntimeEvidenceWriteStatus status{FaultControllerSchedulerMutationRuntimeEvidenceWriteStatus::not_written};
    FaultControllerSchedulerMutationRuntimeEvidenceWriteReason reason{FaultControllerSchedulerMutationRuntimeEvidenceWriteReason::runtime_evidence_sink_missing};
    std::string evidence_schema_name{};
    std::string evidence_schema_version{};
    std::string evidence_record_id{};
    bool runtime_evidence_sink_available{false};
    bool release_gate_policy_allows_write{false};
    bool write_attempted{false};
    bool write_succeeded{false};
    bool write_failed{false};
    bool release_gate_action_executed{false};
    std::string protocol_state{};
};

enum class FaultControllerSchedulerMutationDryRunStatus {
    not_evaluated,
    blocked,
    dry_run_recorded,
};

enum class FaultControllerSchedulerMutationDryRunReason {
    authorization_not_granted,
    execution_attempt_not_recorded,
    dry_run_boundary_absent,
    runtime_evidence_sink_missing,
    postcondition_template_missing,
    unsupported_control_kind,
    control_kind_mismatch,
    target_phase_mismatch,
    dry_run_recorded,
};

struct FaultControllerSchedulerMutationDryRunRequest {
    FaultControllerSchedulerExecutableControlKind requested_control_kind{FaultControllerSchedulerExecutableControlKind::none};
    FaultControllerSchedulerPhaseDescriptor target_phase{};
    bool dry_run_boundary_available{false};
    bool runtime_evidence_sink_available{false};
    bool postcondition_template_available{false};
    std::string expected_phase_before{};
    std::string expected_phase_after{};
    std::size_t expected_affected_exchange_request_count{0U};
    std::size_t expected_affected_target_engine_count{0U};
    bool expected_replay_pending{false};
    bool expected_mass_audit_required{false};
    bool expected_conservation_impact_bounded{false};
};

struct FaultControllerSchedulerMutationDryRunResult {
    FaultControllerSchedulerMutationAuthorization authorization{};
    FaultControllerSchedulerMutationExecutionAttempt attempt{};
    FaultControllerSchedulerMutationDryRunStatus status{FaultControllerSchedulerMutationDryRunStatus::not_evaluated};
    FaultControllerSchedulerMutationDryRunReason reason{FaultControllerSchedulerMutationDryRunReason::authorization_not_granted};
    FaultControllerSchedulerExecutableControlKind requested_control_kind{FaultControllerSchedulerExecutableControlKind::none};
    FaultControllerSchedulerPhaseDescriptor target_phase{};
    std::string phase_before_dry_run{};
    std::string phase_after_dry_run{};
    std::size_t expected_affected_exchange_request_count{0U};
    std::size_t expected_affected_target_engine_count{0U};
    bool expected_replay_pending{false};
    bool expected_mass_audit_required{false};
    bool expected_conservation_impact_bounded{false};
    bool would_pause_exchange_requests{false};
    bool would_skip_target_engine_request{false};
    bool would_hold_replay{false};
    bool would_force_mass_audit{false};
    bool would_abort_scheduler_step{false};
    bool scheduler_control_used{false};
    bool exchange_requests_paused{false};
    bool target_engine_request_skipped{false};
    bool replay_held{false};
    bool mass_audit_forced{false};
    bool scheduler_step_aborted{false};
    bool adapter_call_attempted{false};
    bool release_gate_action_executed{false};
    bool runtime_evidence_sink_available{false};
    bool postcondition_template_available{false};
    bool dry_run_evidence_recorded{false};
};

enum class FaultControllerSchedulerMutationExecutionResultStatus {
    not_executed,
    blocked,
    executed,
    review_required,
};

enum class FaultControllerSchedulerMutationExecutionResultReason {
    authorization_not_granted,
    execution_attempt_not_recorded,
    dry_run_not_recorded,
    execution_boundary_absent,
    upstream_control_kind_mismatch,
    upstream_target_phase_mismatch,
    scheduler_state_missing,
    target_phase_not_current,
    target_engine_missing,
    target_engine_not_found,
    rollback_plan_missing,
    replay_plan_missing,
    mass_audit_plan_missing,
    runtime_evidence_sink_missing,
    postcondition_template_missing,
    unsupported_control_kind,
    executed_pause,
    executed_skip_target_engine_request,
    executed_hold_replay,
    executed_force_mass_audit,
    executed_abort_scheduler_step,
    postcondition_required,
    review_required,
};

struct FaultControllerSchedulerMutationExecutionRequest {
    FaultControllerSchedulerMutationAuthorization authorization{};
    FaultControllerSchedulerMutationExecutionAttempt attempt{};
    FaultControllerSchedulerMutationDryRunResult dry_run{};
    FaultControllerSchedulerMutableState scheduler_state{};
    bool scheduler_state_present{false};
    FaultControllerSchedulerExecutableControlKind requested_control_kind{FaultControllerSchedulerExecutableControlKind::none};
    FaultControllerSchedulerPhaseDescriptor target_phase{};
    std::string target_engine_id{};
    bool execution_boundary_available{false};
    bool runtime_evidence_sink_available{false};
    bool postcondition_template_available{false};
    std::string rollback_plan_reference_id{};
    std::string replay_plan_reference_id{};
    std::string mass_audit_plan_reference_id{};
    std::string release_gate_policy_reference_id{};
};

struct FaultControllerSchedulerMutationExecutionResult {
    FaultControllerSchedulerMutationExecutionRequest request{};
    FaultControllerSchedulerMutationExecutionResultStatus status{FaultControllerSchedulerMutationExecutionResultStatus::not_executed};
    FaultControllerSchedulerMutationExecutionResultReason reason{FaultControllerSchedulerMutationExecutionResultReason::authorization_not_granted};
    FaultControllerSchedulerExecutableControlKind requested_control_kind{FaultControllerSchedulerExecutableControlKind::none};
    FaultControllerSchedulerPhaseDescriptor target_phase{};
    std::string target_engine_id{};
    FaultControllerSchedulerMutableState scheduler_state_before{};
    FaultControllerSchedulerMutableState scheduler_state_after{};
    std::size_t mutation_generation_before{0U};
    std::size_t mutation_generation_after{0U};
    FaultControllerSchedulerExecutableControlKind expected_mutation_kind{FaultControllerSchedulerExecutableControlKind::none};
    FaultControllerSchedulerExecutableControlKind actual_mutation_kind{FaultControllerSchedulerExecutableControlKind::none};
    bool exchange_requests_paused{false};
    bool target_engine_request_skipped{false};
    bool replay_held{false};
    bool mass_audit_forced{false};
    bool scheduler_step_aborted{false};
    bool non_target_behavior_changed{false};
    bool rollback_required{false};
    bool replay_required{false};
    bool mass_audit_required{false};
    bool operator_review_required{false};
    bool adapter_call_attempted{false};
    bool release_gate_action_executed{false};
    bool postcondition_required{false};
};

enum class FaultControllerSchedulerMutationCompletionStatus {
    incomplete,
    complete,
    review_required,
};

enum class FaultControllerSchedulerMutationCompletionReason {
    execution_not_completed,
    postcondition_missing,
    postcondition_not_verified,
    control_kind_mismatch,
    target_phase_mismatch,
    mutation_kind_mismatch,
    mutation_generation_mismatch,
    adapter_call_attempted,
    release_gate_action_executed,
    postcondition_verified,
    review_required,
};

struct FaultControllerSchedulerMutationCompletionRecord {
    FaultControllerSchedulerMutationExecutionResult execution_result{};
    FaultControllerSchedulerMutationPostconditionEvidence postcondition{};
    FaultControllerSchedulerMutationCompletionStatus status{FaultControllerSchedulerMutationCompletionStatus::incomplete};
    FaultControllerSchedulerMutationCompletionReason reason{FaultControllerSchedulerMutationCompletionReason::execution_not_completed};
    FaultControllerSchedulerExecutableControlKind requested_control_kind{FaultControllerSchedulerExecutableControlKind::none};
    FaultControllerSchedulerPhaseDescriptor target_phase{};
    std::size_t mutation_generation_before{0U};
    std::size_t mutation_generation_after{0U};
    bool postcondition_present{false};
    bool postcondition_verified{false};
    bool mutation_completed{false};
    bool operator_review_required{false};
    bool adapter_call_attempted{false};
    bool release_gate_action_executed{false};
};

enum class FaultControllerSchedulerMutationRuntimeWriteIntentStatus {
    not_prepared,
    blocked,
    prepared,
    review_required,
};

enum class FaultControllerSchedulerMutationRuntimeWriteIntentReason {
    mutation_not_completed,
    completion_requires_review,
    write_boundary_absent,
    runtime_evidence_sink_missing,
    schema_name_missing,
    schema_version_missing,
    evidence_record_id_missing,
    state_before_reference_missing,
    state_after_reference_missing,
    postcondition_reference_missing,
    release_gate_policy_reference_missing,
    adapter_call_attempted,
    release_gate_action_already_executed,
    write_intent_prepared,
    review_required,
};

struct FaultControllerSchedulerMutationRuntimeWriteIntentRequest {
    FaultControllerSchedulerMutationCompletionRecord completion{};
    bool write_boundary_available{false};
    bool runtime_evidence_sink_available{false};
    std::string evidence_schema_name{};
    std::string evidence_schema_version{};
    std::string evidence_record_id{};
    std::string scheduler_state_before_reference_id{};
    std::string scheduler_state_after_reference_id{};
    std::string postcondition_evidence_reference_id{};
    std::string release_gate_policy_reference_id{};
};

struct FaultControllerSchedulerMutationRuntimeWriteIntent {
    FaultControllerSchedulerMutationRuntimeWriteIntentRequest request{};
    FaultControllerSchedulerMutationRuntimeWriteIntentStatus status{FaultControllerSchedulerMutationRuntimeWriteIntentStatus::not_prepared};
    FaultControllerSchedulerMutationRuntimeWriteIntentReason reason{FaultControllerSchedulerMutationRuntimeWriteIntentReason::mutation_not_completed};
    FaultControllerSchedulerExecutableControlKind requested_control_kind{FaultControllerSchedulerExecutableControlKind::none};
    FaultControllerSchedulerPhaseDescriptor target_phase{};
    std::size_t mutation_generation_before{0U};
    std::size_t mutation_generation_after{0U};
    std::string evidence_schema_name{};
    std::string evidence_schema_version{};
    std::string evidence_record_id{};
    std::string scheduler_state_before_reference_id{};
    std::string scheduler_state_after_reference_id{};
    std::string postcondition_evidence_reference_id{};
    std::string release_gate_policy_reference_id{};
    bool write_intent_prepared{false};
    bool external_write_attempted{false};
    bool external_write_succeeded{false};
    bool external_write_failed{false};
    bool adapter_call_attempted{false};
    bool release_gate_action_executed{false};
    bool operator_review_required{false};
};

enum class FaultControllerSchedulerReleaseGatePolicyDecisionStatus {
    not_evaluated,
    blocked,
    decision_recorded,
    review_required,
};

enum class FaultControllerSchedulerReleaseGatePolicyDecisionReason {
    write_intent_not_prepared,
    write_intent_requires_review,
    runtime_write_missing,
    runtime_write_failed,
    policy_boundary_absent,
    policy_reference_missing,
    operator_review_policy_missing,
    merge_gate_policy_missing,
    release_gate_policy_missing,
    golden_suite_evidence_missing,
    ci_evidence_missing,
    release_evidence_missing,
    adapter_call_attempted,
    upstream_release_gate_action_detected,
    review_required_decision,
    block_merge_decision,
    block_release_decision,
    abort_decision,
    pass_decision,
};

struct FaultControllerSchedulerReleaseGatePolicyDecisionRequest {
    FaultControllerSchedulerMutationRuntimeWriteIntent write_intent{};
    bool runtime_evidence_write_completed{false};
    bool runtime_evidence_write_succeeded{false};
    bool runtime_evidence_write_failed{false};
    bool policy_boundary_available{false};
    std::string policy_reference_id{};
    bool operator_review_policy_enabled{false};
    bool merge_gate_evaluation_enabled{false};
    bool release_gate_evaluation_enabled{false};
    bool merge_gate_policy_enabled{false};
    bool release_gate_policy_enabled{false};
    bool abort_escalation_policy_enabled{false};
    bool golden_suite_evidence_available{false};
    bool ci_evidence_available{false};
    bool release_evidence_available{false};
};

struct FaultControllerSchedulerReleaseGatePolicyDecision {
    FaultControllerSchedulerReleaseGatePolicyDecisionRequest request{};
    FaultControllerSchedulerReleaseGatePolicyDecisionStatus status{FaultControllerSchedulerReleaseGatePolicyDecisionStatus::not_evaluated};
    FaultControllerSchedulerReleaseGatePolicyDecisionReason reason{FaultControllerSchedulerReleaseGatePolicyDecisionReason::write_intent_not_prepared};
    FaultControllerSchedulerExecutableControlKind requested_control_kind{FaultControllerSchedulerExecutableControlKind::none};
    FaultControllerSchedulerPhaseDescriptor target_phase{};
    std::size_t mutation_generation_before{0U};
    std::size_t mutation_generation_after{0U};
    std::string evidence_record_id{};
    std::string policy_reference_id{};
    bool review_required{false};
    bool merge_blocked{false};
    bool release_blocked{false};
    bool abort_required{false};
    bool policy_decision_recorded{false};
    bool release_gate_action_emitted{false};
    bool external_write_attempted{false};
    bool adapter_call_attempted{false};
};

enum class FaultControllerSchedulerRuntimeEvidenceWriteStatus {
    not_attempted,
    blocked,
    dry_run_recorded,
    written,
    failed,
    review_required,
};

enum class FaultControllerSchedulerRuntimeEvidenceWriteReason {
    write_intent_not_prepared,
    writer_boundary_absent,
    evidence_sink_missing,
    sink_transaction_missing,
    idempotency_key_missing,
    payload_reference_missing,
    payload_schema_invalid,
    payload_hash_missing,
    persistence_target_missing,
    write_mode_missing,
    conflicting_write_mode,
    dry_run_recorded,
    write_succeeded,
    write_failed,
    review_required,
};

struct FaultControllerSchedulerRuntimeEvidenceWriteRequest {
    FaultControllerSchedulerMutationRuntimeWriteIntent write_intent{};
    bool writer_boundary_available{false};
    bool evidence_sink_available{false};
    std::string sink_transaction_id{};
    std::string idempotency_key{};
    std::string payload_reference_id{};
    bool payload_schema_valid{false};
    std::string payload_content_hash{};
    std::string persistence_target_reference_id{};
    bool write_dry_run{false};
    bool write_commit{false};
    bool sink_write_succeeded{false};
};

struct FaultControllerSchedulerRuntimeEvidenceWriteResult {
    FaultControllerSchedulerRuntimeEvidenceWriteRequest request{};
    FaultControllerSchedulerRuntimeEvidenceWriteStatus status{FaultControllerSchedulerRuntimeEvidenceWriteStatus::not_attempted};
    FaultControllerSchedulerRuntimeEvidenceWriteReason reason{FaultControllerSchedulerRuntimeEvidenceWriteReason::write_intent_not_prepared};
    FaultControllerSchedulerExecutableControlKind requested_control_kind{FaultControllerSchedulerExecutableControlKind::none};
    FaultControllerSchedulerPhaseDescriptor target_phase{};
    std::size_t mutation_generation_before{0U};
    std::size_t mutation_generation_after{0U};
    std::string evidence_schema_name{};
    std::string evidence_schema_version{};
    std::string evidence_record_id{};
    std::string scheduler_state_before_reference_id{};
    std::string scheduler_state_after_reference_id{};
    std::string postcondition_evidence_reference_id{};
    std::string release_gate_policy_reference_id{};
    std::string sink_transaction_id{};
    std::string idempotency_key{};
    std::string payload_reference_id{};
    std::string payload_content_hash{};
    std::string persistence_target_reference_id{};
    bool write_attempted{false};
    bool write_dry_run_recorded{false};
    bool write_committed{false};
    bool write_succeeded{false};
    bool write_failed{false};
    bool operator_review_required{false};
    bool release_gate_action_emitted{false};
};

enum class FaultControllerSchedulerReleaseGateActionIntentStatus {
    not_prepared,
    blocked,
    prepared,
    review_required,
};

enum class FaultControllerSchedulerReleaseGateActionIntentReason {
    policy_decision_not_recorded,
    runtime_write_not_committed,
    action_boundary_absent,
    action_sink_missing,
    action_reference_missing,
    policy_reference_missing,
    evidence_record_missing,
    unsupported_action_combination,
    pass_no_action,
    review_required_action_prepared,
    block_merge_action_prepared,
    block_release_action_prepared,
    abort_action_prepared,
};

struct FaultControllerSchedulerReleaseGateActionIntentRequest {
    FaultControllerSchedulerReleaseGatePolicyDecision policy_decision{};
    FaultControllerSchedulerRuntimeEvidenceWriteResult runtime_write{};
    bool action_boundary_available{false};
    bool action_sink_available{false};
    std::string action_reference_id{};
};

struct FaultControllerSchedulerReleaseGateActionIntent {
    FaultControllerSchedulerReleaseGateActionIntentRequest request{};
    FaultControllerSchedulerReleaseGateActionIntentStatus status{FaultControllerSchedulerReleaseGateActionIntentStatus::not_prepared};
    FaultControllerSchedulerReleaseGateActionIntentReason reason{FaultControllerSchedulerReleaseGateActionIntentReason::policy_decision_not_recorded};
    FaultControllerSchedulerExecutableControlKind requested_control_kind{FaultControllerSchedulerExecutableControlKind::none};
    FaultControllerSchedulerPhaseDescriptor target_phase{};
    std::size_t mutation_generation_before{0U};
    std::size_t mutation_generation_after{0U};
    std::string evidence_record_id{};
    std::string policy_reference_id{};
    std::string action_reference_id{};
    bool review_required{false};
    bool merge_blocked{false};
    bool release_blocked{false};
    bool abort_required{false};
    bool action_intent_prepared{false};
    bool release_gate_action_emitted{false};
    bool external_write_attempted{false};
    bool adapter_call_attempted{false};
};

enum class FaultControllerSchedulerReleaseGateActionEmissionStatus {
    not_attempted,
    blocked,
    dry_run_recorded,
    emitted,
    failed,
    review_required,
};

enum class FaultControllerSchedulerReleaseGateActionEmissionReason {
    action_intent_not_prepared,
    emitter_boundary_absent,
    action_sink_missing,
    action_transaction_missing,
    idempotency_key_missing,
    action_payload_missing,
    action_payload_schema_invalid,
    action_payload_hash_missing,
    emission_mode_missing,
    conflicting_emission_mode,
    dry_run_recorded,
    action_emitted,
    action_emit_failed,
    review_required,
};

struct FaultControllerSchedulerReleaseGateActionEmissionRequest {
    FaultControllerSchedulerReleaseGateActionIntent action_intent{};
    bool emitter_boundary_available{false};
    bool action_sink_available{false};
    std::string action_transaction_id{};
    std::string idempotency_key{};
    std::string action_payload_reference_id{};
    bool action_payload_schema_valid{false};
    std::string action_payload_content_hash{};
    bool emission_dry_run{false};
    bool emission_commit{false};
    bool action_sink_emit_succeeded{false};
};

struct FaultControllerSchedulerReleaseGateActionEmissionResult {
    FaultControllerSchedulerReleaseGateActionEmissionRequest request{};
    FaultControllerSchedulerReleaseGateActionEmissionStatus status{FaultControllerSchedulerReleaseGateActionEmissionStatus::not_attempted};
    FaultControllerSchedulerReleaseGateActionEmissionReason reason{FaultControllerSchedulerReleaseGateActionEmissionReason::action_intent_not_prepared};
    FaultControllerSchedulerExecutableControlKind requested_control_kind{FaultControllerSchedulerExecutableControlKind::none};
    FaultControllerSchedulerPhaseDescriptor target_phase{};
    std::size_t mutation_generation_before{0U};
    std::size_t mutation_generation_after{0U};
    std::string evidence_record_id{};
    std::string policy_reference_id{};
    std::string action_reference_id{};
    std::string action_transaction_id{};
    std::string idempotency_key{};
    std::string action_payload_reference_id{};
    std::string action_payload_content_hash{};
    bool review_required{false};
    bool merge_blocked{false};
    bool release_blocked{false};
    bool abort_required{false};
    bool action_emit_attempted{false};
    bool action_dry_run_recorded{false};
    bool action_emitted{false};
    bool action_emit_failed{false};
    bool external_write_attempted{false};
    bool adapter_call_attempted{false};
};

enum class FaultControllerSchedulerReleaseGateActionCompletionStatus {
    incomplete,
    complete,
    failed,
    review_required,
};

enum class FaultControllerSchedulerReleaseGateActionCompletionReason {
    emission_not_completed,
    dry_run_only,
    emission_failed,
    action_reference_missing,
    action_transaction_missing,
    unsupported_action_combination,
    pass_no_action_complete,
    action_emission_complete,
    review_required,
};

struct FaultControllerSchedulerReleaseGateActionCompletionRecord {
    FaultControllerSchedulerReleaseGateActionCompletionStatus status{FaultControllerSchedulerReleaseGateActionCompletionStatus::incomplete};
    FaultControllerSchedulerReleaseGateActionCompletionReason reason{FaultControllerSchedulerReleaseGateActionCompletionReason::emission_not_completed};
    FaultControllerSchedulerExecutableControlKind requested_control_kind{FaultControllerSchedulerExecutableControlKind::none};
    FaultControllerSchedulerPhaseDescriptor target_phase{};
    std::size_t mutation_generation_before{0U};
    std::size_t mutation_generation_after{0U};
    std::string evidence_record_id{};
    std::string policy_reference_id{};
    std::string action_reference_id{};
    std::string action_transaction_id{};
    bool review_required{false};
    bool merge_blocked{false};
    bool release_blocked{false};
    bool abort_required{false};
    bool action_completed{false};
    bool action_failed{false};
    bool action_dry_run_only{false};
    bool external_write_attempted{false};
    bool adapter_call_attempted{false};
};

[[nodiscard]] EngineHealthDiagnostic classify_engine_health(const EngineReport& report);
[[nodiscard]] EngineHealthAggregate aggregate_engine_health(
    const std::vector<EngineHealthDiagnostic>& diagnostics);
[[nodiscard]] FaultControllerDiagnostic make_fault_controller_diagnostic(
    const EngineHealthAggregate& health);
[[nodiscard]] FaultControllerProposedAction propose_fault_controller_action(
    const FaultControllerDiagnostic& diagnostic);
[[nodiscard]] MockCouplingSchedulerFaultObservation observe_mock_scheduler_fault_action(
    const FaultControllerProposedAction& action);
[[nodiscard]] MockCouplingSchedulerFaultConsumption consume_mock_scheduler_fault_action(
    const MockCouplingSchedulerFaultObservation& observation);
[[nodiscard]] FaultControllerPassiveStateClassification classify_fault_controller_passive_state(
    const MockCouplingSchedulerFaultConsumption& consumption);
[[nodiscard]] FaultControllerPassiveTransition make_fault_controller_passive_transition(
    FaultControllerPassiveStateLabel previous_state,
    const FaultControllerPassiveStateClassification& classification);
[[nodiscard]] FaultControllerPassiveActionAuditRecord make_fault_controller_passive_action_audit_record(
    const FaultControllerPassiveTransition& transition);
[[nodiscard]] FaultControllerPassiveActionOutcome make_fault_controller_passive_action_outcome(
    const FaultControllerPassiveActionAuditRecord& audit);
[[nodiscard]] FaultControllerBlockedAction make_fault_controller_blocked_action(
    const FaultControllerPassiveActionOutcome& outcome);
[[nodiscard]] FaultControllerSchedulerControlRequest make_fault_controller_scheduler_control_request(
    const FaultControllerBlockedAction& blocked_action);
[[nodiscard]] FaultControllerSchedulerControlResult make_fault_controller_scheduler_control_result(
    const FaultControllerSchedulerControlRequest& request);
[[nodiscard]] FaultControllerSchedulerPhaseDescriptor describe_fault_controller_scheduler_phase(
    FaultControllerSchedulerPhase phase);
[[nodiscard]] FaultControllerSchedulerEvidenceCompleteness make_fault_controller_scheduler_evidence_completeness(
    const FaultControllerSchedulerPhaseDescriptor& phase,
    const FaultControllerSchedulerControlResult& control_result);
[[nodiscard]] FaultControllerSchedulerTargetValidation make_fault_controller_scheduler_target_validation(
    const FaultControllerSchedulerEvidenceCompleteness& evidence,
    std::string target_engine_id,
    bool target_engine_schedulable,
    FaultControllerSchedulerConservationImpact conservation_impact,
    bool contradictory_flags);
[[nodiscard]] FaultControllerSchedulerOrderingEvidenceValidation make_fault_controller_scheduler_ordering_evidence_validation(
    const FaultControllerSchedulerTargetValidation& target_validation,
    bool ordering_evidence_present);
[[nodiscard]] FaultControllerSchedulerThresholdEvidenceValidation make_fault_controller_scheduler_threshold_evidence_validation(
    const FaultControllerSchedulerOrderingEvidenceValidation& ordering_evidence,
    bool threshold_evidence_present);
[[nodiscard]] FaultControllerSchedulerOperatorApprovalValidation make_fault_controller_scheduler_operator_approval_validation(
    const FaultControllerSchedulerThresholdEvidenceValidation& threshold_evidence,
    bool operator_approval_present);
[[nodiscard]] FaultControllerSchedulerRollbackContextValidation make_fault_controller_scheduler_rollback_context_validation(
    const FaultControllerSchedulerOperatorApprovalValidation& operator_approval,
    bool rollback_context_present);
[[nodiscard]] FaultControllerSchedulerReplayPolicyValidation make_fault_controller_scheduler_replay_policy_validation(
    const FaultControllerSchedulerRollbackContextValidation& rollback_context,
    bool replay_policy_present);
[[nodiscard]] FaultControllerSchedulerMassAuditPolicyValidation make_fault_controller_scheduler_mass_audit_policy_validation(
    const FaultControllerSchedulerReplayPolicyValidation& replay_policy,
    bool mass_audit_policy_present);
[[nodiscard]] FaultControllerSchedulerPhaseControlCompatibilityValidation make_fault_controller_scheduler_phase_control_compatibility_validation(
    const FaultControllerSchedulerMassAuditPolicyValidation& mass_audit_policy,
    const FaultControllerSchedulerPhaseDescriptor& target_phase,
    FaultControllerSchedulerExecutableControlKind requested_control_kind);
[[nodiscard]] FaultControllerExecutableSchedulerControlResult execute_fault_controller_scheduler_control(
    const FaultControllerSchedulerPhaseControlCompatibilityValidation& compatibility,
    bool scheduler_control_boundary_available,
    bool execution_evidence_complete);
[[nodiscard]] FaultControllerExecutableSchedulerControlResult execute_fault_controller_scheduler_control(
    const FaultControllerPauseBeforePhaseExecutionEvidence& evidence,
    bool scheduler_control_boundary_available);
[[nodiscard]] FaultControllerExecutableSchedulerControlResult execute_fault_controller_scheduler_control(
    const FaultControllerSkipTargetEngineExecutionEvidence& evidence,
    bool scheduler_control_boundary_available);
[[nodiscard]] FaultControllerExecutableSchedulerControlResult execute_fault_controller_scheduler_control(
    const FaultControllerHoldReplayExecutionEvidence& evidence,
    bool scheduler_control_boundary_available);
[[nodiscard]] FaultControllerExecutableSchedulerControlResult execute_fault_controller_scheduler_control(
    const FaultControllerForceMassAuditExecutionEvidence& evidence,
    bool scheduler_control_boundary_available);
[[nodiscard]] FaultControllerExecutableSchedulerControlResult execute_fault_controller_scheduler_control(
    const FaultControllerAbortSchedulerStepExecutionEvidence& evidence,
    bool scheduler_control_boundary_available);
[[nodiscard]] FaultControllerPauseBeforePhaseExecutionEvidence make_fault_controller_pause_before_phase_execution_evidence(
    const FaultControllerSchedulerPhaseControlCompatibilityValidation& compatibility,
    bool phase_boundary_identified,
    bool pause_window_defined,
    bool resume_condition_defined,
    bool ordering_impact_reviewed,
    bool conservation_impact_reviewed);
[[nodiscard]] FaultControllerSkipTargetEngineExecutionEvidence make_fault_controller_skip_target_engine_execution_evidence(
    const FaultControllerSchedulerPhaseControlCompatibilityValidation& compatibility,
    std::string target_engine_id,
    bool skip_scope_defined,
    bool ordering_impact_reviewed,
    bool conservation_impact_reviewed,
    bool replay_continuity_reviewed,
    bool mass_audit_continuity_reviewed);
[[nodiscard]] FaultControllerHoldReplayExecutionEvidence make_fault_controller_hold_replay_execution_evidence(
    const FaultControllerSchedulerPhaseControlCompatibilityValidation& compatibility,
    bool replay_hold_scope_defined,
    bool pending_event_state_defined,
    bool rollback_context_linked,
    bool resume_condition_defined,
    bool ordering_impact_reviewed,
    bool conservation_impact_reviewed,
    bool mass_audit_continuity_reviewed);
[[nodiscard]] FaultControllerForceMassAuditExecutionEvidence make_fault_controller_force_mass_audit_execution_evidence(
    const FaultControllerSchedulerPhaseControlCompatibilityValidation& compatibility,
    bool mass_audit_scope_defined,
    bool baseline_mass_state_preserved,
    bool current_mass_state_preserved,
    bool conservation_status_policy_defined,
    bool ordering_impact_reviewed,
    bool replay_continuity_reviewed);
[[nodiscard]] FaultControllerAbortSchedulerStepExecutionEvidence make_fault_controller_abort_scheduler_step_execution_evidence(
    const FaultControllerSchedulerPhaseControlCompatibilityValidation& compatibility,
    bool abort_scope_defined,
    bool rollback_context_linked,
    bool replay_policy_continuity_reviewed,
    bool mass_audit_policy_continuity_reviewed,
    bool ordering_impact_reviewed,
    bool conservation_impact_reviewed,
    bool release_gate_boundary_reviewed);
[[nodiscard]] FaultControllerSchedulerMutationAuthorization authorize_fault_controller_scheduler_mutation(
    const FaultControllerPauseBeforePhaseExecutionEvidence& evidence,
    const FaultControllerMutationAuthorizationRequest& request);
[[nodiscard]] FaultControllerSchedulerMutationAuthorization authorize_fault_controller_scheduler_mutation(
    const FaultControllerSkipTargetEngineExecutionEvidence& evidence,
    const FaultControllerMutationAuthorizationRequest& request);
[[nodiscard]] FaultControllerSchedulerMutationAuthorization authorize_fault_controller_scheduler_mutation(
    const FaultControllerHoldReplayExecutionEvidence& evidence,
    const FaultControllerMutationAuthorizationRequest& request);
[[nodiscard]] FaultControllerSchedulerMutationAuthorization authorize_fault_controller_scheduler_mutation(
    const FaultControllerForceMassAuditExecutionEvidence& evidence,
    const FaultControllerMutationAuthorizationRequest& request);
[[nodiscard]] FaultControllerSchedulerMutationAuthorization authorize_fault_controller_scheduler_mutation(
    const FaultControllerAbortSchedulerStepExecutionEvidence& evidence,
    const FaultControllerMutationAuthorizationRequest& request);
[[nodiscard]] FaultControllerSchedulerMutationExecutionAttempt make_fault_controller_scheduler_mutation_execution_attempt(
    const FaultControllerSchedulerMutationAuthorization& authorization,
    const FaultControllerSchedulerMutationExecutionAttemptRequest& request);
[[nodiscard]] FaultControllerSchedulerMutationPostconditionEvidence make_fault_controller_scheduler_mutation_postcondition_evidence(
    const FaultControllerSchedulerMutationExecutionAttempt& attempt,
    const FaultControllerSchedulerMutationPostconditionRequest& request);
[[nodiscard]] FaultControllerSchedulerMutationRuntimeEvidenceWriteRecord make_fault_controller_scheduler_mutation_runtime_evidence_write_record(
    const FaultControllerSchedulerMutationPostconditionEvidence& postcondition,
    bool runtime_evidence_sink_available,
    bool release_gate_policy_allows_write,
    std::string evidence_record_id);
[[nodiscard]] FaultControllerSchedulerMutationDryRunResult make_fault_controller_scheduler_mutation_dry_run_result(
    const FaultControllerSchedulerMutationAuthorization& authorization,
    const FaultControllerSchedulerMutationExecutionAttempt& attempt,
    const FaultControllerSchedulerMutationDryRunRequest& request);
[[nodiscard]] FaultControllerSchedulerMutationExecutionResult execute_fault_controller_scheduler_mutation(
    const FaultControllerSchedulerMutationExecutionRequest& request);
[[nodiscard]] FaultControllerSchedulerMutationCompletionRecord consume_fault_controller_scheduler_mutation_postcondition(
    const FaultControllerSchedulerMutationExecutionResult& execution_result,
    const FaultControllerSchedulerMutationPostconditionEvidence& postcondition,
    bool postcondition_present);
[[nodiscard]] FaultControllerSchedulerMutationRuntimeWriteIntent prepare_fault_controller_scheduler_mutation_runtime_write_intent(
    const FaultControllerSchedulerMutationRuntimeWriteIntentRequest& request);
[[nodiscard]] FaultControllerSchedulerReleaseGatePolicyDecision make_fault_controller_scheduler_release_gate_policy_decision(
    const FaultControllerSchedulerReleaseGatePolicyDecisionRequest& request);
[[nodiscard]] FaultControllerSchedulerRuntimeEvidenceWriteResult write_fault_controller_scheduler_runtime_evidence(
    const FaultControllerSchedulerRuntimeEvidenceWriteRequest& request);
[[nodiscard]] FaultControllerSchedulerReleaseGateActionIntent prepare_fault_controller_scheduler_release_gate_action_intent(
    const FaultControllerSchedulerReleaseGateActionIntentRequest& request);
[[nodiscard]] FaultControllerSchedulerReleaseGateActionEmissionResult emit_fault_controller_scheduler_release_gate_action(
    const FaultControllerSchedulerReleaseGateActionEmissionRequest& request);
[[nodiscard]] FaultControllerSchedulerReleaseGateActionCompletionRecord complete_fault_controller_scheduler_release_gate_action(
    const FaultControllerSchedulerReleaseGateActionEmissionResult& emission_result);

[[nodiscard]] ExchangeDecision evaluate_exchange(
    const ExchangeCellState& cell,
    const ExchangeRequest& request);

[[nodiscard]] std::vector<SharedExchangeDecision> evaluate_shared_exchange(
    const ExchangeCellState& cell,
    const std::vector<SharedExchangeIntent>& intents,
    double dt_sub);

// 1D-1D engine-interface exchange (e.g. drainage outfall -> river lateral).
// The two parallel 1D engines never see each other: the source adapter
// reports a discharge request, the target adapter reports an acceptance
// capacity, and the core issues the conservative decision both sides accept.
struct EngineInterfaceExchangeRequest {
    SharedExchangeEndpoint source{};
    SharedExchangeEndpoint target{};
    double q_request{0.0};
    double q_capacity{0.0};
    double dt_sub{0.0};
};

struct EngineInterfaceExchangeDecision {
    SharedExchangeEndpoint source{};
    SharedExchangeEndpoint target{};
    double q_granted{0.0};
    double v_granted{0.0};
    double v_unmet{0.0};
};

[[nodiscard]] EngineInterfaceExchangeDecision evaluate_engine_interface_exchange(
    const EngineInterfaceExchangeRequest& request);

// Head-driven exchange intent flow (spec section 7.5 exchange points; regimes:
// free weir, submerged weir with Villemonte correction, orifice, smoothstep
// incipient transition). Produces only the INTENT magnitude and direction
// from the two water surface elevations; Q_limit arbitration, deficit and
// conservation remain in the exchange pipeline.
struct ExchangeFlowGeometry {
    double crest_level{0.0};       // weir crest / sill elevation (m)
    double exchange_width{0.0};    // effective weir width (m), > 0
    double weir_coefficient{1.7};  // SI broad-crested default
    double orifice_coefficient{0.6};
    double orifice_area{0.0};      // > 0 enables the orifice regime when submerged
    double smooth_depth{0.0};      // smoothstep ramp depth near the crest; 0 disables
};

enum class HeadDrivenExchangeRegime {
    none,
    free_weir,
    submerged_weir,
    orifice,
};

struct HeadDrivenExchangeFlow {
    // Positive: surface drains into the engine; negative: engine spills back
    // onto the surface.
    double q_surface_to_engine{0.0};
    HeadDrivenExchangeRegime regime{HeadDrivenExchangeRegime::none};
};

[[nodiscard]] HeadDrivenExchangeFlow compute_head_driven_exchange_flow(
    double surface_water_level,
    double engine_water_level,
    const ExchangeFlowGeometry& geometry);

struct DrainSplit {
    int micro_steps{1};
    double dt_micro{0.0};
    double v_per_micro_step{0.0};
};

[[nodiscard]] DrainSplit split_drain(
    const ExchangeCellState& cell,
    const ExchangeDecision& decision,
    double dt_sub);

[[nodiscard]] ExchangeDecision enforce_nonnegative_storage(
    const ExchangeCellState& cell,
    const ExchangeDecision& decision,
    double dt_sub);

struct ExchangePipelineDecision {
    ExchangeDecision exchange{};
    DrainSplit drain_split{};
    bool drain_split_engaged{false};
    bool negative_depth_fix_engaged{false};
};

struct SharedExchangePipelineDecision {
    std::vector<SharedExchangeDecision> decisions{};
    DrainSplit drain_split{};
    bool drain_split_engaged{false};
    bool negative_depth_fix_engaged{false};
};

[[nodiscard]] ExchangePipelineDecision evaluate_exchange_pipeline(
    const ExchangeCellState& cell,
    const ExchangeRequest& request);

[[nodiscard]] SharedExchangePipelineDecision evaluate_shared_exchange_pipeline(
    const ExchangeCellState& cell,
    const std::vector<SharedExchangeIntent>& intents,
    double dt_sub);

struct ExchangeConservationAudit {
    double request_volume{0.0};
    double accounted_volume{0.0};
    double residual{0.0};
    bool balanced{true};
};

[[nodiscard]] ExchangeConservationAudit audit_exchange_decision(
    const ExchangeRequest& request,
    const ExchangePipelineDecision& decision);

struct SystemMassAudit {
    double surface_mass{0.0};
    double deficit_mass{0.0};
    double total_mass{0.0};
    std::size_t wet_cell_count{0};
};

struct SystemMassDelta {
    SystemMassAudit baseline{};
    SystemMassAudit current{};
    double residual{0.0};
    bool conserved{true};
};

enum class SystemMassConservationStatus {
    conserved,
    drifted,
};

struct SystemMassConservationDiagnostic {
    SystemMassConservationStatus status{SystemMassConservationStatus::conserved};
    double residual{0.0};
    double baseline_total_mass{0.0};
    double current_total_mass{0.0};
};

enum class SystemMassGateAction {
    continue_run,
    abort_run,
};

struct SystemMassGateDecision {
    SystemMassGateAction action{SystemMassGateAction::continue_run};
    SystemMassConservationDiagnostic diagnostic{};
};

enum class SystemMassRuntimeGateStatus {
    running,
    abort,
};

struct SystemMassRuntimeGateOutcome {
    SystemMassGateDecision decision{};
    SystemMassRuntimeGateStatus status{SystemMassRuntimeGateStatus::running};
};

[[nodiscard]] SystemMassRuntimeGateOutcome make_system_mass_runtime_gate_outcome(
    const SystemMassGateDecision& decision);

[[nodiscard]] SystemMassRuntimeGateOutcome make_system_mass_runtime_gate_outcome(
    const SystemMassConservationDiagnostic& diagnostic);

enum class SystemMassRuntimeAbortHandlingState {
    continue_run,
    abort,
};

struct SystemMassRuntimeControlDecision {
    SystemMassRuntimeGateOutcome gate_outcome{};
    SystemMassRuntimeAbortHandlingState handling_state{SystemMassRuntimeAbortHandlingState::continue_run};
    bool should_abort{false};
};

enum class SystemMassRuntimeControlState {
    continue_run,
    abort,
};

struct SystemMassRuntimeControlResult {
    SystemMassRuntimeControlDecision decision{};
    SystemMassRuntimeControlState state{SystemMassRuntimeControlState::continue_run};
};

[[nodiscard]] SystemMassRuntimeControlResult consume_system_mass_runtime_control_decision(
    const SystemMassRuntimeControlDecision& decision);

enum class SystemMassRuntimeOperatorActionState {
    continue_run,
    abort_requested,
};

struct SystemMassRuntimeOperatorAction {
    SystemMassRuntimeControlResult control_result{};
    SystemMassRuntimeOperatorActionState state{SystemMassRuntimeOperatorActionState::continue_run};
};

[[nodiscard]] SystemMassRuntimeOperatorAction make_system_mass_runtime_operator_action(
    const SystemMassRuntimeControlResult& control_result);

[[nodiscard]] SystemMassRuntimeAbortHandlingState classify_system_mass_runtime_abort_handling(
    const SystemMassRuntimeGateOutcome& outcome);

[[nodiscard]] bool should_abort_system_mass_runtime(
    SystemMassRuntimeAbortHandlingState handling_state);

[[nodiscard]] bool should_abort_system_mass_runtime(
    const SystemMassConservationDiagnostic& diagnostic);

[[nodiscard]] SystemMassRuntimeControlDecision make_system_mass_runtime_control_decision(
    const SystemMassRuntimeGateOutcome& gate_outcome);

[[nodiscard]] SystemMassRuntimeControlDecision make_system_mass_runtime_control_decision(
    const SystemMassConservationDiagnostic& diagnostic);

[[nodiscard]] SystemMassAudit compute_system_mass(
    const std::vector<ExchangeCellState>& cells,
    double h_wet);

[[nodiscard]] SystemMassDelta audit_system_mass_against_reference(
    const SystemMassAudit& baseline,
    const std::vector<ExchangeCellState>& current_cells,
    double h_wet);

[[nodiscard]] SystemMassConservationStatus classify_system_mass_conservation(
    const SystemMassDelta& delta);

[[nodiscard]] SystemMassConservationDiagnostic make_system_mass_conservation_diagnostic(
    const SystemMassDelta& delta);

[[nodiscard]] SystemMassGateDecision decide_system_mass_gate_action(
    const SystemMassConservationDiagnostic& diagnostic);

[[nodiscard]] MassDeficitAccount roll_deficit(const MassDeficitAccount& account, double unmet_volume);
[[nodiscard]] MassDeficitAccount apply_repayment(const MassDeficitAccount& account, double applied_volume);

enum class ExchangeDirection {
    surface_to_engine,
    engine_to_surface,
};

struct SharedExchangeEndpointEvent {
    SharedExchangeEndpoint endpoint{};
    double unmet_volume{0.0};
    double repayment_volume{0.0};
};

struct CouplingEvent {
    std::size_t exchange_cell_index{0U};
    ExchangeDirection direction{ExchangeDirection::surface_to_engine};
    double volume_delta{0.0};
    double unmet_volume{0.0};
    double repayment_volume{0.0};
    std::vector<SharedExchangeEndpointEvent> shared_endpoint_events{};
};

struct RuntimeCounters {
    std::size_t count_drain_split{0};
    std::size_t count_negative_depth_fix{0};
};

// 1D -> 2D return flow (engine_to_surface): manhole surcharge/overflow or
// river spill returning water onto the 2D surface through the event queue.
struct ReturnExchangeRequest {
    SharedExchangeEndpoint source{};
    double q_return{0.0};
    double dt_sub{0.0};
};

struct ReturnExchangeDecision {
    SharedExchangeEndpoint source{};
    double q_returned{0.0};
    double v_returned{0.0};
};

class CouplingSnapshot {
public:
    [[nodiscard]] const std::vector<ExchangeCellState>& cells() const noexcept;
    [[nodiscard]] const RuntimeCounters& runtime_counters() const noexcept;
    [[nodiscard]] const std::vector<CouplingEvent>& pending_events() const noexcept;
    [[nodiscard]] SystemMassAudit compute_system_mass(double h_wet) const;

private:
    friend class CouplingState;

    CouplingSnapshot(
        std::vector<ExchangeCellState> cells,
        RuntimeCounters counters,
        std::vector<CouplingEvent> pending_events);

    std::vector<ExchangeCellState> cells_;
    RuntimeCounters runtime_counters_;
    std::vector<CouplingEvent> pending_events_;
};

class CouplingState {
public:
    explicit CouplingState(std::vector<ExchangeCellState> cells);

    [[nodiscard]] const std::vector<ExchangeCellState>& cells() const noexcept;
    [[nodiscard]] const RuntimeCounters& runtime_counters() const noexcept;
    [[nodiscard]] CouplingSnapshot snapshot() const;
    [[nodiscard]] SystemMassAudit compute_system_mass(double h_wet) const;
    [[nodiscard]] SystemMassDelta audit_system_mass_against_reference(
        const SystemMassAudit& baseline,
        double h_wet) const;
    [[nodiscard]] SystemMassConservationDiagnostic diagnose_system_mass_against_reference(
        const SystemMassAudit& baseline,
        double h_wet) const;
    [[nodiscard]] SystemMassGateDecision decide_system_mass_gate_action_against_reference(
        const SystemMassAudit& baseline,
        double h_wet) const;
    [[nodiscard]] SystemMassRuntimeGateOutcome evaluate_system_mass_runtime_gate_against_reference(
        const SystemMassAudit& baseline,
        double h_wet) const;
    [[nodiscard]] SystemMassRuntimeControlDecision decide_system_mass_runtime_control_against_reference(
        const SystemMassAudit& baseline,
        double h_wet) const;
    [[nodiscard]] bool should_abort_system_mass_runtime_control_against_reference(
        const SystemMassAudit& baseline,
        double h_wet) const;
    [[nodiscard]] bool should_abort_system_mass_runtime_against_reference(
        const SystemMassAudit& baseline,
        double h_wet) const;
    [[nodiscard]] SystemMassDelta audit_system_mass_against_snapshot(
        const CouplingSnapshot& baseline,
        double h_wet) const;
    [[nodiscard]] SystemMassConservationDiagnostic diagnose_system_mass_against_snapshot(
        const CouplingSnapshot& baseline,
        double h_wet) const;
    [[nodiscard]] SystemMassGateDecision decide_system_mass_gate_action_against_snapshot(
        const CouplingSnapshot& baseline,
        double h_wet) const;
    [[nodiscard]] SystemMassRuntimeGateOutcome evaluate_system_mass_runtime_gate_against_snapshot(
        const CouplingSnapshot& baseline,
        double h_wet) const;
    [[nodiscard]] bool should_abort_system_mass_runtime_against_snapshot(
        const CouplingSnapshot& baseline,
        double h_wet) const;
    [[nodiscard]] SystemMassRuntimeControlDecision decide_system_mass_runtime_control_against_snapshot(
        const CouplingSnapshot& baseline,
        double h_wet) const;
    [[nodiscard]] bool should_abort_system_mass_runtime_control_against_snapshot(
        const CouplingSnapshot& baseline,
        double h_wet) const;

    void enqueue_event(CouplingEvent event);
    void rollback(const CouplingSnapshot& snapshot);
    void replay_pending();
    void record_pipeline_decision(const ExchangePipelineDecision& decision);
    ExchangePipelineDecision apply_exchange(std::size_t cell_index, const ExchangeRequest& request);
    ReturnExchangeDecision apply_return_exchange(
        std::size_t cell_index,
        const ReturnExchangeRequest& request);
    std::vector<SharedExchangeDecision> apply_shared_exchange(
        std::size_t cell_index,
        const std::vector<SharedExchangeIntent>& intents,
        double dt_sub);
    MockCouplingSchedulerStepResult run_mock_coupling_scheduler_step(
        std::size_t cell_index,
        const std::vector<SharedExchangeIntent>& intents,
        double dt_sub);

private:
    std::vector<ExchangeCellState> cells_;
    RuntimeCounters runtime_counters_{};
    std::vector<CouplingEvent> pending_events_;
};

}  // namespace scau::coupling::core

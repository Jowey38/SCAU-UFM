#include "coupling/core/state.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace scau::coupling::core {
namespace {

bool same_endpoint(
    const SharedExchangeEndpoint& lhs,
    const SharedExchangeEndpoint& rhs) noexcept {
    return lhs.engine == rhs.engine && lhs.node_id == rhs.node_id;
}

MassDeficitAccount endpoint_deficit_account(
    const ExchangeCellState& cell,
    const SharedExchangeEndpoint& endpoint) {
    for (const auto& shared_deficit : cell.shared_deficit_accounts) {
        if (same_endpoint(shared_deficit.endpoint, endpoint)) {
            return shared_deficit.mass_deficit_account;
        }
    }
    return {};
}

void upsert_endpoint_deficit_account(
    ExchangeCellState& cell,
    const SharedExchangeEndpoint& endpoint,
    const MassDeficitAccount& account) {
    for (auto& shared_deficit : cell.shared_deficit_accounts) {
        if (same_endpoint(shared_deficit.endpoint, endpoint)) {
            shared_deficit.mass_deficit_account = account;
            return;
        }
    }
    cell.shared_deficit_accounts.push_back({
        .endpoint = endpoint,
        .mass_deficit_account = account,
    });
}

void validate_exchange_cell_state(const ExchangeCellState& cell) {
    if (!std::isfinite(cell.volume)) {
        throw std::invalid_argument("cell volume must be finite");
    }
    if (!std::isfinite(cell.phi_t) || cell.phi_t < 0.0) {
        throw std::invalid_argument("phi_t must be finite and non-negative");
    }
    if (!std::isfinite(cell.h) || cell.h < 0.0) {
        throw std::invalid_argument("h must be finite and non-negative");
    }
    if (!std::isfinite(cell.area) || cell.area < 0.0) {
        throw std::invalid_argument("area must be finite and non-negative");
    }
    if (!std::isfinite(cell.mass_deficit_account.volume) || cell.mass_deficit_account.volume < 0.0) {
        throw std::invalid_argument("deficit volume must be finite and non-negative");
    }
    if (cell.mass_deficit_account.volume > 0.0 && !cell.shared_deficit_accounts.empty()) {
        throw std::invalid_argument("aggregate and shared endpoint deficits must not be mixed");
    }
    for (std::size_t i = 0; i < cell.shared_deficit_accounts.size(); ++i) {
        const auto& shared_deficit = cell.shared_deficit_accounts[i];
        if (!std::isfinite(shared_deficit.mass_deficit_account.volume) ||
            shared_deficit.mass_deficit_account.volume < 0.0) {
            throw std::invalid_argument("shared deficit volume must be finite and non-negative");
        }
        for (std::size_t j = i + 1; j < cell.shared_deficit_accounts.size(); ++j) {
            if (same_endpoint(shared_deficit.endpoint, cell.shared_deficit_accounts[j].endpoint)) {
                throw std::invalid_argument("shared deficit account endpoints must be unique");
            }
        }
    }
}

void validate_exchange_decision(const ExchangeDecision& decision) {
    if (!std::isfinite(decision.q_granted) || decision.q_granted < 0.0) {
        throw std::invalid_argument("q_granted must be finite and non-negative");
    }
    if (!std::isfinite(decision.v_granted) || decision.v_granted < 0.0) {
        throw std::invalid_argument("v_granted must be finite and non-negative");
    }
    if (!std::isfinite(decision.q_repay) || decision.q_repay < 0.0) {
        throw std::invalid_argument("q_repay must be finite and non-negative");
    }
    if (!std::isfinite(decision.v_repay) || decision.v_repay < 0.0) {
        throw std::invalid_argument("v_repay must be finite and non-negative");
    }
    if (!std::isfinite(decision.v_unmet) || decision.v_unmet < 0.0) {
        throw std::invalid_argument("v_unmet must be finite and non-negative");
    }
}

void apply_volume_delta_to_cell(
    ExchangeCellState& cell,
    ExchangeDirection direction,
    double volume_delta) {
    validate_exchange_cell_state(cell);
    if (!std::isfinite(volume_delta)) {
        throw std::invalid_argument("volume_delta must be finite");
    }

    const double signed_volume_delta = direction == ExchangeDirection::surface_to_engine
        ? -volume_delta
        : volume_delta;
    const double updated_volume = cell.volume + signed_volume_delta;
    if (!std::isfinite(updated_volume) || updated_volume < 0.0) {
        throw std::invalid_argument("replayed volume would produce invalid cell volume");
    }

    double updated_h = cell.h;
    const double storage_area = cell.phi_t * cell.area;
    if (storage_area > 0.0) {
        updated_h = cell.h + signed_volume_delta / storage_area;
        if (!std::isfinite(updated_h) || updated_h < 0.0) {
            throw std::invalid_argument("replayed volume would produce invalid water depth");
        }
    } else if (volume_delta != 0.0) {
        throw std::invalid_argument("zero-storage cell cannot replay nonzero volume delta");
    }

    cell.volume = updated_volume;
    cell.h = updated_h;
}

}  // namespace

EngineHealthDiagnostic classify_engine_health(const EngineReport& report) {
    if (!std::isfinite(report.elapsed_time) || report.elapsed_time < 0.0) {
        throw std::invalid_argument("engine report elapsed_time must be finite and non-negative");
    }
    return EngineHealthDiagnostic{
        .status = report.healthy ? EngineHealthStatus::healthy : EngineHealthStatus::unhealthy,
        .engine_id = report.engine_id,
        .error_code = report.error_code,
        .elapsed_time = report.elapsed_time,
    };
}

EngineHealthAggregate aggregate_engine_health(
    const std::vector<EngineHealthDiagnostic>& diagnostics) {
    EngineHealthAggregate aggregate{
        .report_count = diagnostics.size(),
    };
    for (const auto& diagnostic : diagnostics) {
        if (diagnostic.status == EngineHealthStatus::unhealthy) {
            ++aggregate.unhealthy_count;
        }
    }
    aggregate.status = aggregate.unhealthy_count == 0U
        ? EngineHealthAggregateStatus::all_healthy
        : EngineHealthAggregateStatus::any_unhealthy;
    return aggregate;
}

FaultControllerDiagnostic make_fault_controller_diagnostic(
    const EngineHealthAggregate& health) {
    return FaultControllerDiagnostic{
        .status = health.status == EngineHealthAggregateStatus::all_healthy
            ? FaultControllerDiagnosticStatus::nominal
            : FaultControllerDiagnosticStatus::fault_detected,
        .health = health,
        .should_isolate = false,
        .should_reconnect = false,
        .should_abort = false,
    };
}

FaultControllerProposedAction propose_fault_controller_action(
    const FaultControllerDiagnostic& diagnostic) {
    return FaultControllerProposedAction{
        .diagnostic = diagnostic,
        .state = diagnostic.status == FaultControllerDiagnosticStatus::nominal
            ? FaultControllerProposedActionState::continue_run
            : FaultControllerProposedActionState::review_required,
        .execute_isolation = false,
        .execute_reconnect = false,
        .execute_abort = false,
    };
}

MockCouplingSchedulerFaultObservation observe_mock_scheduler_fault_action(
    const FaultControllerProposedAction& action) {
    return MockCouplingSchedulerFaultObservation{
        .action = action,
        .observed_state = action.state,
        .observed_review_required = action.state == FaultControllerProposedActionState::review_required,
        .executed_isolation = false,
        .executed_reconnect = false,
        .executed_abort = false,
    };
}

MockCouplingSchedulerFaultConsumption consume_mock_scheduler_fault_action(
    const MockCouplingSchedulerFaultObservation& observation) {
    return MockCouplingSchedulerFaultConsumption{
        .observation = observation,
        .consumed_state = observation.observed_state,
        .review_required_consumed = observation.observed_state == FaultControllerProposedActionState::review_required,
        .exchange_scheduling_allowed = true,
        .replay_allowed = true,
        .audit_allowed = true,
        .executed_isolation = false,
        .executed_reconnect = false,
        .executed_abort = false,
    };
}

FaultControllerPassiveStateClassification classify_fault_controller_passive_state(
    const MockCouplingSchedulerFaultConsumption& consumption) {
    return FaultControllerPassiveStateClassification{
        .consumption = consumption,
        .state = consumption.consumed_state == FaultControllerProposedActionState::review_required
            ? FaultControllerPassiveStateLabel::review_required
            : FaultControllerPassiveStateLabel::running,
        .scheduler_control_enabled = false,
        .isolation_enabled = false,
        .reconnect_enabled = false,
        .abort_enabled = false,
    };
}

FaultControllerPassiveTransition make_fault_controller_passive_transition(
    FaultControllerPassiveStateLabel previous_state,
    const FaultControllerPassiveStateClassification& classification) {
    return FaultControllerPassiveTransition{
        .classification = classification,
        .previous_state = previous_state,
        .next_state = classification.state,
        .reason = classification.state == FaultControllerPassiveStateLabel::review_required
            ? FaultControllerPassiveTransitionReason::fault_detected_review_required
            : FaultControllerPassiveTransitionReason::nominal_health,
        .isolation_requested = false,
        .reconnect_requested = false,
        .abort_requested = false,
        .runtime_action_executed = false,
        .scheduler_control_enabled = false,
    };
}

FaultControllerPassiveActionAuditRecord make_fault_controller_passive_action_audit_record(
    const FaultControllerPassiveTransition& transition) {
    return FaultControllerPassiveActionAuditRecord{
        .transition = transition,
        .action_kind = transition.next_state == FaultControllerPassiveStateLabel::review_required
            ? FaultControllerPassiveActionAuditKind::operator_review
            : FaultControllerPassiveActionAuditKind::none,
        .action_stage = FaultControllerPassiveActionAuditStage::transition_recorded,
        .reason = transition.next_state == FaultControllerPassiveStateLabel::review_required
            ? FaultControllerPassiveActionAuditReason::review_required_only
            : FaultControllerPassiveActionAuditReason::nominal_no_action,
        .scheduler_control_enabled = false,
        .runtime_action_requested = false,
        .runtime_action_executed = false,
        .isolation_executed = false,
        .reconnect_executed = false,
        .abort_executed = false,
        .adapter_call_executed = false,
        .release_gate_action_executed = false,
    };
}

FaultControllerPassiveActionOutcome make_fault_controller_passive_action_outcome(
    const FaultControllerPassiveActionAuditRecord& audit) {
    const bool review_required = audit.action_kind == FaultControllerPassiveActionAuditKind::operator_review;
    return FaultControllerPassiveActionOutcome{
        .audit = audit,
        .outcome_kind = review_required
            ? FaultControllerPassiveActionOutcomeKind::operator_review_required
            : FaultControllerPassiveActionOutcomeKind::not_requested,
        .reason = review_required
            ? FaultControllerPassiveActionOutcomeReason::review_required_only
            : FaultControllerPassiveActionOutcomeReason::nominal_no_action,
        .scheduler_control_available = false,
        .scheduler_control_used = false,
        .adapter_boundary_available = false,
        .adapter_call_attempted = false,
        .adapter_call_succeeded = false,
        .adapter_call_failed = false,
        .runtime_action_requested = false,
        .runtime_action_attempted = false,
        .runtime_action_executed = false,
        .runtime_action_skipped = false,
        .runtime_action_failed = false,
        .operator_review_required = review_required,
        .rollback_required = false,
        .replay_required = false,
        .mass_audit_required = false,
        .release_gate_action_executed = false,
    };
}

FaultControllerBlockedAction make_fault_controller_blocked_action(
    const FaultControllerPassiveActionOutcome& outcome) {
    return FaultControllerBlockedAction{
        .outcome = outcome,
        .reason = outcome.operator_review_required
            ? FaultControllerBlockedActionReason::operator_review_required
            : FaultControllerBlockedActionReason::no_action_requested,
        .execution_allowed = false,
        .scheduler_control_allowed = false,
        .adapter_call_allowed = false,
        .isolation_allowed = false,
        .reconnect_allowed = false,
        .abort_allowed = false,
        .release_gate_action_allowed = false,
    };
}

FaultControllerSchedulerControlRequest make_fault_controller_scheduler_control_request(
    const FaultControllerBlockedAction& blocked_action) {
    const bool control_requested = blocked_action.reason == FaultControllerBlockedActionReason::operator_review_required;
    return FaultControllerSchedulerControlRequest{
        .outcome = blocked_action.outcome,
        .blocked_action = blocked_action,
        .requested_kind = control_requested
            ? FaultControllerSchedulerControlKind::observe_only
            : FaultControllerSchedulerControlKind::none,
        .status = control_requested
            ? FaultControllerSchedulerControlStatus::blocked_boundary_absent
            : FaultControllerSchedulerControlStatus::not_requested,
        .reason = control_requested
            ? FaultControllerSchedulerControlReason::scheduler_control_boundary_absent
            : FaultControllerSchedulerControlReason::no_scheduler_control_requested,
        .target_engine_id = blocked_action.outcome.audit.transition.classification.consumption.observation.action.diagnostic.health.unhealthy_count > 0U
            ? "fault_controller_review"
            : "",
        .scheduler_control_boundary_available = false,
        .threshold_evidence_complete = false,
        .operator_approved = false,
        .rollback_context_available = false,
        .replay_policy_available = false,
        .mass_audit_policy_available = false,
        .scheduler_control_allowed = false,
        .scheduler_control_used = false,
        .adapter_call_allowed = false,
        .exchange_requests_paused = false,
        .target_engine_request_skipped = false,
        .replay_held = false,
        .mass_audit_forced = false,
        .scheduler_state_mutated = false,
        .release_gate_action_executed = false,
    };
}

FaultControllerSchedulerControlResult make_fault_controller_scheduler_control_result(
    const FaultControllerSchedulerControlRequest& request) {
    const bool control_requested = request.requested_kind != FaultControllerSchedulerControlKind::none;
    return FaultControllerSchedulerControlResult{
        .request = request,
        .attempted_kind = request.requested_kind,
        .status = control_requested
            ? FaultControllerSchedulerControlResultStatus::blocked_boundary_absent
            : FaultControllerSchedulerControlResultStatus::not_requested,
        .reason = control_requested
            ? FaultControllerSchedulerControlResultReason::scheduler_control_boundary_absent
            : FaultControllerSchedulerControlResultReason::no_scheduler_control_requested,
        .phase_before_control = "passive_scheduler_control_request_recorded",
        .phase_after_control = "passive_scheduler_control_result_recorded",
        .target_engine_id = request.target_engine_id,
        .scheduler_control_boundary_available = request.scheduler_control_boundary_available,
        .threshold_evidence_complete = request.threshold_evidence_complete,
        .operator_approved = request.operator_approved,
        .rollback_context_available = request.rollback_context_available,
        .replay_policy_available = request.replay_policy_available,
        .mass_audit_policy_available = request.mass_audit_policy_available,
        .scheduler_control_used = false,
        .exchange_requests_paused = false,
        .target_engine_request_skipped = false,
        .replay_held = false,
        .mass_audit_forced = false,
        .scheduler_state_mutated = false,
        .adapter_call_attempted = false,
        .adapter_call_succeeded = false,
        .adapter_call_failed = false,
        .rollback_required = false,
        .replay_required = false,
        .mass_audit_required = false,
        .operator_review_required = request.outcome.operator_review_required,
        .release_gate_action_executed = false,
    };
}

FaultControllerSchedulerPhaseDescriptor describe_fault_controller_scheduler_phase(
    FaultControllerSchedulerPhase phase) {
    auto descriptor = FaultControllerSchedulerPhaseDescriptor{.phase = phase};
    switch (phase) {
    case FaultControllerSchedulerPhase::health_collection:
        descriptor.phase_name = "health_collection";
        break;
    case FaultControllerSchedulerPhase::health_classification:
        descriptor.phase_name = "health_classification";
        break;
    case FaultControllerSchedulerPhase::health_aggregation:
        descriptor.phase_name = "health_aggregation";
        break;
    case FaultControllerSchedulerPhase::fault_diagnostic_derivation:
        descriptor.phase_name = "fault_diagnostic_derivation";
        break;
    case FaultControllerSchedulerPhase::proposed_action_derivation:
        descriptor.phase_name = "proposed_action_derivation";
        break;
    case FaultControllerSchedulerPhase::fault_action_observation:
        descriptor.phase_name = "fault_action_observation";
        break;
    case FaultControllerSchedulerPhase::fault_action_consumption:
        descriptor.phase_name = "fault_action_consumption";
        break;
    case FaultControllerSchedulerPhase::passive_state_classification:
        descriptor.phase_name = "passive_state_classification";
        break;
    case FaultControllerSchedulerPhase::passive_transition_creation:
        descriptor.phase_name = "passive_transition_creation";
        break;
    case FaultControllerSchedulerPhase::passive_action_audit_creation:
        descriptor.phase_name = "passive_action_audit_creation";
        break;
    case FaultControllerSchedulerPhase::passive_action_outcome_creation:
        descriptor.phase_name = "passive_action_outcome_creation";
        break;
    case FaultControllerSchedulerPhase::blocked_action_creation:
        descriptor.phase_name = "blocked_action_creation";
        break;
    case FaultControllerSchedulerPhase::scheduler_control_request_creation:
        descriptor.phase_name = "scheduler_control_request_creation";
        break;
    case FaultControllerSchedulerPhase::scheduler_control_result_creation:
        descriptor.phase_name = "scheduler_control_result_creation";
        break;
    case FaultControllerSchedulerPhase::exchange_request_preparation:
        descriptor.phase_name = "exchange_request_preparation";
        descriptor.category = FaultControllerSchedulerPhaseCategory::scheduling_conservation;
        descriptor.category_name = "scheduling_conservation";
        break;
    case FaultControllerSchedulerPhase::shared_cell_arbitration:
        descriptor.phase_name = "shared_cell_arbitration";
        descriptor.category = FaultControllerSchedulerPhaseCategory::scheduling_conservation;
        descriptor.category_name = "scheduling_conservation";
        break;
    case FaultControllerSchedulerPhase::exchange_acceptance:
        descriptor.phase_name = "exchange_acceptance";
        descriptor.category = FaultControllerSchedulerPhaseCategory::scheduling_conservation;
        descriptor.category_name = "scheduling_conservation";
        break;
    case FaultControllerSchedulerPhase::replay:
        descriptor.phase_name = "replay";
        descriptor.category = FaultControllerSchedulerPhaseCategory::scheduling_conservation;
        descriptor.category_name = "scheduling_conservation";
        break;
    case FaultControllerSchedulerPhase::mass_audit:
        descriptor.phase_name = "mass_audit";
        descriptor.category = FaultControllerSchedulerPhaseCategory::scheduling_conservation;
        descriptor.category_name = "scheduling_conservation";
        break;
    case FaultControllerSchedulerPhase::post_step_reporting:
        descriptor.phase_name = "post_step_reporting";
        descriptor.category = FaultControllerSchedulerPhaseCategory::scheduling_conservation;
        descriptor.category_name = "scheduling_conservation";
        break;
    }
    return descriptor;
}

FaultControllerSchedulerEvidenceCompleteness make_fault_controller_scheduler_evidence_completeness(
    const FaultControllerSchedulerPhaseDescriptor& phase,
    const FaultControllerSchedulerControlResult& control_result) {
    auto reason = FaultControllerSchedulerEvidenceCompletenessReason::no_scheduler_control_requested;
    if (control_result.attempted_kind != FaultControllerSchedulerControlKind::none) {
        if (!control_result.scheduler_control_boundary_available) {
            reason = FaultControllerSchedulerEvidenceCompletenessReason::scheduler_control_boundary_absent;
        } else if (!control_result.threshold_evidence_complete) {
            reason = FaultControllerSchedulerEvidenceCompletenessReason::threshold_evidence_missing;
        } else if (control_result.operator_review_required && !control_result.operator_approved) {
            reason = FaultControllerSchedulerEvidenceCompletenessReason::operator_review_pending;
        } else if (!control_result.rollback_context_available) {
            reason = FaultControllerSchedulerEvidenceCompletenessReason::rollback_context_missing;
        } else if (!control_result.replay_policy_available) {
            reason = FaultControllerSchedulerEvidenceCompletenessReason::replay_policy_missing;
        } else if (!control_result.mass_audit_policy_available) {
            reason = FaultControllerSchedulerEvidenceCompletenessReason::mass_audit_policy_missing;
        }
    }

    return FaultControllerSchedulerEvidenceCompleteness{
        .phase = phase,
        .control_result = control_result,
        .status = FaultControllerSchedulerEvidenceCompletenessStatus::incomplete,
        .reason = reason,
        .passive_provenance_complete = false,
        .scheduler_control_boundary_available = control_result.scheduler_control_boundary_available,
        .threshold_evidence_complete = control_result.threshold_evidence_complete,
        .operator_approved = control_result.operator_approved,
        .rollback_context_available = control_result.rollback_context_available,
        .replay_policy_available = control_result.replay_policy_available,
        .mass_audit_policy_available = control_result.mass_audit_policy_available,
        .scheduler_control_allowed = false,
        .scheduler_control_used = false,
        .exchange_requests_paused = false,
        .target_engine_request_skipped = false,
        .replay_held = false,
        .mass_audit_forced = false,
        .scheduler_state_mutated = false,
        .adapter_call_attempted = false,
        .release_gate_action_executed = false,
    };
}

FaultControllerSchedulerTargetValidation make_fault_controller_scheduler_target_validation(
    const FaultControllerSchedulerEvidenceCompleteness& evidence,
    std::string target_engine_id,
    bool target_engine_schedulable,
    FaultControllerSchedulerConservationImpact conservation_impact,
    bool contradictory_flags) {
    const bool control_requested = evidence.control_result.attempted_kind != FaultControllerSchedulerControlKind::none;
    auto reason = FaultControllerSchedulerTargetValidationReason::no_scheduler_control_requested;
    if (control_requested) {
        if (evidence.phase.phase_name.empty()) {
            reason = FaultControllerSchedulerTargetValidationReason::target_phase_missing;
        } else if (target_engine_id.empty()) {
            reason = FaultControllerSchedulerTargetValidationReason::target_engine_missing;
        } else if (!target_engine_schedulable) {
            reason = FaultControllerSchedulerTargetValidationReason::target_engine_not_schedulable;
        } else if (conservation_impact == FaultControllerSchedulerConservationImpact::unknown) {
            reason = FaultControllerSchedulerTargetValidationReason::conservation_impact_unknown;
        } else if (contradictory_flags) {
            reason = FaultControllerSchedulerTargetValidationReason::contradictory_action_flags;
        }
    }

    const bool target_engine_specified = !target_engine_id.empty();
    return FaultControllerSchedulerTargetValidation{
        .evidence = evidence,
        .target_phase = evidence.phase,
        .status = FaultControllerSchedulerTargetValidationStatus::invalid,
        .reason = reason,
        .target_engine_id = std::move(target_engine_id),
        .target_phase_specified = !evidence.phase.phase_name.empty(),
        .target_engine_specified = target_engine_specified,
        .target_engine_schedulable = target_engine_schedulable,
        .conservation_impact = conservation_impact,
        .contradictory_flags = contradictory_flags,
        .scheduler_control_allowed = false,
        .scheduler_control_used = false,
        .exchange_requests_paused = false,
        .target_engine_request_skipped = false,
        .replay_held = false,
        .mass_audit_forced = false,
        .scheduler_state_mutated = false,
        .adapter_call_attempted = false,
        .release_gate_action_executed = false,
    };
}

FaultControllerSchedulerOrderingEvidenceValidation make_fault_controller_scheduler_ordering_evidence_validation(
    const FaultControllerSchedulerTargetValidation& target_validation,
    bool ordering_evidence_present) {
    const bool control_requested = target_validation.evidence.control_result.attempted_kind != FaultControllerSchedulerControlKind::none;
    const bool ordering_evidence_expected = control_requested
        && target_validation.target_phase.category == FaultControllerSchedulerPhaseCategory::scheduling_conservation;

    auto reason = FaultControllerSchedulerOrderingEvidenceReason::no_scheduler_control_requested;
    if (control_requested && !ordering_evidence_expected) {
        reason = FaultControllerSchedulerOrderingEvidenceReason::ordering_evidence_not_required;
    } else if (ordering_evidence_expected) {
        reason = ordering_evidence_present
            ? FaultControllerSchedulerOrderingEvidenceReason::ordering_evidence_present
            : FaultControllerSchedulerOrderingEvidenceReason::ordering_evidence_missing;
    }

    return FaultControllerSchedulerOrderingEvidenceValidation{
        .target_validation = target_validation,
        .status = FaultControllerSchedulerOrderingEvidenceStatus::invalid,
        .reason = reason,
        .ordering_evidence_expected = ordering_evidence_expected,
        .ordering_evidence_present = ordering_evidence_present,
        .scheduler_control_allowed = false,
        .scheduler_control_used = false,
        .exchange_requests_paused = false,
        .target_engine_request_skipped = false,
        .replay_held = false,
        .mass_audit_forced = false,
        .scheduler_state_mutated = false,
        .adapter_call_attempted = false,
        .release_gate_action_executed = false,
    };
}

FaultControllerSchedulerThresholdEvidenceValidation make_fault_controller_scheduler_threshold_evidence_validation(
    const FaultControllerSchedulerOrderingEvidenceValidation& ordering_evidence,
    bool threshold_evidence_present) {
    const bool control_requested = ordering_evidence.target_validation.evidence.control_result.attempted_kind != FaultControllerSchedulerControlKind::none;
    auto reason = FaultControllerSchedulerThresholdEvidenceReason::no_scheduler_control_requested;
    if (control_requested) {
        reason = threshold_evidence_present
            ? FaultControllerSchedulerThresholdEvidenceReason::threshold_evidence_present
            : FaultControllerSchedulerThresholdEvidenceReason::threshold_evidence_missing;
    }

    return FaultControllerSchedulerThresholdEvidenceValidation{
        .ordering_evidence = ordering_evidence,
        .status = FaultControllerSchedulerThresholdEvidenceStatus::invalid,
        .reason = reason,
        .threshold_evidence_expected = control_requested,
        .threshold_evidence_present = threshold_evidence_present,
        .scheduler_control_allowed = false,
        .scheduler_control_used = false,
        .exchange_requests_paused = false,
        .target_engine_request_skipped = false,
        .replay_held = false,
        .mass_audit_forced = false,
        .scheduler_state_mutated = false,
        .adapter_call_attempted = false,
        .release_gate_action_executed = false,
    };
}

FaultControllerSchedulerOperatorApprovalValidation make_fault_controller_scheduler_operator_approval_validation(
    const FaultControllerSchedulerThresholdEvidenceValidation& threshold_evidence,
    bool operator_approval_present) {
    const bool control_requested = threshold_evidence.ordering_evidence.target_validation.evidence.control_result.attempted_kind != FaultControllerSchedulerControlKind::none;
    const bool operator_approval_expected = control_requested
        && threshold_evidence.ordering_evidence.target_validation.evidence.control_result.operator_review_required;

    auto reason = FaultControllerSchedulerOperatorApprovalReason::no_scheduler_control_requested;
    if (control_requested && !operator_approval_expected) {
        reason = FaultControllerSchedulerOperatorApprovalReason::operator_approval_not_required;
    } else if (operator_approval_expected) {
        reason = operator_approval_present
            ? FaultControllerSchedulerOperatorApprovalReason::operator_approval_present
            : FaultControllerSchedulerOperatorApprovalReason::operator_approval_pending;
    }

    return FaultControllerSchedulerOperatorApprovalValidation{
        .threshold_evidence_status = threshold_evidence.status,
        .threshold_evidence_reason = threshold_evidence.reason,
        .status = FaultControllerSchedulerOperatorApprovalStatus::invalid,
        .reason = reason,
        .threshold_evidence_expected = threshold_evidence.threshold_evidence_expected,
        .threshold_evidence_present = threshold_evidence.threshold_evidence_present,
        .operator_review_required = threshold_evidence.ordering_evidence.target_validation.evidence.control_result.operator_review_required,
        .operator_approval_expected = operator_approval_expected,
        .operator_approval_present = operator_approval_present,
        .scheduler_control_allowed = false,
        .scheduler_control_used = false,
        .exchange_requests_paused = false,
        .target_engine_request_skipped = false,
        .replay_held = false,
        .mass_audit_forced = false,
        .scheduler_state_mutated = false,
        .adapter_call_attempted = false,
        .release_gate_action_executed = false,
    };
}

FaultControllerSchedulerRollbackContextValidation make_fault_controller_scheduler_rollback_context_validation(
    const FaultControllerSchedulerOperatorApprovalValidation& operator_approval,
    bool rollback_context_present) {
    const bool control_requested = operator_approval.reason != FaultControllerSchedulerOperatorApprovalReason::no_scheduler_control_requested;
    auto reason = FaultControllerSchedulerRollbackContextReason::no_scheduler_control_requested;
    if (control_requested) {
        reason = rollback_context_present
            ? FaultControllerSchedulerRollbackContextReason::rollback_context_present
            : FaultControllerSchedulerRollbackContextReason::rollback_context_missing;
    }

    return FaultControllerSchedulerRollbackContextValidation{
        .operator_approval_status = operator_approval.status,
        .operator_approval_reason = operator_approval.reason,
        .status = FaultControllerSchedulerRollbackContextStatus::invalid,
        .reason = reason,
        .operator_approval_expected = operator_approval.operator_approval_expected,
        .operator_approval_present = operator_approval.operator_approval_present,
        .rollback_context_expected = control_requested,
        .rollback_context_present = rollback_context_present,
        .scheduler_control_allowed = false,
        .scheduler_control_used = false,
        .exchange_requests_paused = false,
        .target_engine_request_skipped = false,
        .replay_held = false,
        .mass_audit_forced = false,
        .scheduler_state_mutated = false,
        .adapter_call_attempted = false,
        .release_gate_action_executed = false,
    };
}

FaultControllerSchedulerReplayPolicyValidation make_fault_controller_scheduler_replay_policy_validation(
    const FaultControllerSchedulerRollbackContextValidation& rollback_context,
    bool replay_policy_present) {
    const bool control_requested = rollback_context.reason != FaultControllerSchedulerRollbackContextReason::no_scheduler_control_requested;
    auto reason = FaultControllerSchedulerReplayPolicyReason::no_scheduler_control_requested;
    if (control_requested) {
        reason = replay_policy_present
            ? FaultControllerSchedulerReplayPolicyReason::replay_policy_present
            : FaultControllerSchedulerReplayPolicyReason::replay_policy_missing;
    }

    return FaultControllerSchedulerReplayPolicyValidation{
        .rollback_context_status = rollback_context.status,
        .rollback_context_reason = rollback_context.reason,
        .status = FaultControllerSchedulerReplayPolicyStatus::invalid,
        .reason = reason,
        .rollback_context_expected = rollback_context.rollback_context_expected,
        .rollback_context_present = rollback_context.rollback_context_present,
        .replay_policy_expected = control_requested,
        .replay_policy_present = replay_policy_present,
        .scheduler_control_allowed = false,
        .scheduler_control_used = false,
        .exchange_requests_paused = false,
        .target_engine_request_skipped = false,
        .replay_held = false,
        .mass_audit_forced = false,
        .scheduler_state_mutated = false,
        .adapter_call_attempted = false,
        .release_gate_action_executed = false,
    };
}

FaultControllerSchedulerMassAuditPolicyValidation make_fault_controller_scheduler_mass_audit_policy_validation(
    const FaultControllerSchedulerReplayPolicyValidation& replay_policy,
    bool mass_audit_policy_present) {
    const bool control_requested = replay_policy.reason != FaultControllerSchedulerReplayPolicyReason::no_scheduler_control_requested;
    auto reason = FaultControllerSchedulerMassAuditPolicyReason::no_scheduler_control_requested;
    if (control_requested) {
        reason = mass_audit_policy_present
            ? FaultControllerSchedulerMassAuditPolicyReason::mass_audit_policy_present
            : FaultControllerSchedulerMassAuditPolicyReason::mass_audit_policy_missing;
    }

    return FaultControllerSchedulerMassAuditPolicyValidation{
        .replay_policy_status = replay_policy.status,
        .replay_policy_reason = replay_policy.reason,
        .status = FaultControllerSchedulerMassAuditPolicyStatus::invalid,
        .reason = reason,
        .replay_policy_expected = replay_policy.replay_policy_expected,
        .replay_policy_present = replay_policy.replay_policy_present,
        .mass_audit_policy_expected = control_requested,
        .mass_audit_policy_present = mass_audit_policy_present,
        .scheduler_control_allowed = false,
        .scheduler_control_used = false,
        .exchange_requests_paused = false,
        .target_engine_request_skipped = false,
        .replay_held = false,
        .mass_audit_forced = false,
        .scheduler_state_mutated = false,
        .adapter_call_attempted = false,
        .release_gate_action_executed = false,
    };
}

namespace {

bool is_phase_control_supported(
    FaultControllerSchedulerPhase phase,
    FaultControllerSchedulerExecutableControlKind control_kind) {
    switch (control_kind) {
    case FaultControllerSchedulerExecutableControlKind::none:
        return false;
    case FaultControllerSchedulerExecutableControlKind::observe_only:
        return true;
    case FaultControllerSchedulerExecutableControlKind::pause_before_phase:
        return phase == FaultControllerSchedulerPhase::exchange_request_preparation
            || phase == FaultControllerSchedulerPhase::shared_cell_arbitration
            || phase == FaultControllerSchedulerPhase::exchange_acceptance
            || phase == FaultControllerSchedulerPhase::post_step_reporting;
    case FaultControllerSchedulerExecutableControlKind::skip_target_engine_request:
        return phase == FaultControllerSchedulerPhase::exchange_request_preparation;
    case FaultControllerSchedulerExecutableControlKind::hold_replay_until_review:
        return phase == FaultControllerSchedulerPhase::replay;
    case FaultControllerSchedulerExecutableControlKind::force_mass_audit_before_continue:
        return phase == FaultControllerSchedulerPhase::mass_audit;
    case FaultControllerSchedulerExecutableControlKind::abort_scheduler_step:
        return phase == FaultControllerSchedulerPhase::exchange_request_preparation
            || phase == FaultControllerSchedulerPhase::shared_cell_arbitration
            || phase == FaultControllerSchedulerPhase::exchange_acceptance
            || phase == FaultControllerSchedulerPhase::replay
            || phase == FaultControllerSchedulerPhase::mass_audit;
    }
    return false;
}

}  // namespace

FaultControllerSchedulerPhaseControlCompatibilityValidation make_fault_controller_scheduler_phase_control_compatibility_validation(
    const FaultControllerSchedulerMassAuditPolicyValidation& mass_audit_policy,
    const FaultControllerSchedulerPhaseDescriptor& target_phase,
    FaultControllerSchedulerExecutableControlKind requested_control_kind) {
    const bool control_requested = mass_audit_policy.reason != FaultControllerSchedulerMassAuditPolicyReason::no_scheduler_control_requested;
    const bool phase_control_expected = control_requested;
    const bool phase_control_supported = control_requested
        && is_phase_control_supported(target_phase.phase, requested_control_kind);

    auto reason = FaultControllerSchedulerPhaseControlCompatibilityReason::no_scheduler_control_requested;
    if (control_requested && requested_control_kind == FaultControllerSchedulerExecutableControlKind::none) {
        reason = FaultControllerSchedulerPhaseControlCompatibilityReason::control_kind_missing;
    } else if (control_requested && target_phase.category == FaultControllerSchedulerPhaseCategory::passive_provenance
        && requested_control_kind != FaultControllerSchedulerExecutableControlKind::observe_only) {
        reason = FaultControllerSchedulerPhaseControlCompatibilityReason::target_phase_not_controllable;
    } else if (control_requested && !phase_control_supported) {
        reason = FaultControllerSchedulerPhaseControlCompatibilityReason::control_kind_unsupported_for_phase;
    } else if (control_requested) {
        reason = FaultControllerSchedulerPhaseControlCompatibilityReason::control_kind_supported;
    }

    return FaultControllerSchedulerPhaseControlCompatibilityValidation{
        .mass_audit_policy_status = mass_audit_policy.status,
        .mass_audit_policy_reason = mass_audit_policy.reason,
        .target_phase = target_phase,
        .requested_control_kind = requested_control_kind,
        .status = phase_control_supported
            ? FaultControllerSchedulerPhaseControlCompatibilityStatus::compatible
            : FaultControllerSchedulerPhaseControlCompatibilityStatus::incompatible,
        .reason = reason,
        .phase_control_expected = phase_control_expected,
        .phase_control_supported = phase_control_supported,
        .scheduler_control_allowed = false,
        .scheduler_control_used = false,
        .exchange_requests_paused = false,
        .target_engine_request_skipped = false,
        .replay_held = false,
        .mass_audit_forced = false,
        .scheduler_state_mutated = false,
        .adapter_call_attempted = false,
        .release_gate_action_executed = false,
    };
}

FaultControllerExecutableSchedulerControlResult execute_fault_controller_scheduler_control(
    const FaultControllerSchedulerPhaseControlCompatibilityValidation& compatibility,
    bool scheduler_control_boundary_available,
    bool execution_evidence_complete) {
    auto status = FaultControllerExecutableSchedulerControlStatus::blocked_boundary_absent;
    auto reason = FaultControllerExecutableSchedulerControlReason::scheduler_control_boundary_absent;

    if (scheduler_control_boundary_available) {
        if (!execution_evidence_complete) {
            status = FaultControllerExecutableSchedulerControlStatus::blocked_missing_execution_evidence;
            reason = FaultControllerExecutableSchedulerControlReason::execution_evidence_missing;
        } else if (compatibility.status != FaultControllerSchedulerPhaseControlCompatibilityStatus::compatible) {
            status = FaultControllerExecutableSchedulerControlStatus::blocked_incompatible_phase_control;
            reason = FaultControllerExecutableSchedulerControlReason::incompatible_phase_control;
        } else if (compatibility.requested_control_kind == FaultControllerSchedulerExecutableControlKind::observe_only) {
            status = FaultControllerExecutableSchedulerControlStatus::executed_noop;
            reason = FaultControllerExecutableSchedulerControlReason::observe_only_noop;
        } else {
            status = FaultControllerExecutableSchedulerControlStatus::blocked_missing_execution_evidence;
            reason = FaultControllerExecutableSchedulerControlReason::mutating_execution_evidence_missing;
        }
    }

    return FaultControllerExecutableSchedulerControlResult{
        .compatibility_status = compatibility.status,
        .compatibility_reason = compatibility.reason,
        .target_phase = compatibility.target_phase,
        .requested_control_kind = compatibility.requested_control_kind,
        .status = status,
        .reason = reason,
        .scheduler_control_boundary_available = scheduler_control_boundary_available,
        .execution_evidence_complete = execution_evidence_complete,
        .scheduler_control_allowed = false,
        .scheduler_control_used = false,
        .exchange_requests_paused = false,
        .target_engine_request_skipped = false,
        .replay_held = false,
        .mass_audit_forced = false,
        .scheduler_state_mutated = false,
        .adapter_call_attempted = false,
        .release_gate_action_executed = false,
    };
}

namespace {

FaultControllerSchedulerPhaseControlCompatibilityValidation compatibility_from_evidence(
    FaultControllerSchedulerPhaseControlCompatibilityStatus compatibility_status,
    FaultControllerSchedulerPhaseControlCompatibilityReason compatibility_reason,
    const FaultControllerSchedulerPhaseDescriptor& target_phase,
    FaultControllerSchedulerExecutableControlKind requested_control_kind) {
    return FaultControllerSchedulerPhaseControlCompatibilityValidation{
        .target_phase = target_phase,
        .requested_control_kind = requested_control_kind,
        .status = compatibility_status,
        .reason = compatibility_reason,
        .phase_control_expected = requested_control_kind != FaultControllerSchedulerExecutableControlKind::none,
        .phase_control_supported = compatibility_status == FaultControllerSchedulerPhaseControlCompatibilityStatus::compatible,
        .scheduler_control_allowed = false,
        .scheduler_control_used = false,
        .exchange_requests_paused = false,
        .target_engine_request_skipped = false,
        .replay_held = false,
        .mass_audit_forced = false,
        .scheduler_state_mutated = false,
        .adapter_call_attempted = false,
        .release_gate_action_executed = false,
    };
}

}  // namespace

FaultControllerExecutableSchedulerControlResult execute_fault_controller_scheduler_control(
    const FaultControllerPauseBeforePhaseExecutionEvidence& evidence,
    bool scheduler_control_boundary_available) {
    return execute_fault_controller_scheduler_control(
        compatibility_from_evidence(
            evidence.compatibility_status,
            evidence.compatibility_reason,
            evidence.target_phase,
            evidence.requested_control_kind),
        scheduler_control_boundary_available,
        evidence.status == FaultControllerPauseBeforePhaseEvidenceStatus::complete);
}

FaultControllerExecutableSchedulerControlResult execute_fault_controller_scheduler_control(
    const FaultControllerSkipTargetEngineExecutionEvidence& evidence,
    bool scheduler_control_boundary_available) {
    return execute_fault_controller_scheduler_control(
        compatibility_from_evidence(
            evidence.compatibility_status,
            evidence.compatibility_reason,
            evidence.target_phase,
            evidence.requested_control_kind),
        scheduler_control_boundary_available,
        evidence.status == FaultControllerSkipTargetEngineEvidenceStatus::complete);
}

FaultControllerExecutableSchedulerControlResult execute_fault_controller_scheduler_control(
    const FaultControllerHoldReplayExecutionEvidence& evidence,
    bool scheduler_control_boundary_available) {
    return execute_fault_controller_scheduler_control(
        compatibility_from_evidence(
            evidence.compatibility_status,
            evidence.compatibility_reason,
            evidence.target_phase,
            evidence.requested_control_kind),
        scheduler_control_boundary_available,
        evidence.status == FaultControllerHoldReplayEvidenceStatus::complete);
}

FaultControllerExecutableSchedulerControlResult execute_fault_controller_scheduler_control(
    const FaultControllerForceMassAuditExecutionEvidence& evidence,
    bool scheduler_control_boundary_available) {
    return execute_fault_controller_scheduler_control(
        compatibility_from_evidence(
            evidence.compatibility_status,
            evidence.compatibility_reason,
            evidence.target_phase,
            evidence.requested_control_kind),
        scheduler_control_boundary_available,
        evidence.status == FaultControllerForceMassAuditEvidenceStatus::complete);
}

FaultControllerExecutableSchedulerControlResult execute_fault_controller_scheduler_control(
    const FaultControllerAbortSchedulerStepExecutionEvidence& evidence,
    bool scheduler_control_boundary_available) {
    return execute_fault_controller_scheduler_control(
        compatibility_from_evidence(
            evidence.compatibility_status,
            evidence.compatibility_reason,
            evidence.target_phase,
            evidence.requested_control_kind),
        scheduler_control_boundary_available,
        evidence.status == FaultControllerAbortSchedulerStepEvidenceStatus::complete);
}

FaultControllerPauseBeforePhaseExecutionEvidence make_fault_controller_pause_before_phase_execution_evidence(
    const FaultControllerSchedulerPhaseControlCompatibilityValidation& compatibility,
    bool phase_boundary_identified,
    bool pause_window_defined,
    bool resume_condition_defined,
    bool ordering_impact_reviewed,
    bool conservation_impact_reviewed) {
    auto reason = FaultControllerPauseBeforePhaseEvidenceReason::no_scheduler_control_requested;
    if (!compatibility.phase_control_expected) {
        reason = FaultControllerPauseBeforePhaseEvidenceReason::no_scheduler_control_requested;
    } else if (compatibility.requested_control_kind != FaultControllerSchedulerExecutableControlKind::pause_before_phase) {
        reason = FaultControllerPauseBeforePhaseEvidenceReason::wrong_control_kind;
    } else if (compatibility.status != FaultControllerSchedulerPhaseControlCompatibilityStatus::compatible) {
        reason = FaultControllerPauseBeforePhaseEvidenceReason::target_phase_not_pause_compatible;
    } else if (!phase_boundary_identified) {
        reason = FaultControllerPauseBeforePhaseEvidenceReason::phase_boundary_missing;
    } else if (!pause_window_defined) {
        reason = FaultControllerPauseBeforePhaseEvidenceReason::pause_window_missing;
    } else if (!resume_condition_defined) {
        reason = FaultControllerPauseBeforePhaseEvidenceReason::resume_condition_missing;
    } else if (!ordering_impact_reviewed) {
        reason = FaultControllerPauseBeforePhaseEvidenceReason::ordering_impact_missing;
    } else if (!conservation_impact_reviewed) {
        reason = FaultControllerPauseBeforePhaseEvidenceReason::conservation_impact_missing;
    } else {
        reason = FaultControllerPauseBeforePhaseEvidenceReason::evidence_present;
    }

    const bool complete = reason == FaultControllerPauseBeforePhaseEvidenceReason::evidence_present;
    return FaultControllerPauseBeforePhaseExecutionEvidence{
        .compatibility_status = compatibility.status,
        .compatibility_reason = compatibility.reason,
        .target_phase = compatibility.target_phase,
        .requested_control_kind = compatibility.requested_control_kind,
        .status = complete
            ? FaultControllerPauseBeforePhaseEvidenceStatus::complete
            : FaultControllerPauseBeforePhaseEvidenceStatus::incomplete,
        .reason = reason,
        .phase_boundary_identified = phase_boundary_identified,
        .pause_window_defined = pause_window_defined,
        .resume_condition_defined = resume_condition_defined,
        .ordering_impact_reviewed = ordering_impact_reviewed,
        .conservation_impact_reviewed = conservation_impact_reviewed,
        .scheduler_control_allowed = false,
        .scheduler_control_used = false,
        .exchange_requests_paused = false,
        .target_engine_request_skipped = false,
        .replay_held = false,
        .mass_audit_forced = false,
        .scheduler_state_mutated = false,
        .adapter_call_attempted = false,
        .release_gate_action_executed = false,
    };
}

FaultControllerSkipTargetEngineExecutionEvidence make_fault_controller_skip_target_engine_execution_evidence(
    const FaultControllerSchedulerPhaseControlCompatibilityValidation& compatibility,
    std::string target_engine_id,
    bool skip_scope_defined,
    bool ordering_impact_reviewed,
    bool conservation_impact_reviewed,
    bool replay_continuity_reviewed,
    bool mass_audit_continuity_reviewed) {
    const bool target_engine_identified = !target_engine_id.empty();
    auto reason = FaultControllerSkipTargetEngineEvidenceReason::no_scheduler_control_requested;
    if (!compatibility.phase_control_expected) {
        reason = FaultControllerSkipTargetEngineEvidenceReason::no_scheduler_control_requested;
    } else if (compatibility.requested_control_kind != FaultControllerSchedulerExecutableControlKind::skip_target_engine_request) {
        reason = FaultControllerSkipTargetEngineEvidenceReason::wrong_control_kind;
    } else if (compatibility.status != FaultControllerSchedulerPhaseControlCompatibilityStatus::compatible) {
        reason = FaultControllerSkipTargetEngineEvidenceReason::target_phase_not_skip_compatible;
    } else if (!target_engine_identified) {
        reason = FaultControllerSkipTargetEngineEvidenceReason::target_engine_missing;
    } else if (!skip_scope_defined) {
        reason = FaultControllerSkipTargetEngineEvidenceReason::skip_scope_missing;
    } else if (!ordering_impact_reviewed) {
        reason = FaultControllerSkipTargetEngineEvidenceReason::ordering_impact_missing;
    } else if (!conservation_impact_reviewed) {
        reason = FaultControllerSkipTargetEngineEvidenceReason::conservation_impact_missing;
    } else if (!replay_continuity_reviewed) {
        reason = FaultControllerSkipTargetEngineEvidenceReason::replay_continuity_missing;
    } else if (!mass_audit_continuity_reviewed) {
        reason = FaultControllerSkipTargetEngineEvidenceReason::mass_audit_continuity_missing;
    } else {
        reason = FaultControllerSkipTargetEngineEvidenceReason::evidence_present;
    }

    const bool complete = reason == FaultControllerSkipTargetEngineEvidenceReason::evidence_present;
    return FaultControllerSkipTargetEngineExecutionEvidence{
        .compatibility_status = compatibility.status,
        .compatibility_reason = compatibility.reason,
        .target_phase = compatibility.target_phase,
        .requested_control_kind = compatibility.requested_control_kind,
        .status = complete
            ? FaultControllerSkipTargetEngineEvidenceStatus::complete
            : FaultControllerSkipTargetEngineEvidenceStatus::incomplete,
        .reason = reason,
        .target_engine_id = std::move(target_engine_id),
        .target_engine_identified = target_engine_identified,
        .skip_scope_defined = skip_scope_defined,
        .ordering_impact_reviewed = ordering_impact_reviewed,
        .conservation_impact_reviewed = conservation_impact_reviewed,
        .replay_continuity_reviewed = replay_continuity_reviewed,
        .mass_audit_continuity_reviewed = mass_audit_continuity_reviewed,
        .scheduler_control_allowed = false,
        .scheduler_control_used = false,
        .exchange_requests_paused = false,
        .target_engine_request_skipped = false,
        .replay_held = false,
        .mass_audit_forced = false,
        .scheduler_state_mutated = false,
        .adapter_call_attempted = false,
        .release_gate_action_executed = false,
    };
}

FaultControllerHoldReplayExecutionEvidence make_fault_controller_hold_replay_execution_evidence(
    const FaultControllerSchedulerPhaseControlCompatibilityValidation& compatibility,
    bool replay_hold_scope_defined,
    bool pending_event_state_defined,
    bool rollback_context_linked,
    bool resume_condition_defined,
    bool ordering_impact_reviewed,
    bool conservation_impact_reviewed,
    bool mass_audit_continuity_reviewed) {
    auto reason = FaultControllerHoldReplayEvidenceReason::no_scheduler_control_requested;
    if (!compatibility.phase_control_expected) {
        reason = FaultControllerHoldReplayEvidenceReason::no_scheduler_control_requested;
    } else if (compatibility.requested_control_kind != FaultControllerSchedulerExecutableControlKind::hold_replay_until_review) {
        reason = FaultControllerHoldReplayEvidenceReason::wrong_control_kind;
    } else if (compatibility.status != FaultControllerSchedulerPhaseControlCompatibilityStatus::compatible) {
        reason = FaultControllerHoldReplayEvidenceReason::target_phase_not_hold_compatible;
    } else if (!replay_hold_scope_defined) {
        reason = FaultControllerHoldReplayEvidenceReason::replay_hold_scope_missing;
    } else if (!pending_event_state_defined) {
        reason = FaultControllerHoldReplayEvidenceReason::pending_event_state_missing;
    } else if (!rollback_context_linked) {
        reason = FaultControllerHoldReplayEvidenceReason::rollback_context_link_missing;
    } else if (!resume_condition_defined) {
        reason = FaultControllerHoldReplayEvidenceReason::resume_condition_missing;
    } else if (!ordering_impact_reviewed) {
        reason = FaultControllerHoldReplayEvidenceReason::ordering_impact_missing;
    } else if (!conservation_impact_reviewed) {
        reason = FaultControllerHoldReplayEvidenceReason::conservation_impact_missing;
    } else if (!mass_audit_continuity_reviewed) {
        reason = FaultControllerHoldReplayEvidenceReason::mass_audit_continuity_missing;
    } else {
        reason = FaultControllerHoldReplayEvidenceReason::evidence_present;
    }

    const bool complete = reason == FaultControllerHoldReplayEvidenceReason::evidence_present;
    return FaultControllerHoldReplayExecutionEvidence{
        .compatibility_status = compatibility.status,
        .compatibility_reason = compatibility.reason,
        .target_phase = compatibility.target_phase,
        .requested_control_kind = compatibility.requested_control_kind,
        .status = complete
            ? FaultControllerHoldReplayEvidenceStatus::complete
            : FaultControllerHoldReplayEvidenceStatus::incomplete,
        .reason = reason,
        .replay_hold_scope_defined = replay_hold_scope_defined,
        .pending_event_state_defined = pending_event_state_defined,
        .rollback_context_linked = rollback_context_linked,
        .resume_condition_defined = resume_condition_defined,
        .ordering_impact_reviewed = ordering_impact_reviewed,
        .conservation_impact_reviewed = conservation_impact_reviewed,
        .mass_audit_continuity_reviewed = mass_audit_continuity_reviewed,
        .scheduler_control_allowed = false,
        .scheduler_control_used = false,
        .exchange_requests_paused = false,
        .target_engine_request_skipped = false,
        .replay_held = false,
        .mass_audit_forced = false,
        .scheduler_state_mutated = false,
        .adapter_call_attempted = false,
        .release_gate_action_executed = false,
    };
}

FaultControllerForceMassAuditExecutionEvidence make_fault_controller_force_mass_audit_execution_evidence(
    const FaultControllerSchedulerPhaseControlCompatibilityValidation& compatibility,
    bool mass_audit_scope_defined,
    bool baseline_mass_state_preserved,
    bool current_mass_state_preserved,
    bool conservation_status_policy_defined,
    bool ordering_impact_reviewed,
    bool replay_continuity_reviewed) {
    auto reason = FaultControllerForceMassAuditEvidenceReason::no_scheduler_control_requested;
    if (!compatibility.phase_control_expected) {
        reason = FaultControllerForceMassAuditEvidenceReason::no_scheduler_control_requested;
    } else if (compatibility.requested_control_kind != FaultControllerSchedulerExecutableControlKind::force_mass_audit_before_continue) {
        reason = FaultControllerForceMassAuditEvidenceReason::wrong_control_kind;
    } else if (compatibility.status != FaultControllerSchedulerPhaseControlCompatibilityStatus::compatible) {
        reason = FaultControllerForceMassAuditEvidenceReason::target_phase_not_audit_compatible;
    } else if (!mass_audit_scope_defined) {
        reason = FaultControllerForceMassAuditEvidenceReason::mass_audit_scope_missing;
    } else if (!baseline_mass_state_preserved) {
        reason = FaultControllerForceMassAuditEvidenceReason::baseline_mass_state_missing;
    } else if (!current_mass_state_preserved) {
        reason = FaultControllerForceMassAuditEvidenceReason::current_mass_state_missing;
    } else if (!conservation_status_policy_defined) {
        reason = FaultControllerForceMassAuditEvidenceReason::conservation_status_policy_missing;
    } else if (!ordering_impact_reviewed) {
        reason = FaultControllerForceMassAuditEvidenceReason::ordering_impact_missing;
    } else if (!replay_continuity_reviewed) {
        reason = FaultControllerForceMassAuditEvidenceReason::replay_continuity_missing;
    } else {
        reason = FaultControllerForceMassAuditEvidenceReason::evidence_present;
    }

    const bool complete = reason == FaultControllerForceMassAuditEvidenceReason::evidence_present;
    return FaultControllerForceMassAuditExecutionEvidence{
        .compatibility_status = compatibility.status,
        .compatibility_reason = compatibility.reason,
        .target_phase = compatibility.target_phase,
        .requested_control_kind = compatibility.requested_control_kind,
        .status = complete
            ? FaultControllerForceMassAuditEvidenceStatus::complete
            : FaultControllerForceMassAuditEvidenceStatus::incomplete,
        .reason = reason,
        .mass_audit_scope_defined = mass_audit_scope_defined,
        .baseline_mass_state_preserved = baseline_mass_state_preserved,
        .current_mass_state_preserved = current_mass_state_preserved,
        .conservation_status_policy_defined = conservation_status_policy_defined,
        .ordering_impact_reviewed = ordering_impact_reviewed,
        .replay_continuity_reviewed = replay_continuity_reviewed,
        .scheduler_control_allowed = false,
        .scheduler_control_used = false,
        .exchange_requests_paused = false,
        .target_engine_request_skipped = false,
        .replay_held = false,
        .mass_audit_forced = false,
        .scheduler_state_mutated = false,
        .adapter_call_attempted = false,
        .release_gate_action_executed = false,
    };
}

FaultControllerAbortSchedulerStepExecutionEvidence make_fault_controller_abort_scheduler_step_execution_evidence(
    const FaultControllerSchedulerPhaseControlCompatibilityValidation& compatibility,
    bool abort_scope_defined,
    bool rollback_context_linked,
    bool replay_policy_continuity_reviewed,
    bool mass_audit_policy_continuity_reviewed,
    bool ordering_impact_reviewed,
    bool conservation_impact_reviewed,
    bool release_gate_boundary_reviewed) {
    auto reason = FaultControllerAbortSchedulerStepEvidenceReason::no_scheduler_control_requested;
    if (!compatibility.phase_control_expected) {
        reason = FaultControllerAbortSchedulerStepEvidenceReason::no_scheduler_control_requested;
    } else if (compatibility.requested_control_kind != FaultControllerSchedulerExecutableControlKind::abort_scheduler_step) {
        reason = FaultControllerAbortSchedulerStepEvidenceReason::wrong_control_kind;
    } else if (compatibility.status != FaultControllerSchedulerPhaseControlCompatibilityStatus::compatible) {
        reason = FaultControllerAbortSchedulerStepEvidenceReason::target_phase_not_abort_compatible;
    } else if (!abort_scope_defined) {
        reason = FaultControllerAbortSchedulerStepEvidenceReason::abort_scope_missing;
    } else if (!rollback_context_linked) {
        reason = FaultControllerAbortSchedulerStepEvidenceReason::rollback_context_link_missing;
    } else if (!replay_policy_continuity_reviewed) {
        reason = FaultControllerAbortSchedulerStepEvidenceReason::replay_policy_continuity_missing;
    } else if (!mass_audit_policy_continuity_reviewed) {
        reason = FaultControllerAbortSchedulerStepEvidenceReason::mass_audit_policy_continuity_missing;
    } else if (!ordering_impact_reviewed) {
        reason = FaultControllerAbortSchedulerStepEvidenceReason::ordering_impact_missing;
    } else if (!conservation_impact_reviewed) {
        reason = FaultControllerAbortSchedulerStepEvidenceReason::conservation_impact_missing;
    } else if (!release_gate_boundary_reviewed) {
        reason = FaultControllerAbortSchedulerStepEvidenceReason::release_gate_boundary_missing;
    } else {
        reason = FaultControllerAbortSchedulerStepEvidenceReason::evidence_present;
    }

    const bool complete = reason == FaultControllerAbortSchedulerStepEvidenceReason::evidence_present;
    return FaultControllerAbortSchedulerStepExecutionEvidence{
        .compatibility_status = compatibility.status,
        .compatibility_reason = compatibility.reason,
        .target_phase = compatibility.target_phase,
        .requested_control_kind = compatibility.requested_control_kind,
        .status = complete
            ? FaultControllerAbortSchedulerStepEvidenceStatus::complete
            : FaultControllerAbortSchedulerStepEvidenceStatus::incomplete,
        .reason = reason,
        .abort_scope_defined = abort_scope_defined,
        .rollback_context_linked = rollback_context_linked,
        .replay_policy_continuity_reviewed = replay_policy_continuity_reviewed,
        .mass_audit_policy_continuity_reviewed = mass_audit_policy_continuity_reviewed,
        .ordering_impact_reviewed = ordering_impact_reviewed,
        .conservation_impact_reviewed = conservation_impact_reviewed,
        .release_gate_boundary_reviewed = release_gate_boundary_reviewed,
        .scheduler_control_allowed = false,
        .scheduler_control_used = false,
        .exchange_requests_paused = false,
        .target_engine_request_skipped = false,
        .replay_held = false,
        .mass_audit_forced = false,
        .scheduler_state_mutated = false,
        .scheduler_step_aborted = false,
        .adapter_call_attempted = false,
        .release_gate_action_executed = false,
    };
}

namespace {

bool scheduler_mutation_control_kind_supported(FaultControllerSchedulerExecutableControlKind control_kind) {
    return control_kind == FaultControllerSchedulerExecutableControlKind::pause_before_phase
        || control_kind == FaultControllerSchedulerExecutableControlKind::skip_target_engine_request
        || control_kind == FaultControllerSchedulerExecutableControlKind::hold_replay_until_review
        || control_kind == FaultControllerSchedulerExecutableControlKind::force_mass_audit_before_continue
        || control_kind == FaultControllerSchedulerExecutableControlKind::abort_scheduler_step;
}

bool scheduler_mutation_upstream_kind_matches(
    const FaultControllerSchedulerMutationExecutionRequest& request) {
    return request.authorization.requested_control_kind == request.requested_control_kind
        && request.attempt.requested_control_kind == request.requested_control_kind
        && request.dry_run.requested_control_kind == request.requested_control_kind;
}

bool scheduler_mutation_upstream_phase_matches(
    const FaultControllerSchedulerMutationExecutionRequest& request) {
    return request.authorization.target_phase.phase == request.target_phase.phase
        && request.attempt.target_phase.phase == request.target_phase.phase
        && request.dry_run.target_phase.phase == request.target_phase.phase;
}

bool scheduler_mutation_target_engine_found(
    const FaultControllerSchedulerMutableState& state,
    const std::string& target_engine_id) {
    return std::any_of(
        state.exchange_requests.begin(),
        state.exchange_requests.end(),
        [&target_engine_id](const FaultControllerSchedulerExchangeRequestState& exchange_request) {
            return exchange_request.target_engine_id == target_engine_id;
        });
}

FaultControllerSchedulerMutationExecutionResult make_scheduler_mutation_execution_result(
    const FaultControllerSchedulerMutationExecutionRequest& request,
    FaultControllerSchedulerMutationExecutionResultStatus status,
    FaultControllerSchedulerMutationExecutionResultReason reason,
    FaultControllerSchedulerMutableState after,
    FaultControllerSchedulerExecutableControlKind actual_mutation_kind = FaultControllerSchedulerExecutableControlKind::none) {
    const auto mutation_generation_after = after.mutation_generation;
    const auto rollback_required = after.step_state.rollback_required;
    const auto replay_required = after.step_state.replay_required;
    const auto mass_audit_required = after.step_state.mass_audit_required;
    const auto operator_review_required = after.step_state.review_required;
    return FaultControllerSchedulerMutationExecutionResult{
        .request = request,
        .status = status,
        .reason = reason,
        .requested_control_kind = request.requested_control_kind,
        .target_phase = request.target_phase,
        .target_engine_id = request.target_engine_id,
        .scheduler_state_before = request.scheduler_state,
        .scheduler_state_after = std::move(after),
        .mutation_generation_before = request.scheduler_state.mutation_generation,
        .mutation_generation_after = mutation_generation_after,
        .expected_mutation_kind = request.requested_control_kind,
        .actual_mutation_kind = actual_mutation_kind,
        .exchange_requests_paused = actual_mutation_kind == FaultControllerSchedulerExecutableControlKind::pause_before_phase,
        .target_engine_request_skipped = actual_mutation_kind == FaultControllerSchedulerExecutableControlKind::skip_target_engine_request,
        .replay_held = actual_mutation_kind == FaultControllerSchedulerExecutableControlKind::hold_replay_until_review,
        .mass_audit_forced = actual_mutation_kind == FaultControllerSchedulerExecutableControlKind::force_mass_audit_before_continue,
        .scheduler_step_aborted = actual_mutation_kind == FaultControllerSchedulerExecutableControlKind::abort_scheduler_step,
        .non_target_behavior_changed = false,
        .rollback_required = rollback_required,
        .replay_required = replay_required,
        .mass_audit_required = mass_audit_required,
        .operator_review_required = operator_review_required,
        .adapter_call_attempted = false,
        .release_gate_action_executed = false,
        .postcondition_required = status == FaultControllerSchedulerMutationExecutionResultStatus::executed,
    };
}

FaultControllerSchedulerMutationAuthorization make_fault_controller_scheduler_mutation_authorization(
    const FaultControllerExecutableSchedulerControlResult& executable_result,
    FaultControllerSchedulerExecutableControlKind evidence_control_kind,
    const FaultControllerSchedulerPhaseDescriptor& evidence_target_phase,
    bool behavior_evidence_complete,
    const FaultControllerMutationAuthorizationRequest& request) {
    auto reason = FaultControllerSchedulerMutationAuthorizationReason::authorization_present;
    if (evidence_control_kind == FaultControllerSchedulerExecutableControlKind::none
        || request.requested_control_kind == FaultControllerSchedulerExecutableControlKind::none) {
        reason = FaultControllerSchedulerMutationAuthorizationReason::no_scheduler_control_requested;
    } else if (!request.authorization_boundary_available) {
        reason = FaultControllerSchedulerMutationAuthorizationReason::authorization_boundary_absent;
    } else if (!behavior_evidence_complete) {
        reason = FaultControllerSchedulerMutationAuthorizationReason::behavior_evidence_incomplete;
    } else if (evidence_control_kind != request.requested_control_kind) {
        reason = FaultControllerSchedulerMutationAuthorizationReason::control_kind_mismatch;
    } else if (evidence_target_phase.phase != request.target_phase.phase) {
        reason = FaultControllerSchedulerMutationAuthorizationReason::target_phase_mismatch;
    } else if (!request.operator_mutation_approved) {
        reason = FaultControllerSchedulerMutationAuthorizationReason::operator_mutation_approval_missing;
    } else if (!request.runtime_mutation_enabled) {
        reason = FaultControllerSchedulerMutationAuthorizationReason::runtime_mutation_disabled;
    } else if (!request.rollback_plan_available) {
        reason = FaultControllerSchedulerMutationAuthorizationReason::rollback_plan_missing;
    } else if (!request.replay_plan_available) {
        reason = FaultControllerSchedulerMutationAuthorizationReason::replay_plan_missing;
    } else if (!request.mass_audit_plan_available) {
        reason = FaultControllerSchedulerMutationAuthorizationReason::mass_audit_plan_missing;
    } else if (!request.audit_sink_available) {
        reason = FaultControllerSchedulerMutationAuthorizationReason::audit_sink_missing;
    } else if (!request.release_gate_policy_available) {
        reason = FaultControllerSchedulerMutationAuthorizationReason::release_gate_policy_missing;
    } else if (!request.conservation_impact_bounded) {
        reason = FaultControllerSchedulerMutationAuthorizationReason::conservation_impact_not_bounded;
    }

    const bool authorized = reason == FaultControllerSchedulerMutationAuthorizationReason::authorization_present;
    return FaultControllerSchedulerMutationAuthorization{
        .executable_result = executable_result,
        .requested_control_kind = request.requested_control_kind,
        .target_phase = request.target_phase,
        .status = authorized
            ? FaultControllerSchedulerMutationAuthorizationStatus::authorized
            : FaultControllerSchedulerMutationAuthorizationStatus::not_authorized,
        .reason = reason,
        .operator_mutation_approved = request.operator_mutation_approved,
        .runtime_mutation_enabled = request.runtime_mutation_enabled,
        .rollback_plan_available = request.rollback_plan_available,
        .replay_plan_available = request.replay_plan_available,
        .mass_audit_plan_available = request.mass_audit_plan_available,
        .audit_sink_available = request.audit_sink_available,
        .release_gate_policy_available = request.release_gate_policy_available,
        .conservation_impact_bounded = request.conservation_impact_bounded,
        .scheduler_control_allowed = authorized,
        .scheduler_state_mutation_allowed = authorized,
        .exchange_requests_pause_allowed = authorized
            && request.requested_control_kind == FaultControllerSchedulerExecutableControlKind::pause_before_phase,
        .target_engine_request_skip_allowed = authorized
            && request.requested_control_kind == FaultControllerSchedulerExecutableControlKind::skip_target_engine_request,
        .replay_hold_allowed = authorized
            && request.requested_control_kind == FaultControllerSchedulerExecutableControlKind::hold_replay_until_review,
        .mass_audit_force_allowed = authorized
            && request.requested_control_kind == FaultControllerSchedulerExecutableControlKind::force_mass_audit_before_continue,
        .scheduler_step_abort_allowed = authorized
            && request.requested_control_kind == FaultControllerSchedulerExecutableControlKind::abort_scheduler_step,
        .scheduler_control_used = false,
        .exchange_requests_paused = false,
        .target_engine_request_skipped = false,
        .replay_held = false,
        .mass_audit_forced = false,
        .scheduler_step_aborted = false,
        .adapter_call_attempted = false,
        .release_gate_action_executed = false,
    };
}

}  // namespace

FaultControllerSchedulerMutationAuthorization authorize_fault_controller_scheduler_mutation(
    const FaultControllerPauseBeforePhaseExecutionEvidence& evidence,
    const FaultControllerMutationAuthorizationRequest& request) {
    return make_fault_controller_scheduler_mutation_authorization(
        execute_fault_controller_scheduler_control(evidence, true),
        evidence.requested_control_kind,
        evidence.target_phase,
        evidence.status == FaultControllerPauseBeforePhaseEvidenceStatus::complete,
        request);
}

FaultControllerSchedulerMutationAuthorization authorize_fault_controller_scheduler_mutation(
    const FaultControllerSkipTargetEngineExecutionEvidence& evidence,
    const FaultControllerMutationAuthorizationRequest& request) {
    return make_fault_controller_scheduler_mutation_authorization(
        execute_fault_controller_scheduler_control(evidence, true),
        evidence.requested_control_kind,
        evidence.target_phase,
        evidence.status == FaultControllerSkipTargetEngineEvidenceStatus::complete,
        request);
}

FaultControllerSchedulerMutationAuthorization authorize_fault_controller_scheduler_mutation(
    const FaultControllerHoldReplayExecutionEvidence& evidence,
    const FaultControllerMutationAuthorizationRequest& request) {
    return make_fault_controller_scheduler_mutation_authorization(
        execute_fault_controller_scheduler_control(evidence, true),
        evidence.requested_control_kind,
        evidence.target_phase,
        evidence.status == FaultControllerHoldReplayEvidenceStatus::complete,
        request);
}

FaultControllerSchedulerMutationAuthorization authorize_fault_controller_scheduler_mutation(
    const FaultControllerForceMassAuditExecutionEvidence& evidence,
    const FaultControllerMutationAuthorizationRequest& request) {
    return make_fault_controller_scheduler_mutation_authorization(
        execute_fault_controller_scheduler_control(evidence, true),
        evidence.requested_control_kind,
        evidence.target_phase,
        evidence.status == FaultControllerForceMassAuditEvidenceStatus::complete,
        request);
}

FaultControllerSchedulerMutationAuthorization authorize_fault_controller_scheduler_mutation(
    const FaultControllerAbortSchedulerStepExecutionEvidence& evidence,
    const FaultControllerMutationAuthorizationRequest& request) {
    return make_fault_controller_scheduler_mutation_authorization(
        execute_fault_controller_scheduler_control(evidence, true),
        evidence.requested_control_kind,
        evidence.target_phase,
        evidence.status == FaultControllerAbortSchedulerStepEvidenceStatus::complete,
        request);
}

FaultControllerSchedulerMutationExecutionAttempt make_fault_controller_scheduler_mutation_execution_attempt(
    const FaultControllerSchedulerMutationAuthorization& authorization,
    const FaultControllerSchedulerMutationExecutionAttemptRequest& request) {
    auto reason = FaultControllerSchedulerMutationExecutionAttemptReason::attempt_recorded;
    if (authorization.status != FaultControllerSchedulerMutationAuthorizationStatus::authorized) {
        reason = FaultControllerSchedulerMutationExecutionAttemptReason::authorization_not_granted;
    } else if (!request.execution_boundary_available) {
        reason = FaultControllerSchedulerMutationExecutionAttemptReason::execution_boundary_absent;
    } else if (!request.pre_state_reference_available) {
        reason = FaultControllerSchedulerMutationExecutionAttemptReason::pre_state_reference_missing;
    } else if (!request.runtime_evidence_sink_available) {
        reason = FaultControllerSchedulerMutationExecutionAttemptReason::runtime_evidence_sink_missing;
    }

    const bool recorded = reason == FaultControllerSchedulerMutationExecutionAttemptReason::attempt_recorded;
    return FaultControllerSchedulerMutationExecutionAttempt{
        .authorization = authorization,
        .status = recorded
            ? FaultControllerSchedulerMutationExecutionAttemptStatus::attempt_recorded
            : FaultControllerSchedulerMutationExecutionAttemptStatus::not_attempted,
        .reason = reason,
        .requested_control_kind = authorization.requested_control_kind,
        .target_phase = authorization.target_phase,
        .execution_boundary_available = request.execution_boundary_available,
        .pre_state_reference_available = request.pre_state_reference_available,
        .runtime_evidence_sink_available = request.runtime_evidence_sink_available,
        .pre_step_phase_name = request.pre_step_phase_name,
        .pre_step_pending_event_count = request.pre_step_pending_event_count,
        .pre_step_exchange_request_count = request.pre_step_exchange_request_count,
        .pre_step_replay_pending = request.pre_step_replay_pending,
        .pre_step_mass_total = request.pre_step_mass_total,
        .pre_step_conservation_status = request.pre_step_conservation_status,
        .scheduler_control_attempted = recorded,
        .exchange_requests_paused = false,
        .target_engine_request_skipped = false,
        .replay_held = false,
        .mass_audit_forced = false,
        .scheduler_step_aborted = false,
        .adapter_call_attempted = false,
        .release_gate_action_executed = false,
    };
}

FaultControllerSchedulerMutationPostconditionEvidence make_fault_controller_scheduler_mutation_postcondition_evidence(
    const FaultControllerSchedulerMutationExecutionAttempt& attempt,
    const FaultControllerSchedulerMutationPostconditionRequest& request) {
    auto reason = FaultControllerSchedulerMutationPostconditionReason::verified_expected_no_change;
    if (attempt.status != FaultControllerSchedulerMutationExecutionAttemptStatus::attempt_recorded) {
        reason = FaultControllerSchedulerMutationPostconditionReason::execution_not_attempted;
    } else if (!request.post_state_reference_available) {
        reason = FaultControllerSchedulerMutationPostconditionReason::post_state_reference_missing;
    } else if (!request.expected_mutation && request.phase_changed) {
        reason = FaultControllerSchedulerMutationPostconditionReason::unexpected_phase_change;
    } else if (!request.expected_mutation && request.exchange_request_changed) {
        reason = FaultControllerSchedulerMutationPostconditionReason::unexpected_exchange_request_change;
    } else if (!request.expected_mutation && request.replay_changed) {
        reason = FaultControllerSchedulerMutationPostconditionReason::unexpected_replay_change;
    } else if (!request.expected_mutation && request.mass_audit_changed) {
        reason = FaultControllerSchedulerMutationPostconditionReason::unexpected_mass_audit_change;
    } else if (!request.conservation_residual_bounded) {
        reason = FaultControllerSchedulerMutationPostconditionReason::conservation_residual_unbounded;
    } else if (!request.release_gate_policy_available) {
        reason = FaultControllerSchedulerMutationPostconditionReason::release_gate_policy_missing;
    } else if (request.expected_mutation) {
        reason = FaultControllerSchedulerMutationPostconditionReason::verified_expected_mutation;
    }

    auto status = FaultControllerSchedulerMutationPostconditionStatus::verified_no_change;
    if (reason == FaultControllerSchedulerMutationPostconditionReason::execution_not_attempted) {
        status = FaultControllerSchedulerMutationPostconditionStatus::not_evaluated;
    } else if (reason == FaultControllerSchedulerMutationPostconditionReason::verified_expected_mutation) {
        status = FaultControllerSchedulerMutationPostconditionStatus::verified_mutation;
    } else if (reason != FaultControllerSchedulerMutationPostconditionReason::verified_expected_no_change) {
        status = FaultControllerSchedulerMutationPostconditionStatus::failed_postcondition;
    }

    return FaultControllerSchedulerMutationPostconditionEvidence{
        .attempt = attempt,
        .status = status,
        .reason = reason,
        .requested_control_kind = attempt.requested_control_kind,
        .target_phase = attempt.target_phase,
        .pre_step_phase_name = attempt.pre_step_phase_name,
        .post_step_phase_name = request.post_step_phase_name,
        .pre_step_pending_event_count = attempt.pre_step_pending_event_count,
        .post_step_pending_event_count = request.post_step_pending_event_count,
        .pre_step_exchange_request_count = attempt.pre_step_exchange_request_count,
        .post_step_exchange_request_count = request.post_step_exchange_request_count,
        .pre_step_replay_pending = attempt.pre_step_replay_pending,
        .post_step_replay_pending = request.post_step_replay_pending,
        .pre_step_mass_total = attempt.pre_step_mass_total,
        .post_step_mass_total = request.post_step_mass_total,
        .conservation_residual = request.conservation_residual,
        .conservation_residual_bounded = request.conservation_residual_bounded,
        .exchange_requests_paused = false,
        .target_engine_request_skipped = false,
        .replay_held = false,
        .mass_audit_forced = false,
        .scheduler_step_aborted = false,
        .adapter_call_attempted = false,
        .release_gate_action_executed = false,
        .operator_review_required = status == FaultControllerSchedulerMutationPostconditionStatus::failed_postcondition,
        .protocol_state = status == FaultControllerSchedulerMutationPostconditionStatus::failed_postcondition
            ? "REVIEW_REQUIRED"
            : "",
    };
}

FaultControllerSchedulerMutationRuntimeEvidenceWriteRecord make_fault_controller_scheduler_mutation_runtime_evidence_write_record(
    const FaultControllerSchedulerMutationPostconditionEvidence& postcondition,
    bool runtime_evidence_sink_available,
    bool release_gate_policy_allows_write,
    std::string evidence_record_id) {
    auto reason = FaultControllerSchedulerMutationRuntimeEvidenceWriteReason::written;
    if (!runtime_evidence_sink_available) {
        reason = FaultControllerSchedulerMutationRuntimeEvidenceWriteReason::runtime_evidence_sink_missing;
    } else if (postcondition.status == FaultControllerSchedulerMutationPostconditionStatus::not_evaluated) {
        reason = FaultControllerSchedulerMutationRuntimeEvidenceWriteReason::postcondition_missing;
    } else if (postcondition.status == FaultControllerSchedulerMutationPostconditionStatus::failed_postcondition) {
        reason = FaultControllerSchedulerMutationRuntimeEvidenceWriteReason::postcondition_failed;
    } else if (!release_gate_policy_allows_write) {
        reason = FaultControllerSchedulerMutationRuntimeEvidenceWriteReason::write_blocked_by_release_gate_policy;
    }

    const bool written = reason == FaultControllerSchedulerMutationRuntimeEvidenceWriteReason::written;
    return FaultControllerSchedulerMutationRuntimeEvidenceWriteRecord{
        .postcondition = postcondition,
        .status = written
            ? FaultControllerSchedulerMutationRuntimeEvidenceWriteStatus::written
            : FaultControllerSchedulerMutationRuntimeEvidenceWriteStatus::blocked,
        .reason = reason,
        .evidence_schema_name = "fault_controller_scheduler_mutation_runtime_evidence",
        .evidence_schema_version = "1",
        .evidence_record_id = written ? std::move(evidence_record_id) : "",
        .runtime_evidence_sink_available = runtime_evidence_sink_available,
        .release_gate_policy_allows_write = release_gate_policy_allows_write,
        .write_attempted = written,
        .write_succeeded = written,
        .write_failed = !written,
        .release_gate_action_executed = false,
        .protocol_state = postcondition.protocol_state,
    };
}

FaultControllerSchedulerMutationDryRunResult make_fault_controller_scheduler_mutation_dry_run_result(
    const FaultControllerSchedulerMutationAuthorization& authorization,
    const FaultControllerSchedulerMutationExecutionAttempt& attempt,
    const FaultControllerSchedulerMutationDryRunRequest& request) {
    auto reason = FaultControllerSchedulerMutationDryRunReason::dry_run_recorded;
    if (authorization.status != FaultControllerSchedulerMutationAuthorizationStatus::authorized) {
        reason = FaultControllerSchedulerMutationDryRunReason::authorization_not_granted;
    } else if (attempt.status != FaultControllerSchedulerMutationExecutionAttemptStatus::attempt_recorded) {
        reason = FaultControllerSchedulerMutationDryRunReason::execution_attempt_not_recorded;
    } else if (!request.dry_run_boundary_available) {
        reason = FaultControllerSchedulerMutationDryRunReason::dry_run_boundary_absent;
    } else if (!request.runtime_evidence_sink_available) {
        reason = FaultControllerSchedulerMutationDryRunReason::runtime_evidence_sink_missing;
    } else if (!request.postcondition_template_available) {
        reason = FaultControllerSchedulerMutationDryRunReason::postcondition_template_missing;
    } else if (request.requested_control_kind == FaultControllerSchedulerExecutableControlKind::none
        || request.requested_control_kind == FaultControllerSchedulerExecutableControlKind::observe_only) {
        reason = FaultControllerSchedulerMutationDryRunReason::unsupported_control_kind;
    } else if (request.requested_control_kind != authorization.requested_control_kind
        || request.requested_control_kind != attempt.requested_control_kind) {
        reason = FaultControllerSchedulerMutationDryRunReason::control_kind_mismatch;
    } else if (request.target_phase.phase != authorization.target_phase.phase
        || request.target_phase.phase != attempt.target_phase.phase) {
        reason = FaultControllerSchedulerMutationDryRunReason::target_phase_mismatch;
    }

    const bool recorded = reason == FaultControllerSchedulerMutationDryRunReason::dry_run_recorded;
    return FaultControllerSchedulerMutationDryRunResult{
        .authorization = authorization,
        .attempt = attempt,
        .status = recorded
            ? FaultControllerSchedulerMutationDryRunStatus::dry_run_recorded
            : FaultControllerSchedulerMutationDryRunStatus::blocked,
        .reason = reason,
        .requested_control_kind = request.requested_control_kind,
        .target_phase = request.target_phase,
        .phase_before_dry_run = request.expected_phase_before,
        .phase_after_dry_run = request.expected_phase_after,
        .expected_affected_exchange_request_count = request.expected_affected_exchange_request_count,
        .expected_affected_target_engine_count = request.expected_affected_target_engine_count,
        .expected_replay_pending = request.expected_replay_pending,
        .expected_mass_audit_required = request.expected_mass_audit_required,
        .expected_conservation_impact_bounded = request.expected_conservation_impact_bounded,
        .would_pause_exchange_requests = recorded
            && request.requested_control_kind == FaultControllerSchedulerExecutableControlKind::pause_before_phase,
        .would_skip_target_engine_request = recorded
            && request.requested_control_kind == FaultControllerSchedulerExecutableControlKind::skip_target_engine_request,
        .would_hold_replay = recorded
            && request.requested_control_kind == FaultControllerSchedulerExecutableControlKind::hold_replay_until_review,
        .would_force_mass_audit = recorded
            && request.requested_control_kind == FaultControllerSchedulerExecutableControlKind::force_mass_audit_before_continue,
        .would_abort_scheduler_step = recorded
            && request.requested_control_kind == FaultControllerSchedulerExecutableControlKind::abort_scheduler_step,
        .scheduler_control_used = false,
        .exchange_requests_paused = false,
        .target_engine_request_skipped = false,
        .replay_held = false,
        .mass_audit_forced = false,
        .scheduler_step_aborted = false,
        .adapter_call_attempted = false,
        .release_gate_action_executed = false,
        .runtime_evidence_sink_available = request.runtime_evidence_sink_available,
        .postcondition_template_available = request.postcondition_template_available,
        .dry_run_evidence_recorded = recorded,
    };
}

FaultControllerSchedulerMutationExecutionResult execute_fault_controller_scheduler_mutation(
    const FaultControllerSchedulerMutationExecutionRequest& request) {
    auto after = request.scheduler_state;
    if (request.authorization.status != FaultControllerSchedulerMutationAuthorizationStatus::authorized) {
        return make_scheduler_mutation_execution_result(
            request,
            FaultControllerSchedulerMutationExecutionResultStatus::blocked,
            FaultControllerSchedulerMutationExecutionResultReason::authorization_not_granted,
            std::move(after));
    }
    if (request.attempt.status != FaultControllerSchedulerMutationExecutionAttemptStatus::attempt_recorded) {
        return make_scheduler_mutation_execution_result(
            request,
            FaultControllerSchedulerMutationExecutionResultStatus::blocked,
            FaultControllerSchedulerMutationExecutionResultReason::execution_attempt_not_recorded,
            std::move(after));
    }
    if (request.dry_run.status != FaultControllerSchedulerMutationDryRunStatus::dry_run_recorded) {
        return make_scheduler_mutation_execution_result(
            request,
            FaultControllerSchedulerMutationExecutionResultStatus::blocked,
            FaultControllerSchedulerMutationExecutionResultReason::dry_run_not_recorded,
            std::move(after));
    }
    if (!request.execution_boundary_available) {
        return make_scheduler_mutation_execution_result(
            request,
            FaultControllerSchedulerMutationExecutionResultStatus::blocked,
            FaultControllerSchedulerMutationExecutionResultReason::execution_boundary_absent,
            std::move(after));
    }
    if (!scheduler_mutation_upstream_kind_matches(request)) {
        return make_scheduler_mutation_execution_result(
            request,
            FaultControllerSchedulerMutationExecutionResultStatus::blocked,
            FaultControllerSchedulerMutationExecutionResultReason::upstream_control_kind_mismatch,
            std::move(after));
    }
    if (!scheduler_mutation_upstream_phase_matches(request)) {
        return make_scheduler_mutation_execution_result(
            request,
            FaultControllerSchedulerMutationExecutionResultStatus::blocked,
            FaultControllerSchedulerMutationExecutionResultReason::upstream_target_phase_mismatch,
            std::move(after));
    }
    if (!request.scheduler_state_present) {
        return make_scheduler_mutation_execution_result(
            request,
            FaultControllerSchedulerMutationExecutionResultStatus::blocked,
            FaultControllerSchedulerMutationExecutionResultReason::scheduler_state_missing,
            std::move(after));
    }
    if (request.scheduler_state.current_phase.phase != request.target_phase.phase) {
        return make_scheduler_mutation_execution_result(
            request,
            FaultControllerSchedulerMutationExecutionResultStatus::blocked,
            FaultControllerSchedulerMutationExecutionResultReason::target_phase_not_current,
            std::move(after));
    }
    if (request.requested_control_kind == FaultControllerSchedulerExecutableControlKind::skip_target_engine_request
        && request.target_engine_id.empty()) {
        return make_scheduler_mutation_execution_result(
            request,
            FaultControllerSchedulerMutationExecutionResultStatus::blocked,
            FaultControllerSchedulerMutationExecutionResultReason::target_engine_missing,
            std::move(after));
    }
    if (request.requested_control_kind == FaultControllerSchedulerExecutableControlKind::skip_target_engine_request
        && !scheduler_mutation_target_engine_found(request.scheduler_state, request.target_engine_id)) {
        return make_scheduler_mutation_execution_result(
            request,
            FaultControllerSchedulerMutationExecutionResultStatus::blocked,
            FaultControllerSchedulerMutationExecutionResultReason::target_engine_not_found,
            std::move(after));
    }
    if (request.rollback_plan_reference_id.empty()) {
        return make_scheduler_mutation_execution_result(
            request,
            FaultControllerSchedulerMutationExecutionResultStatus::blocked,
            FaultControllerSchedulerMutationExecutionResultReason::rollback_plan_missing,
            std::move(after));
    }
    if (request.replay_plan_reference_id.empty()) {
        return make_scheduler_mutation_execution_result(
            request,
            FaultControllerSchedulerMutationExecutionResultStatus::blocked,
            FaultControllerSchedulerMutationExecutionResultReason::replay_plan_missing,
            std::move(after));
    }
    if (request.mass_audit_plan_reference_id.empty()) {
        return make_scheduler_mutation_execution_result(
            request,
            FaultControllerSchedulerMutationExecutionResultStatus::blocked,
            FaultControllerSchedulerMutationExecutionResultReason::mass_audit_plan_missing,
            std::move(after));
    }
    if (!request.runtime_evidence_sink_available) {
        return make_scheduler_mutation_execution_result(
            request,
            FaultControllerSchedulerMutationExecutionResultStatus::blocked,
            FaultControllerSchedulerMutationExecutionResultReason::runtime_evidence_sink_missing,
            std::move(after));
    }
    if (!request.postcondition_template_available) {
        return make_scheduler_mutation_execution_result(
            request,
            FaultControllerSchedulerMutationExecutionResultStatus::blocked,
            FaultControllerSchedulerMutationExecutionResultReason::postcondition_template_missing,
            std::move(after));
    }
    if (!scheduler_mutation_control_kind_supported(request.requested_control_kind)) {
        return make_scheduler_mutation_execution_result(
            request,
            FaultControllerSchedulerMutationExecutionResultStatus::blocked,
            FaultControllerSchedulerMutationExecutionResultReason::unsupported_control_kind,
            std::move(after));
    }

    auto reason = FaultControllerSchedulerMutationExecutionResultReason::postcondition_required;
    switch (request.requested_control_kind) {
    case FaultControllerSchedulerExecutableControlKind::pause_before_phase:
        for (auto& exchange_request : after.exchange_requests) {
            if (exchange_request.target_phase.phase == request.target_phase.phase) {
                exchange_request.request_paused = true;
            }
        }
        reason = FaultControllerSchedulerMutationExecutionResultReason::executed_pause;
        break;
    case FaultControllerSchedulerExecutableControlKind::skip_target_engine_request:
        for (auto& exchange_request : after.exchange_requests) {
            if (exchange_request.target_engine_id == request.target_engine_id) {
                exchange_request.request_skipped = true;
                break;
            }
        }
        reason = FaultControllerSchedulerMutationExecutionResultReason::executed_skip_target_engine_request;
        break;
    case FaultControllerSchedulerExecutableControlKind::hold_replay_until_review:
        after.replay_state.replay_held = true;
        reason = FaultControllerSchedulerMutationExecutionResultReason::executed_hold_replay;
        break;
    case FaultControllerSchedulerExecutableControlKind::force_mass_audit_before_continue:
        after.mass_audit_state.mass_audit_forced = true;
        reason = FaultControllerSchedulerMutationExecutionResultReason::executed_force_mass_audit;
        break;
    case FaultControllerSchedulerExecutableControlKind::abort_scheduler_step:
        after.step_state.step_aborted = true;
        after.step_state.review_required = true;
        after.step_state.rollback_required = true;
        after.step_state.replay_required = true;
        after.step_state.mass_audit_required = true;
        after.step_state.abort_reason_id = request.rollback_plan_reference_id;
        after.step_state.protocol_state = "REVIEW_REQUIRED";
        reason = FaultControllerSchedulerMutationExecutionResultReason::executed_abort_scheduler_step;
        break;
    case FaultControllerSchedulerExecutableControlKind::none:
    case FaultControllerSchedulerExecutableControlKind::observe_only:
        break;
    }
    after.mutation_generation = request.scheduler_state.mutation_generation + 1U;
    return make_scheduler_mutation_execution_result(
        request,
        FaultControllerSchedulerMutationExecutionResultStatus::executed,
        reason,
        std::move(after),
        request.requested_control_kind);
}

FaultControllerSchedulerMutationCompletionRecord consume_fault_controller_scheduler_mutation_postcondition(
    const FaultControllerSchedulerMutationExecutionResult& execution_result,
    const FaultControllerSchedulerMutationPostconditionEvidence& postcondition,
    bool postcondition_present) {
    auto reason = FaultControllerSchedulerMutationCompletionReason::postcondition_verified;
    if (execution_result.status != FaultControllerSchedulerMutationExecutionResultStatus::executed) {
        reason = FaultControllerSchedulerMutationCompletionReason::execution_not_completed;
    } else if (!postcondition_present) {
        reason = FaultControllerSchedulerMutationCompletionReason::postcondition_missing;
    } else if (postcondition.status != FaultControllerSchedulerMutationPostconditionStatus::verified_mutation) {
        reason = postcondition.status == FaultControllerSchedulerMutationPostconditionStatus::failed_postcondition
            ? FaultControllerSchedulerMutationCompletionReason::review_required
            : FaultControllerSchedulerMutationCompletionReason::postcondition_not_verified;
    } else if (postcondition.requested_control_kind != execution_result.requested_control_kind) {
        reason = FaultControllerSchedulerMutationCompletionReason::control_kind_mismatch;
    } else if (postcondition.target_phase.phase != execution_result.target_phase.phase) {
        reason = FaultControllerSchedulerMutationCompletionReason::target_phase_mismatch;
    } else if (execution_result.expected_mutation_kind != execution_result.actual_mutation_kind) {
        reason = FaultControllerSchedulerMutationCompletionReason::mutation_kind_mismatch;
    } else if (execution_result.mutation_generation_after != execution_result.mutation_generation_before + 1U) {
        reason = FaultControllerSchedulerMutationCompletionReason::mutation_generation_mismatch;
    } else if (execution_result.adapter_call_attempted || postcondition.adapter_call_attempted) {
        reason = FaultControllerSchedulerMutationCompletionReason::adapter_call_attempted;
    } else if (execution_result.release_gate_action_executed || postcondition.release_gate_action_executed) {
        reason = FaultControllerSchedulerMutationCompletionReason::release_gate_action_executed;
    }

    auto status = FaultControllerSchedulerMutationCompletionStatus::incomplete;
    if (reason == FaultControllerSchedulerMutationCompletionReason::postcondition_verified) {
        status = FaultControllerSchedulerMutationCompletionStatus::complete;
    } else if (reason == FaultControllerSchedulerMutationCompletionReason::review_required) {
        status = FaultControllerSchedulerMutationCompletionStatus::review_required;
    }

    return FaultControllerSchedulerMutationCompletionRecord{
        .execution_result = execution_result,
        .postcondition = postcondition,
        .status = status,
        .reason = reason,
        .requested_control_kind = execution_result.requested_control_kind,
        .target_phase = execution_result.target_phase,
        .mutation_generation_before = execution_result.mutation_generation_before,
        .mutation_generation_after = execution_result.mutation_generation_after,
        .postcondition_present = postcondition_present,
        .postcondition_verified = reason == FaultControllerSchedulerMutationCompletionReason::postcondition_verified,
        .mutation_completed = status == FaultControllerSchedulerMutationCompletionStatus::complete,
        .operator_review_required = status == FaultControllerSchedulerMutationCompletionStatus::review_required,
        .adapter_call_attempted = execution_result.adapter_call_attempted || postcondition.adapter_call_attempted,
        .release_gate_action_executed = execution_result.release_gate_action_executed || postcondition.release_gate_action_executed,
    };
}

FaultControllerSchedulerMutationRuntimeWriteIntent prepare_fault_controller_scheduler_mutation_runtime_write_intent(
    const FaultControllerSchedulerMutationRuntimeWriteIntentRequest& request) {
    auto reason = FaultControllerSchedulerMutationRuntimeWriteIntentReason::write_intent_prepared;
    if (request.completion.status != FaultControllerSchedulerMutationCompletionStatus::complete) {
        reason = request.completion.status == FaultControllerSchedulerMutationCompletionStatus::review_required
            ? FaultControllerSchedulerMutationRuntimeWriteIntentReason::completion_requires_review
            : FaultControllerSchedulerMutationRuntimeWriteIntentReason::mutation_not_completed;
    } else if (request.completion.operator_review_required) {
        reason = FaultControllerSchedulerMutationRuntimeWriteIntentReason::completion_requires_review;
    } else if (!request.write_boundary_available) {
        reason = FaultControllerSchedulerMutationRuntimeWriteIntentReason::write_boundary_absent;
    } else if (!request.runtime_evidence_sink_available) {
        reason = FaultControllerSchedulerMutationRuntimeWriteIntentReason::runtime_evidence_sink_missing;
    } else if (request.evidence_schema_name.empty()) {
        reason = FaultControllerSchedulerMutationRuntimeWriteIntentReason::schema_name_missing;
    } else if (request.evidence_schema_version.empty()) {
        reason = FaultControllerSchedulerMutationRuntimeWriteIntentReason::schema_version_missing;
    } else if (request.evidence_record_id.empty()) {
        reason = FaultControllerSchedulerMutationRuntimeWriteIntentReason::evidence_record_id_missing;
    } else if (request.scheduler_state_before_reference_id.empty()) {
        reason = FaultControllerSchedulerMutationRuntimeWriteIntentReason::state_before_reference_missing;
    } else if (request.scheduler_state_after_reference_id.empty()) {
        reason = FaultControllerSchedulerMutationRuntimeWriteIntentReason::state_after_reference_missing;
    } else if (request.postcondition_evidence_reference_id.empty()) {
        reason = FaultControllerSchedulerMutationRuntimeWriteIntentReason::postcondition_reference_missing;
    } else if (request.release_gate_policy_reference_id.empty()) {
        reason = FaultControllerSchedulerMutationRuntimeWriteIntentReason::release_gate_policy_reference_missing;
    } else if (request.completion.adapter_call_attempted) {
        reason = FaultControllerSchedulerMutationRuntimeWriteIntentReason::adapter_call_attempted;
    } else if (request.completion.release_gate_action_executed) {
        reason = FaultControllerSchedulerMutationRuntimeWriteIntentReason::release_gate_action_already_executed;
    }

    auto status = FaultControllerSchedulerMutationRuntimeWriteIntentStatus::blocked;
    if (reason == FaultControllerSchedulerMutationRuntimeWriteIntentReason::write_intent_prepared) {
        status = FaultControllerSchedulerMutationRuntimeWriteIntentStatus::prepared;
    } else if (reason == FaultControllerSchedulerMutationRuntimeWriteIntentReason::completion_requires_review) {
        status = FaultControllerSchedulerMutationRuntimeWriteIntentStatus::review_required;
    }

    return FaultControllerSchedulerMutationRuntimeWriteIntent{
        .request = request,
        .status = status,
        .reason = reason,
        .requested_control_kind = request.completion.requested_control_kind,
        .target_phase = request.completion.target_phase,
        .mutation_generation_before = request.completion.mutation_generation_before,
        .mutation_generation_after = request.completion.mutation_generation_after,
        .evidence_schema_name = request.evidence_schema_name,
        .evidence_schema_version = request.evidence_schema_version,
        .evidence_record_id = status == FaultControllerSchedulerMutationRuntimeWriteIntentStatus::prepared ? request.evidence_record_id : "",
        .scheduler_state_before_reference_id = request.scheduler_state_before_reference_id,
        .scheduler_state_after_reference_id = request.scheduler_state_after_reference_id,
        .postcondition_evidence_reference_id = request.postcondition_evidence_reference_id,
        .release_gate_policy_reference_id = request.release_gate_policy_reference_id,
        .write_intent_prepared = status == FaultControllerSchedulerMutationRuntimeWriteIntentStatus::prepared,
        .external_write_attempted = false,
        .external_write_succeeded = false,
        .external_write_failed = false,
        .adapter_call_attempted = request.completion.adapter_call_attempted,
        .release_gate_action_executed = false,
        .operator_review_required = status == FaultControllerSchedulerMutationRuntimeWriteIntentStatus::review_required,
    };
}

FaultControllerSchedulerReleaseGatePolicyDecision make_fault_controller_scheduler_release_gate_policy_decision(
    const FaultControllerSchedulerReleaseGatePolicyDecisionRequest& request) {
    auto reason = FaultControllerSchedulerReleaseGatePolicyDecisionReason::pass_decision;
    if (request.write_intent.status != FaultControllerSchedulerMutationRuntimeWriteIntentStatus::prepared) {
        reason = request.write_intent.status == FaultControllerSchedulerMutationRuntimeWriteIntentStatus::review_required
            ? FaultControllerSchedulerReleaseGatePolicyDecisionReason::write_intent_requires_review
            : FaultControllerSchedulerReleaseGatePolicyDecisionReason::write_intent_not_prepared;
    } else if (!request.runtime_evidence_write_completed) {
        reason = FaultControllerSchedulerReleaseGatePolicyDecisionReason::runtime_write_missing;
    } else if (request.runtime_evidence_write_failed || !request.runtime_evidence_write_succeeded) {
        reason = FaultControllerSchedulerReleaseGatePolicyDecisionReason::runtime_write_failed;
    } else if (!request.policy_boundary_available) {
        reason = FaultControllerSchedulerReleaseGatePolicyDecisionReason::policy_boundary_absent;
    } else if (request.policy_reference_id.empty()) {
        reason = FaultControllerSchedulerReleaseGatePolicyDecisionReason::policy_reference_missing;
    } else if (!request.operator_review_policy_enabled) {
        reason = FaultControllerSchedulerReleaseGatePolicyDecisionReason::operator_review_policy_missing;
    } else if (request.merge_gate_evaluation_enabled && !request.merge_gate_policy_enabled) {
        reason = FaultControllerSchedulerReleaseGatePolicyDecisionReason::merge_gate_policy_missing;
    } else if (request.release_gate_evaluation_enabled && !request.release_gate_policy_enabled) {
        reason = FaultControllerSchedulerReleaseGatePolicyDecisionReason::release_gate_policy_missing;
    } else if (request.release_gate_evaluation_enabled && !request.golden_suite_evidence_available) {
        reason = FaultControllerSchedulerReleaseGatePolicyDecisionReason::golden_suite_evidence_missing;
    } else if (request.merge_gate_evaluation_enabled && !request.ci_evidence_available) {
        reason = FaultControllerSchedulerReleaseGatePolicyDecisionReason::ci_evidence_missing;
    } else if (request.release_gate_evaluation_enabled && !request.release_evidence_available) {
        reason = FaultControllerSchedulerReleaseGatePolicyDecisionReason::release_evidence_missing;
    } else if (request.write_intent.adapter_call_attempted) {
        reason = FaultControllerSchedulerReleaseGatePolicyDecisionReason::adapter_call_attempted;
    } else if (request.write_intent.release_gate_action_executed) {
        reason = FaultControllerSchedulerReleaseGatePolicyDecisionReason::upstream_release_gate_action_detected;
    }

    auto status = FaultControllerSchedulerReleaseGatePolicyDecisionStatus::decision_recorded;
    if (reason == FaultControllerSchedulerReleaseGatePolicyDecisionReason::write_intent_not_prepared
        || reason == FaultControllerSchedulerReleaseGatePolicyDecisionReason::policy_boundary_absent
        || reason == FaultControllerSchedulerReleaseGatePolicyDecisionReason::policy_reference_missing
        || reason == FaultControllerSchedulerReleaseGatePolicyDecisionReason::operator_review_policy_missing) {
        status = FaultControllerSchedulerReleaseGatePolicyDecisionStatus::blocked;
    } else if (reason != FaultControllerSchedulerReleaseGatePolicyDecisionReason::pass_decision) {
        status = FaultControllerSchedulerReleaseGatePolicyDecisionStatus::review_required;
    }

    const bool block_merge = reason == FaultControllerSchedulerReleaseGatePolicyDecisionReason::ci_evidence_missing
        || reason == FaultControllerSchedulerReleaseGatePolicyDecisionReason::merge_gate_policy_missing;
    const bool block_release = reason == FaultControllerSchedulerReleaseGatePolicyDecisionReason::golden_suite_evidence_missing
        || reason == FaultControllerSchedulerReleaseGatePolicyDecisionReason::release_evidence_missing
        || reason == FaultControllerSchedulerReleaseGatePolicyDecisionReason::release_gate_policy_missing;
    const bool review_required = status == FaultControllerSchedulerReleaseGatePolicyDecisionStatus::review_required
        && !block_merge
        && !block_release;
    return FaultControllerSchedulerReleaseGatePolicyDecision{
        .request = request,
        .status = status,
        .reason = reason,
        .requested_control_kind = request.write_intent.requested_control_kind,
        .target_phase = request.write_intent.target_phase,
        .mutation_generation_before = request.write_intent.mutation_generation_before,
        .mutation_generation_after = request.write_intent.mutation_generation_after,
        .evidence_record_id = request.write_intent.evidence_record_id,
        .policy_reference_id = request.policy_reference_id,
        .review_required = review_required,
        .merge_blocked = block_merge,
        .release_blocked = block_release,
        .abort_required = reason == FaultControllerSchedulerReleaseGatePolicyDecisionReason::abort_decision,
        .policy_decision_recorded = status == FaultControllerSchedulerReleaseGatePolicyDecisionStatus::decision_recorded,
        .release_gate_action_emitted = false,
        .external_write_attempted = request.write_intent.external_write_attempted,
        .adapter_call_attempted = request.write_intent.adapter_call_attempted,
    };
}

FaultControllerSchedulerRuntimeEvidenceWriteResult write_fault_controller_scheduler_runtime_evidence(
    const FaultControllerSchedulerRuntimeEvidenceWriteRequest& request) {
    auto reason = FaultControllerSchedulerRuntimeEvidenceWriteReason::write_succeeded;
    if (request.write_intent.status != FaultControllerSchedulerMutationRuntimeWriteIntentStatus::prepared) {
        reason = FaultControllerSchedulerRuntimeEvidenceWriteReason::write_intent_not_prepared;
    } else if (!request.writer_boundary_available) {
        reason = FaultControllerSchedulerRuntimeEvidenceWriteReason::writer_boundary_absent;
    } else if (!request.evidence_sink_available) {
        reason = FaultControllerSchedulerRuntimeEvidenceWriteReason::evidence_sink_missing;
    } else if (request.sink_transaction_id.empty()) {
        reason = FaultControllerSchedulerRuntimeEvidenceWriteReason::sink_transaction_missing;
    } else if (request.idempotency_key.empty()) {
        reason = FaultControllerSchedulerRuntimeEvidenceWriteReason::idempotency_key_missing;
    } else if (request.payload_reference_id.empty()) {
        reason = FaultControllerSchedulerRuntimeEvidenceWriteReason::payload_reference_missing;
    } else if (!request.payload_schema_valid) {
        reason = FaultControllerSchedulerRuntimeEvidenceWriteReason::payload_schema_invalid;
    } else if (request.payload_content_hash.empty()) {
        reason = FaultControllerSchedulerRuntimeEvidenceWriteReason::payload_hash_missing;
    } else if (request.persistence_target_reference_id.empty()) {
        reason = FaultControllerSchedulerRuntimeEvidenceWriteReason::persistence_target_missing;
    } else if (!request.write_dry_run && !request.write_commit) {
        reason = FaultControllerSchedulerRuntimeEvidenceWriteReason::write_mode_missing;
    } else if (request.write_dry_run && request.write_commit) {
        reason = FaultControllerSchedulerRuntimeEvidenceWriteReason::conflicting_write_mode;
    } else if (request.write_dry_run) {
        reason = FaultControllerSchedulerRuntimeEvidenceWriteReason::dry_run_recorded;
    } else if (!request.sink_write_succeeded) {
        reason = FaultControllerSchedulerRuntimeEvidenceWriteReason::write_failed;
    }

    auto status = FaultControllerSchedulerRuntimeEvidenceWriteStatus::blocked;
    if (reason == FaultControllerSchedulerRuntimeEvidenceWriteReason::dry_run_recorded) {
        status = FaultControllerSchedulerRuntimeEvidenceWriteStatus::dry_run_recorded;
    } else if (reason == FaultControllerSchedulerRuntimeEvidenceWriteReason::write_succeeded) {
        status = FaultControllerSchedulerRuntimeEvidenceWriteStatus::written;
    } else if (reason == FaultControllerSchedulerRuntimeEvidenceWriteReason::write_failed) {
        status = FaultControllerSchedulerRuntimeEvidenceWriteStatus::failed;
    }

    const bool write_attempted = request.write_commit
        && status != FaultControllerSchedulerRuntimeEvidenceWriteStatus::blocked;
    return FaultControllerSchedulerRuntimeEvidenceWriteResult{
        .request = request,
        .status = status,
        .reason = reason,
        .requested_control_kind = request.write_intent.requested_control_kind,
        .target_phase = request.write_intent.target_phase,
        .mutation_generation_before = request.write_intent.mutation_generation_before,
        .mutation_generation_after = request.write_intent.mutation_generation_after,
        .evidence_schema_name = request.write_intent.evidence_schema_name,
        .evidence_schema_version = request.write_intent.evidence_schema_version,
        .evidence_record_id = status == FaultControllerSchedulerRuntimeEvidenceWriteStatus::blocked ? "" : request.write_intent.evidence_record_id,
        .scheduler_state_before_reference_id = request.write_intent.scheduler_state_before_reference_id,
        .scheduler_state_after_reference_id = request.write_intent.scheduler_state_after_reference_id,
        .postcondition_evidence_reference_id = request.write_intent.postcondition_evidence_reference_id,
        .release_gate_policy_reference_id = request.write_intent.release_gate_policy_reference_id,
        .sink_transaction_id = request.sink_transaction_id,
        .idempotency_key = request.idempotency_key,
        .payload_reference_id = request.payload_reference_id,
        .payload_content_hash = request.payload_content_hash,
        .persistence_target_reference_id = request.persistence_target_reference_id,
        .write_attempted = write_attempted,
        .write_dry_run_recorded = status == FaultControllerSchedulerRuntimeEvidenceWriteStatus::dry_run_recorded,
        .write_committed = status == FaultControllerSchedulerRuntimeEvidenceWriteStatus::written,
        .write_succeeded = status == FaultControllerSchedulerRuntimeEvidenceWriteStatus::written,
        .write_failed = status == FaultControllerSchedulerRuntimeEvidenceWriteStatus::failed,
        .operator_review_required = status == FaultControllerSchedulerRuntimeEvidenceWriteStatus::failed,
        .release_gate_action_emitted = false,
    };
}

FaultControllerSchedulerReleaseGateActionIntent prepare_fault_controller_scheduler_release_gate_action_intent(
    const FaultControllerSchedulerReleaseGateActionIntentRequest& request) {
    auto reason = FaultControllerSchedulerReleaseGateActionIntentReason::pass_no_action;
    if (request.policy_decision.status != FaultControllerSchedulerReleaseGatePolicyDecisionStatus::decision_recorded
        && request.policy_decision.status != FaultControllerSchedulerReleaseGatePolicyDecisionStatus::review_required) {
        reason = FaultControllerSchedulerReleaseGateActionIntentReason::policy_decision_not_recorded;
    } else if (!request.runtime_write.write_committed || !request.runtime_write.write_succeeded) {
        reason = FaultControllerSchedulerReleaseGateActionIntentReason::runtime_write_not_committed;
    } else if (!request.action_boundary_available) {
        reason = FaultControllerSchedulerReleaseGateActionIntentReason::action_boundary_absent;
    } else if (!request.action_sink_available) {
        reason = FaultControllerSchedulerReleaseGateActionIntentReason::action_sink_missing;
    } else if (request.action_reference_id.empty()) {
        reason = FaultControllerSchedulerReleaseGateActionIntentReason::action_reference_missing;
    } else if (request.policy_decision.policy_reference_id.empty()) {
        reason = FaultControllerSchedulerReleaseGateActionIntentReason::policy_reference_missing;
    } else if (request.policy_decision.evidence_record_id.empty()) {
        reason = FaultControllerSchedulerReleaseGateActionIntentReason::evidence_record_missing;
    } else if (request.policy_decision.abort_required) {
        reason = FaultControllerSchedulerReleaseGateActionIntentReason::abort_action_prepared;
    } else if (request.policy_decision.release_blocked) {
        reason = FaultControllerSchedulerReleaseGateActionIntentReason::block_release_action_prepared;
    } else if (request.policy_decision.merge_blocked) {
        reason = FaultControllerSchedulerReleaseGateActionIntentReason::block_merge_action_prepared;
    } else if (request.policy_decision.review_required) {
        reason = FaultControllerSchedulerReleaseGateActionIntentReason::review_required_action_prepared;
    }

    if (request.policy_decision.abort_required
        && (request.policy_decision.review_required || request.policy_decision.merge_blocked || request.policy_decision.release_blocked)) {
        reason = FaultControllerSchedulerReleaseGateActionIntentReason::unsupported_action_combination;
    }

    auto status = FaultControllerSchedulerReleaseGateActionIntentStatus::blocked;
    if (reason == FaultControllerSchedulerReleaseGateActionIntentReason::pass_no_action) {
        status = FaultControllerSchedulerReleaseGateActionIntentStatus::prepared;
    } else if (reason == FaultControllerSchedulerReleaseGateActionIntentReason::review_required_action_prepared
        || reason == FaultControllerSchedulerReleaseGateActionIntentReason::block_merge_action_prepared
        || reason == FaultControllerSchedulerReleaseGateActionIntentReason::block_release_action_prepared
        || reason == FaultControllerSchedulerReleaseGateActionIntentReason::abort_action_prepared) {
        status = FaultControllerSchedulerReleaseGateActionIntentStatus::prepared;
    } else if (reason == FaultControllerSchedulerReleaseGateActionIntentReason::unsupported_action_combination) {
        status = FaultControllerSchedulerReleaseGateActionIntentStatus::review_required;
    }

    const bool action_prepared = status == FaultControllerSchedulerReleaseGateActionIntentStatus::prepared
        && reason != FaultControllerSchedulerReleaseGateActionIntentReason::pass_no_action;
    return FaultControllerSchedulerReleaseGateActionIntent{
        .request = request,
        .status = status,
        .reason = reason,
        .requested_control_kind = request.policy_decision.requested_control_kind,
        .target_phase = request.policy_decision.target_phase,
        .mutation_generation_before = request.policy_decision.mutation_generation_before,
        .mutation_generation_after = request.policy_decision.mutation_generation_after,
        .evidence_record_id = request.policy_decision.evidence_record_id,
        .policy_reference_id = request.policy_decision.policy_reference_id,
        .action_reference_id = action_prepared ? request.action_reference_id : "",
        .review_required = request.policy_decision.review_required,
        .merge_blocked = request.policy_decision.merge_blocked,
        .release_blocked = request.policy_decision.release_blocked,
        .abort_required = request.policy_decision.abort_required,
        .action_intent_prepared = action_prepared,
        .release_gate_action_emitted = false,
        .external_write_attempted = request.policy_decision.external_write_attempted || request.runtime_write.write_attempted,
        .adapter_call_attempted = request.policy_decision.adapter_call_attempted,
    };
}

FaultControllerSchedulerReleaseGateActionEmissionResult emit_fault_controller_scheduler_release_gate_action(
    const FaultControllerSchedulerReleaseGateActionEmissionRequest& request) {
    auto reason = FaultControllerSchedulerReleaseGateActionEmissionReason::action_emitted;
    if (!request.action_intent.action_intent_prepared
        || request.action_intent.status != FaultControllerSchedulerReleaseGateActionIntentStatus::prepared) {
        reason = request.action_intent.status == FaultControllerSchedulerReleaseGateActionIntentStatus::review_required
            ? FaultControllerSchedulerReleaseGateActionEmissionReason::review_required
            : FaultControllerSchedulerReleaseGateActionEmissionReason::action_intent_not_prepared;
    } else if (!request.emitter_boundary_available) {
        reason = FaultControllerSchedulerReleaseGateActionEmissionReason::emitter_boundary_absent;
    } else if (!request.action_sink_available) {
        reason = FaultControllerSchedulerReleaseGateActionEmissionReason::action_sink_missing;
    } else if (request.action_transaction_id.empty()) {
        reason = FaultControllerSchedulerReleaseGateActionEmissionReason::action_transaction_missing;
    } else if (request.idempotency_key.empty()) {
        reason = FaultControllerSchedulerReleaseGateActionEmissionReason::idempotency_key_missing;
    } else if (request.action_payload_reference_id.empty()) {
        reason = FaultControllerSchedulerReleaseGateActionEmissionReason::action_payload_missing;
    } else if (!request.action_payload_schema_valid) {
        reason = FaultControllerSchedulerReleaseGateActionEmissionReason::action_payload_schema_invalid;
    } else if (request.action_payload_content_hash.empty()) {
        reason = FaultControllerSchedulerReleaseGateActionEmissionReason::action_payload_hash_missing;
    } else if (!request.emission_dry_run && !request.emission_commit) {
        reason = FaultControllerSchedulerReleaseGateActionEmissionReason::emission_mode_missing;
    } else if (request.emission_dry_run && request.emission_commit) {
        reason = FaultControllerSchedulerReleaseGateActionEmissionReason::conflicting_emission_mode;
    } else if (request.emission_dry_run) {
        reason = FaultControllerSchedulerReleaseGateActionEmissionReason::dry_run_recorded;
    } else if (!request.action_sink_emit_succeeded) {
        reason = FaultControllerSchedulerReleaseGateActionEmissionReason::action_emit_failed;
    }

    auto status = FaultControllerSchedulerReleaseGateActionEmissionStatus::blocked;
    if (reason == FaultControllerSchedulerReleaseGateActionEmissionReason::dry_run_recorded) {
        status = FaultControllerSchedulerReleaseGateActionEmissionStatus::dry_run_recorded;
    } else if (reason == FaultControllerSchedulerReleaseGateActionEmissionReason::action_emitted) {
        status = FaultControllerSchedulerReleaseGateActionEmissionStatus::emitted;
    } else if (reason == FaultControllerSchedulerReleaseGateActionEmissionReason::action_emit_failed) {
        status = FaultControllerSchedulerReleaseGateActionEmissionStatus::failed;
    } else if (reason == FaultControllerSchedulerReleaseGateActionEmissionReason::review_required) {
        status = FaultControllerSchedulerReleaseGateActionEmissionStatus::review_required;
    }

    const bool emit_attempted = request.emission_commit
        && status != FaultControllerSchedulerReleaseGateActionEmissionStatus::blocked
        && status != FaultControllerSchedulerReleaseGateActionEmissionStatus::review_required;
    return FaultControllerSchedulerReleaseGateActionEmissionResult{
        .request = request,
        .status = status,
        .reason = reason,
        .requested_control_kind = request.action_intent.requested_control_kind,
        .target_phase = request.action_intent.target_phase,
        .mutation_generation_before = request.action_intent.mutation_generation_before,
        .mutation_generation_after = request.action_intent.mutation_generation_after,
        .evidence_record_id = request.action_intent.evidence_record_id,
        .policy_reference_id = request.action_intent.policy_reference_id,
        .action_reference_id = status == FaultControllerSchedulerReleaseGateActionEmissionStatus::blocked ? "" : request.action_intent.action_reference_id,
        .action_transaction_id = request.action_transaction_id,
        .idempotency_key = request.idempotency_key,
        .action_payload_reference_id = request.action_payload_reference_id,
        .action_payload_content_hash = request.action_payload_content_hash,
        .review_required = request.action_intent.review_required || status == FaultControllerSchedulerReleaseGateActionEmissionStatus::review_required,
        .merge_blocked = request.action_intent.merge_blocked,
        .release_blocked = request.action_intent.release_blocked,
        .abort_required = request.action_intent.abort_required,
        .action_emit_attempted = emit_attempted,
        .action_dry_run_recorded = status == FaultControllerSchedulerReleaseGateActionEmissionStatus::dry_run_recorded,
        .action_emitted = status == FaultControllerSchedulerReleaseGateActionEmissionStatus::emitted,
        .action_emit_failed = status == FaultControllerSchedulerReleaseGateActionEmissionStatus::failed,
        .external_write_attempted = request.action_intent.external_write_attempted,
        .adapter_call_attempted = request.action_intent.adapter_call_attempted,
    };
}

FaultControllerSchedulerReleaseGateActionCompletionRecord complete_fault_controller_scheduler_release_gate_action(
    const FaultControllerSchedulerReleaseGateActionEmissionResult& emission_result) {
    auto reason = FaultControllerSchedulerReleaseGateActionCompletionReason::action_emission_complete;
    if (emission_result.status == FaultControllerSchedulerReleaseGateActionEmissionStatus::review_required) {
        reason = FaultControllerSchedulerReleaseGateActionCompletionReason::review_required;
    } else if (emission_result.status == FaultControllerSchedulerReleaseGateActionEmissionStatus::failed) {
        reason = FaultControllerSchedulerReleaseGateActionCompletionReason::emission_failed;
    } else if (emission_result.status == FaultControllerSchedulerReleaseGateActionEmissionStatus::dry_run_recorded) {
        reason = FaultControllerSchedulerReleaseGateActionCompletionReason::dry_run_only;
    } else if (emission_result.status != FaultControllerSchedulerReleaseGateActionEmissionStatus::emitted) {
        reason = FaultControllerSchedulerReleaseGateActionCompletionReason::emission_not_completed;
    } else if (emission_result.action_reference_id.empty()) {
        reason = FaultControllerSchedulerReleaseGateActionCompletionReason::action_reference_missing;
    } else if (emission_result.action_transaction_id.empty()) {
        reason = FaultControllerSchedulerReleaseGateActionCompletionReason::action_transaction_missing;
    } else if (emission_result.abort_required
        && (emission_result.review_required || emission_result.merge_blocked || emission_result.release_blocked)) {
        reason = FaultControllerSchedulerReleaseGateActionCompletionReason::unsupported_action_combination;
    } else if (!emission_result.review_required
        && !emission_result.merge_blocked
        && !emission_result.release_blocked
        && !emission_result.abort_required) {
        reason = FaultControllerSchedulerReleaseGateActionCompletionReason::pass_no_action_complete;
    }

    auto status = FaultControllerSchedulerReleaseGateActionCompletionStatus::incomplete;
    if (reason == FaultControllerSchedulerReleaseGateActionCompletionReason::action_emission_complete
        || reason == FaultControllerSchedulerReleaseGateActionCompletionReason::pass_no_action_complete) {
        status = FaultControllerSchedulerReleaseGateActionCompletionStatus::complete;
    } else if (reason == FaultControllerSchedulerReleaseGateActionCompletionReason::emission_failed) {
        status = FaultControllerSchedulerReleaseGateActionCompletionStatus::failed;
    } else if (reason == FaultControllerSchedulerReleaseGateActionCompletionReason::review_required
        || reason == FaultControllerSchedulerReleaseGateActionCompletionReason::unsupported_action_combination) {
        status = FaultControllerSchedulerReleaseGateActionCompletionStatus::review_required;
    }

    return FaultControllerSchedulerReleaseGateActionCompletionRecord{
        .status = status,
        .reason = reason,
        .requested_control_kind = emission_result.requested_control_kind,
        .target_phase = emission_result.target_phase,
        .mutation_generation_before = emission_result.mutation_generation_before,
        .mutation_generation_after = emission_result.mutation_generation_after,
        .evidence_record_id = emission_result.evidence_record_id,
        .policy_reference_id = emission_result.policy_reference_id,
        .action_reference_id = status == FaultControllerSchedulerReleaseGateActionCompletionStatus::complete ? emission_result.action_reference_id : "",
        .action_transaction_id = status == FaultControllerSchedulerReleaseGateActionCompletionStatus::complete ? emission_result.action_transaction_id : "",
        .review_required = emission_result.review_required || status == FaultControllerSchedulerReleaseGateActionCompletionStatus::review_required,
        .merge_blocked = emission_result.merge_blocked,
        .release_blocked = emission_result.release_blocked,
        .abort_required = emission_result.abort_required,
        .action_completed = status == FaultControllerSchedulerReleaseGateActionCompletionStatus::complete,
        .action_failed = status == FaultControllerSchedulerReleaseGateActionCompletionStatus::failed,
        .action_dry_run_only = reason == FaultControllerSchedulerReleaseGateActionCompletionReason::dry_run_only,
        .external_write_attempted = emission_result.external_write_attempted,
        .adapter_call_attempted = emission_result.adapter_call_attempted,
    };
}

FlowLimit compute_flow_limit(const ExchangeCellState& cell, double dt_sub) {
    if (!std::isfinite(dt_sub) || dt_sub <= 0.0) {
        throw std::invalid_argument("dt_sub must be finite and positive");
    }
    validate_exchange_cell_state(cell);

    const double v_limit = 0.9 * cell.phi_t * cell.h * cell.area;
    return FlowLimit{
        .v_limit = v_limit,
        .q_limit = v_limit / dt_sub,
    };
}

ExchangeDecision evaluate_exchange(const ExchangeCellState& cell, const ExchangeRequest& request) {
    if (!std::isfinite(request.q_request) || request.q_request < 0.0) {
        throw std::invalid_argument("q_request must be finite and non-negative");
    }
    validate_exchange_cell_state(cell);
    if (!cell.shared_deficit_accounts.empty()) {
        throw std::invalid_argument("aggregate exchange path does not support shared endpoint deficits");
    }

    const auto limit = compute_flow_limit(cell, request.dt_sub);
    const double q_repay = std::min(cell.mass_deficit_account.volume / request.dt_sub, limit.q_limit);
    const double q_granted = std::min(request.q_request, limit.q_limit - q_repay);
    const double v_granted = q_granted * request.dt_sub;
    const double v_repay = q_repay * request.dt_sub;
    const double v_unmet = (request.q_request - q_granted) * request.dt_sub;

    return ExchangeDecision{
        .q_granted = q_granted,
        .v_granted = v_granted,
        .q_repay = q_repay,
        .v_repay = v_repay,
        .v_unmet = v_unmet,
    };
}

std::vector<SharedExchangeDecision> evaluate_shared_exchange(
    const ExchangeCellState& cell,
    const std::vector<SharedExchangeIntent>& intents,
    double dt_sub) {
    if (!std::isfinite(dt_sub) || dt_sub <= 0.0) {
        throw std::invalid_argument("dt_sub must be finite and positive");
    }
    validate_exchange_cell_state(cell);
    if (intents.empty()) {
        return {};
    }

    double total_weight = 0.0;
    double weighted_request = 0.0;
    for (std::size_t i = 0; i < intents.size(); ++i) {
        const auto& intent = intents[i];
        if (!std::isfinite(intent.q_request) || intent.q_request < 0.0) {
            throw std::invalid_argument("q_request must be finite and non-negative");
        }
        if (!std::isfinite(intent.priority_weight) || intent.priority_weight <= 0.0) {
            throw std::invalid_argument("priority_weight must be finite and positive");
        }
        for (std::size_t j = i + 1; j < intents.size(); ++j) {
            if (same_endpoint(intent.endpoint, intents[j].endpoint)) {
                throw std::invalid_argument("shared exchange intent endpoints must be unique");
            }
        }
        total_weight += intent.priority_weight;
        weighted_request += intent.priority_weight * intent.q_request;
    }

    const auto limit = compute_flow_limit(cell, dt_sub);

    double total_q_repay = 0.0;
    std::vector<double> repayment_flows;
    repayment_flows.reserve(intents.size());
    for (const auto& intent : intents) {
        const double weight_fraction = intent.priority_weight / total_weight;
        const FlowLimit allocated_limit{
            .v_limit = weight_fraction * limit.v_limit,
            .q_limit = weight_fraction * limit.q_limit,
        };
        const double endpoint_repay_request = endpoint_deficit_account(cell, intent.endpoint).volume / dt_sub;
        const double q_repay = std::min(endpoint_repay_request, allocated_limit.q_limit);
        repayment_flows.push_back(q_repay);
        total_q_repay += q_repay;
    }

    const double remaining_q_limit = std::max(0.0, limit.q_limit - total_q_repay);
    const double grant_scale = weighted_request > 0.0
        ? std::clamp(remaining_q_limit / weighted_request, 0.0, 1.0)
        : 0.0;

    std::vector<SharedExchangeDecision> decisions;
    decisions.reserve(intents.size());
    for (std::size_t i = 0; i < intents.size(); ++i) {
        const auto& intent = intents[i];
        const double weight_fraction = intent.priority_weight / total_weight;
        const FlowLimit allocated_limit{
            .v_limit = weight_fraction * limit.v_limit,
            .q_limit = weight_fraction * limit.q_limit,
        };
        const double q_repay = repayment_flows[i];
        const double scaled_q = intent.priority_weight * intent.q_request * grant_scale;
        const double q_granted = std::min(
            intent.q_request,
            std::min(std::max(0.0, allocated_limit.q_limit - q_repay), scaled_q));
        decisions.push_back({
            .endpoint = intent.endpoint,
            .exchange = {
                .q_granted = q_granted,
                .v_granted = q_granted * dt_sub,
                .q_repay = q_repay,
                .v_repay = q_repay * dt_sub,
                .v_unmet = (intent.q_request - q_granted) * dt_sub,
            },
            .allocated_limit = allocated_limit,
            .priority_weight = intent.priority_weight,
        });
    }

    return decisions;
}

DrainSplit split_drain(const ExchangeCellState& cell, const ExchangeDecision& decision, double dt_sub) {
    if (!std::isfinite(dt_sub) || dt_sub <= 0.0) {
        throw std::invalid_argument("dt_sub must be finite and positive");
    }
    validate_exchange_cell_state(cell);
    validate_exchange_decision(decision);

    const double applied_volume = decision.v_granted + decision.v_repay;
    const double threshold = 0.2 * cell.phi_t * cell.h * cell.area;
    if (applied_volume <= threshold) {
        return DrainSplit{
            .micro_steps = 1,
            .dt_micro = dt_sub,
            .v_per_micro_step = applied_volume,
        };
    }
    if (threshold <= 0.0) {
        throw std::invalid_argument("geometric storage must be positive when applied volume is positive");
    }

    const int requested = static_cast<int>(std::ceil(applied_volume / threshold));
    const int micro_steps = std::min(5, requested);
    return DrainSplit{
        .micro_steps = micro_steps,
        .dt_micro = dt_sub / micro_steps,
        .v_per_micro_step = applied_volume / micro_steps,
    };
}

ExchangeDecision enforce_nonnegative_storage(
    const ExchangeCellState& cell,
    const ExchangeDecision& decision,
    double dt_sub) {
    if (!std::isfinite(dt_sub) || dt_sub <= 0.0) {
        throw std::invalid_argument("dt_sub must be finite and positive");
    }
    validate_exchange_cell_state(cell);
    validate_exchange_decision(decision);

    const double available_storage = cell.phi_t * cell.h * cell.area;
    if (decision.v_granted <= available_storage) {
        return decision;
    }

    const double additional_unmet = decision.v_granted - available_storage;
    return ExchangeDecision{
        .q_granted = available_storage / dt_sub,
        .v_granted = available_storage,
        .q_repay = decision.q_repay,
        .v_repay = decision.v_repay,
        .v_unmet = decision.v_unmet + additional_unmet,
    };
}

EngineInterfaceExchangeDecision evaluate_engine_interface_exchange(
    const EngineInterfaceExchangeRequest& request) {
    if (request.source.engine == request.target.engine) {
        throw std::invalid_argument(
            "engine interface exchange source and target must belong to different engines");
    }
    if (!std::isfinite(request.q_request) || request.q_request < 0.0) {
        throw std::invalid_argument(
            "engine interface exchange q_request must be finite and non-negative");
    }
    if (!std::isfinite(request.q_capacity) || request.q_capacity < 0.0) {
        throw std::invalid_argument(
            "engine interface exchange q_capacity must be finite and non-negative");
    }
    if (!std::isfinite(request.dt_sub) || request.dt_sub <= 0.0) {
        throw std::invalid_argument(
            "engine interface exchange dt_sub must be finite and positive");
    }

    const double q_granted =
        request.q_request < request.q_capacity ? request.q_request : request.q_capacity;
    return EngineInterfaceExchangeDecision{
        .source = request.source,
        .target = request.target,
        .q_granted = q_granted,
        .v_granted = q_granted * request.dt_sub,
        .v_unmet = (request.q_request - q_granted) * request.dt_sub,
    };
}

HeadDrivenExchangeFlow compute_head_driven_exchange_flow(
    double surface_water_level,
    double engine_water_level,
    const ExchangeFlowGeometry& geometry) {
    if (!std::isfinite(surface_water_level) || !std::isfinite(engine_water_level)) {
        throw std::invalid_argument(
            "head driven exchange water levels must be finite");
    }
    if (!std::isfinite(geometry.crest_level)) {
        throw std::invalid_argument("head driven exchange crest_level must be finite");
    }
    if (!std::isfinite(geometry.exchange_width) || geometry.exchange_width <= 0.0) {
        throw std::invalid_argument(
            "head driven exchange exchange_width must be finite and positive");
    }
    if (!std::isfinite(geometry.weir_coefficient) || geometry.weir_coefficient <= 0.0) {
        throw std::invalid_argument(
            "head driven exchange weir_coefficient must be finite and positive");
    }
    if (!std::isfinite(geometry.orifice_coefficient) || geometry.orifice_coefficient <= 0.0) {
        throw std::invalid_argument(
            "head driven exchange orifice_coefficient must be finite and positive");
    }
    if (!std::isfinite(geometry.orifice_area) || geometry.orifice_area < 0.0) {
        throw std::invalid_argument(
            "head driven exchange orifice_area must be finite and non-negative");
    }
    if (!std::isfinite(geometry.smooth_depth) || geometry.smooth_depth < 0.0) {
        throw std::invalid_argument(
            "head driven exchange smooth_depth must be finite and non-negative");
    }

    constexpr double kGravity = 9.80665;

    const bool surface_is_upstream = surface_water_level >= engine_water_level;
    const double h_up =
        (surface_is_upstream ? surface_water_level : engine_water_level) -
        geometry.crest_level;
    const double h_down =
        (surface_is_upstream ? engine_water_level : surface_water_level) -
        geometry.crest_level;

    if (h_up <= 0.0 || surface_water_level == engine_water_level) {
        return HeadDrivenExchangeFlow{};
    }

    double magnitude = 0.0;
    HeadDrivenExchangeRegime regime = HeadDrivenExchangeRegime::none;
    if (h_down > 0.0 && geometry.orifice_area > 0.0) {
        // Fully submerged opening (gate_outlet / culvert_link): orifice flow.
        magnitude = geometry.orifice_coefficient * geometry.orifice_area *
            std::sqrt(2.0 * kGravity * (h_up - h_down));
        regime = HeadDrivenExchangeRegime::orifice;
    } else {
        magnitude = geometry.weir_coefficient * geometry.exchange_width *
            std::pow(h_up, 1.5);
        regime = HeadDrivenExchangeRegime::free_weir;
        if (h_down > 0.0) {
            // Villemonte submerged-weir reduction.
            const double submergence = h_down / h_up;
            magnitude *= std::pow(1.0 - std::pow(submergence, 1.5), 0.385);
            regime = HeadDrivenExchangeRegime::submerged_weir;
        }
    }

    if (geometry.smooth_depth > 0.0 && h_up < geometry.smooth_depth) {
        // Smoothstep incipient transition: continuous activation near the
        // crest avoids on/off flickering of the exchange.
        const double t = h_up / geometry.smooth_depth;
        magnitude *= t * t * (3.0 - 2.0 * t);
    }

    return HeadDrivenExchangeFlow{
        .q_surface_to_engine = surface_is_upstream ? magnitude : -magnitude,
        .regime = regime,
    };
}

ExchangePipelineDecision evaluate_exchange_pipeline(
    const ExchangeCellState& cell,
    const ExchangeRequest& request) {
    const auto initial = evaluate_exchange(cell, request);
    const auto nonnegative = enforce_nonnegative_storage(cell, initial, request.dt_sub);
    const auto split = split_drain(cell, nonnegative, request.dt_sub);

    return ExchangePipelineDecision{
        .exchange = nonnegative,
        .drain_split = split,
        .drain_split_engaged = split.micro_steps > 1,
        .negative_depth_fix_engaged = nonnegative.v_granted < initial.v_granted,
    };
}

SharedExchangePipelineDecision evaluate_shared_exchange_pipeline(
    const ExchangeCellState& cell,
    const std::vector<SharedExchangeIntent>& intents,
    double dt_sub) {
    const auto initial = evaluate_shared_exchange(cell, intents, dt_sub);
    if (initial.empty()) {
        return {};
    }

    double v_granted = 0.0;
    double v_repay = 0.0;
    for (const auto& decision : initial) {
        v_granted += decision.exchange.v_granted;
        v_repay += decision.exchange.v_repay;
    }

    const ExchangeDecision aggregate{
        .v_granted = v_granted,
        .v_repay = v_repay,
    };
    const auto split = split_drain(cell, aggregate, dt_sub);

    return SharedExchangePipelineDecision{
        .decisions = initial,
        .drain_split = split,
        .drain_split_engaged = split.micro_steps > 1,
        .negative_depth_fix_engaged = false,
    };
}

ExchangeConservationAudit audit_exchange_decision(
    const ExchangeRequest& request,
    const ExchangePipelineDecision& decision) {
    if (!std::isfinite(request.dt_sub) || request.dt_sub <= 0.0) {
        throw std::invalid_argument("dt_sub must be finite and positive");
    }
    if (!std::isfinite(request.q_request) || request.q_request < 0.0) {
        throw std::invalid_argument("q_request must be finite and non-negative");
    }
    validate_exchange_decision(decision.exchange);

    const double request_volume = request.q_request * request.dt_sub;
    const double accounted_volume = decision.exchange.v_granted + decision.exchange.v_unmet;
    const double residual = request_volume - accounted_volume;

    return ExchangeConservationAudit{
        .request_volume = request_volume,
        .accounted_volume = accounted_volume,
        .residual = residual,
        .balanced = (residual == 0.0),
    };
}

SystemMassAudit compute_system_mass(
    const std::vector<ExchangeCellState>& cells,
    double h_wet) {
    if (!std::isfinite(h_wet) || h_wet <= 0.0) {
        throw std::invalid_argument("h_wet must be finite and positive");
    }

    SystemMassAudit audit{};
    for (const auto& cell : cells) {
        validate_exchange_cell_state(cell);

        if (cell.h >= h_wet) {
            audit.surface_mass += cell.phi_t * cell.h * cell.area;
            ++audit.wet_cell_count;
        }
        audit.deficit_mass += cell.mass_deficit_account.volume;
        for (const auto& shared_deficit : cell.shared_deficit_accounts) {
            audit.deficit_mass += shared_deficit.mass_deficit_account.volume;
        }
    }
    audit.total_mass = audit.surface_mass + audit.deficit_mass;
    return audit;
}

SystemMassDelta audit_system_mass_against_reference(
    const SystemMassAudit& baseline,
    const std::vector<ExchangeCellState>& current_cells,
    double h_wet) {
    if (!std::isfinite(baseline.surface_mass) || baseline.surface_mass < 0.0 ||
        !std::isfinite(baseline.deficit_mass) || baseline.deficit_mass < 0.0 ||
        !std::isfinite(baseline.total_mass) || baseline.total_mass < 0.0) {
        throw std::invalid_argument("baseline mass fields must be finite and non-negative");
    }

    const auto current = compute_system_mass(current_cells, h_wet);
    const double residual = current.total_mass - baseline.total_mass;
    return SystemMassDelta{
        .baseline = baseline,
        .current = current,
        .residual = residual,
        .conserved = (residual == 0.0),
    };
}

SystemMassConservationStatus classify_system_mass_conservation(const SystemMassDelta& delta) {
    if (delta.conserved) {
        return SystemMassConservationStatus::conserved;
    }
    return SystemMassConservationStatus::drifted;
}

SystemMassConservationDiagnostic make_system_mass_conservation_diagnostic(
    const SystemMassDelta& delta) {
    return SystemMassConservationDiagnostic{
        .status = classify_system_mass_conservation(delta),
        .residual = delta.residual,
        .baseline_total_mass = delta.baseline.total_mass,
        .current_total_mass = delta.current.total_mass,
    };
}

SystemMassGateDecision decide_system_mass_gate_action(
    const SystemMassConservationDiagnostic& diagnostic) {
    if (diagnostic.status == SystemMassConservationStatus::conserved) {
        return SystemMassGateDecision{
            .action = SystemMassGateAction::continue_run,
            .diagnostic = diagnostic,
        };
    }
    return SystemMassGateDecision{
        .action = SystemMassGateAction::abort_run,
        .diagnostic = diagnostic,
    };
}

SystemMassRuntimeGateOutcome make_system_mass_runtime_gate_outcome(
    const SystemMassGateDecision& decision) {
    return SystemMassRuntimeGateOutcome{
        .decision = decision,
        .status = decision.action == SystemMassGateAction::abort_run
            ? SystemMassRuntimeGateStatus::abort
            : SystemMassRuntimeGateStatus::running,
    };
}

SystemMassRuntimeGateOutcome make_system_mass_runtime_gate_outcome(
    const SystemMassConservationDiagnostic& diagnostic) {
    return make_system_mass_runtime_gate_outcome(
        decide_system_mass_gate_action(diagnostic));
}

SystemMassRuntimeAbortHandlingState classify_system_mass_runtime_abort_handling(
    const SystemMassRuntimeGateOutcome& outcome) {
    if (outcome.status == SystemMassRuntimeGateStatus::abort) {
        return SystemMassRuntimeAbortHandlingState::abort;
    }
    return SystemMassRuntimeAbortHandlingState::continue_run;
}

bool should_abort_system_mass_runtime(SystemMassRuntimeAbortHandlingState handling_state) {
    return handling_state == SystemMassRuntimeAbortHandlingState::abort;
}

bool should_abort_system_mass_runtime(const SystemMassConservationDiagnostic& diagnostic) {
    return make_system_mass_runtime_control_decision(diagnostic).should_abort;
}

SystemMassRuntimeControlDecision make_system_mass_runtime_control_decision(
    const SystemMassRuntimeGateOutcome& gate_outcome) {
    const auto handling_state = classify_system_mass_runtime_abort_handling(gate_outcome);
    return SystemMassRuntimeControlDecision{
        .gate_outcome = gate_outcome,
        .handling_state = handling_state,
        .should_abort = should_abort_system_mass_runtime(handling_state),
    };
}

SystemMassRuntimeControlDecision make_system_mass_runtime_control_decision(
    const SystemMassConservationDiagnostic& diagnostic) {
    return make_system_mass_runtime_control_decision(
        make_system_mass_runtime_gate_outcome(diagnostic));
}

SystemMassRuntimeControlResult consume_system_mass_runtime_control_decision(
    const SystemMassRuntimeControlDecision& decision) {
    return SystemMassRuntimeControlResult{
        .decision = decision,
        .state = decision.should_abort
            ? SystemMassRuntimeControlState::abort
            : SystemMassRuntimeControlState::continue_run,
    };
}

SystemMassRuntimeOperatorAction make_system_mass_runtime_operator_action(
    const SystemMassRuntimeControlResult& control_result) {
    return SystemMassRuntimeOperatorAction{
        .control_result = control_result,
        .state = control_result.state == SystemMassRuntimeControlState::abort
            ? SystemMassRuntimeOperatorActionState::abort_requested
            : SystemMassRuntimeOperatorActionState::continue_run,
    };
}

MassDeficitAccount roll_deficit(const MassDeficitAccount& account, double unmet_volume) {
    if (!std::isfinite(account.volume) || account.volume < 0.0) {
        throw std::invalid_argument("deficit volume must be finite and non-negative");
    }
    if (!std::isfinite(unmet_volume) || unmet_volume < 0.0) {
        throw std::invalid_argument("unmet volume must be finite and non-negative");
    }

    const double updated_volume = account.volume + unmet_volume;
    if (!std::isfinite(updated_volume)) {
        throw std::invalid_argument("updated deficit volume must be finite");
    }
    return MassDeficitAccount{.volume = updated_volume};
}

MassDeficitAccount apply_repayment(const MassDeficitAccount& account, double applied_volume) {
    if (!std::isfinite(account.volume) || account.volume < 0.0) {
        throw std::invalid_argument("deficit volume must be finite and non-negative");
    }
    if (!std::isfinite(applied_volume) || applied_volume < 0.0) {
        throw std::invalid_argument("applied volume must be finite and non-negative");
    }

    return MassDeficitAccount{.volume = std::max(0.0, account.volume - applied_volume)};
}

CouplingSnapshot::CouplingSnapshot(
    std::vector<ExchangeCellState> cells,
    RuntimeCounters counters,
    std::vector<CouplingEvent> pending_events)
    : cells_(std::move(cells)),
      runtime_counters_(counters),
      pending_events_(std::move(pending_events)) {}

const std::vector<ExchangeCellState>& CouplingSnapshot::cells() const noexcept {
    return cells_;
}

const RuntimeCounters& CouplingSnapshot::runtime_counters() const noexcept {
    return runtime_counters_;
}

const std::vector<CouplingEvent>& CouplingSnapshot::pending_events() const noexcept {
    return pending_events_;
}

SystemMassAudit CouplingSnapshot::compute_system_mass(double h_wet) const {
    return core::compute_system_mass(cells_, h_wet);
}

CouplingState::CouplingState(std::vector<ExchangeCellState> cells) : cells_(std::move(cells)) {
    for (const auto& cell : cells_) {
        validate_exchange_cell_state(cell);
    }
}

const std::vector<ExchangeCellState>& CouplingState::cells() const noexcept {
    return cells_;
}

const RuntimeCounters& CouplingState::runtime_counters() const noexcept {
    return runtime_counters_;
}

CouplingSnapshot CouplingState::snapshot() const {
    return CouplingSnapshot{cells_, runtime_counters_, pending_events_};
}

SystemMassAudit CouplingState::compute_system_mass(double h_wet) const {
    return core::compute_system_mass(cells_, h_wet);
}

SystemMassDelta CouplingState::audit_system_mass_against_reference(
    const SystemMassAudit& baseline,
    double h_wet) const {
    return core::audit_system_mass_against_reference(baseline, cells_, h_wet);
}

SystemMassConservationDiagnostic CouplingState::diagnose_system_mass_against_reference(
    const SystemMassAudit& baseline,
    double h_wet) const {
    return make_system_mass_conservation_diagnostic(
        audit_system_mass_against_reference(baseline, h_wet));
}

SystemMassGateDecision CouplingState::decide_system_mass_gate_action_against_reference(
    const SystemMassAudit& baseline,
    double h_wet) const {
    return core::decide_system_mass_gate_action(
        diagnose_system_mass_against_reference(baseline, h_wet));
}

SystemMassRuntimeGateOutcome CouplingState::evaluate_system_mass_runtime_gate_against_reference(
    const SystemMassAudit& baseline,
    double h_wet) const {
    return make_system_mass_runtime_gate_outcome(
        decide_system_mass_gate_action_against_reference(baseline, h_wet));
}

SystemMassRuntimeControlDecision CouplingState::decide_system_mass_runtime_control_against_reference(
    const SystemMassAudit& baseline,
    double h_wet) const {
    return make_system_mass_runtime_control_decision(
        evaluate_system_mass_runtime_gate_against_reference(baseline, h_wet));
}

bool CouplingState::should_abort_system_mass_runtime_control_against_reference(
    const SystemMassAudit& baseline,
    double h_wet) const {
    return decide_system_mass_runtime_control_against_reference(baseline, h_wet).should_abort;
}

bool CouplingState::should_abort_system_mass_runtime_against_reference(
    const SystemMassAudit& baseline,
    double h_wet) const {
    return should_abort_system_mass_runtime(
        diagnose_system_mass_against_reference(baseline, h_wet));
}

SystemMassDelta CouplingState::audit_system_mass_against_snapshot(
    const CouplingSnapshot& baseline,
    double h_wet) const {
    return audit_system_mass_against_reference(baseline.compute_system_mass(h_wet), h_wet);
}

SystemMassConservationDiagnostic CouplingState::diagnose_system_mass_against_snapshot(
    const CouplingSnapshot& baseline,
    double h_wet) const {
    return make_system_mass_conservation_diagnostic(
        audit_system_mass_against_snapshot(baseline, h_wet));
}

SystemMassGateDecision CouplingState::decide_system_mass_gate_action_against_snapshot(
    const CouplingSnapshot& baseline,
    double h_wet) const {
    return core::decide_system_mass_gate_action(
        diagnose_system_mass_against_snapshot(baseline, h_wet));
}

SystemMassRuntimeGateOutcome CouplingState::evaluate_system_mass_runtime_gate_against_snapshot(
    const CouplingSnapshot& baseline,
    double h_wet) const {
    return make_system_mass_runtime_gate_outcome(
        diagnose_system_mass_against_snapshot(baseline, h_wet));
}

bool CouplingState::should_abort_system_mass_runtime_against_snapshot(
    const CouplingSnapshot& baseline,
    double h_wet) const {
    return should_abort_system_mass_runtime(
        diagnose_system_mass_against_snapshot(baseline, h_wet));
}

SystemMassRuntimeControlDecision CouplingState::decide_system_mass_runtime_control_against_snapshot(
    const CouplingSnapshot& baseline,
    double h_wet) const {
    return make_system_mass_runtime_control_decision(
        evaluate_system_mass_runtime_gate_against_snapshot(baseline, h_wet));
}

bool CouplingState::should_abort_system_mass_runtime_control_against_snapshot(
    const CouplingSnapshot& baseline,
    double h_wet) const {
    return decide_system_mass_runtime_control_against_snapshot(baseline, h_wet).should_abort;
}

void CouplingState::enqueue_event(CouplingEvent event) {
    if (event.exchange_cell_index >= cells_.size()) {
        throw std::out_of_range("coupling event exchange cell index is out of range");
    }
    if (!std::isfinite(event.volume_delta) || event.volume_delta < 0.0) {
        throw std::invalid_argument("coupling event volume_delta must be finite and non-negative");
    }
    if (!std::isfinite(event.unmet_volume) || event.unmet_volume < 0.0) {
        throw std::invalid_argument("coupling event unmet_volume must be finite and non-negative");
    }
    if (!std::isfinite(event.repayment_volume) || event.repayment_volume < 0.0) {
        throw std::invalid_argument("coupling event repayment_volume must be finite and non-negative");
    }
    double endpoint_unmet_volume = 0.0;
    double endpoint_repayment_volume = 0.0;
    for (std::size_t i = 0; i < event.shared_endpoint_events.size(); ++i) {
        const auto& endpoint_event = event.shared_endpoint_events[i];
        if (!std::isfinite(endpoint_event.unmet_volume) || endpoint_event.unmet_volume < 0.0) {
            throw std::invalid_argument("shared endpoint unmet_volume must be finite and non-negative");
        }
        if (!std::isfinite(endpoint_event.repayment_volume) || endpoint_event.repayment_volume < 0.0) {
            throw std::invalid_argument("shared endpoint repayment_volume must be finite and non-negative");
        }
        for (std::size_t j = i + 1; j < event.shared_endpoint_events.size(); ++j) {
            if (same_endpoint(endpoint_event.endpoint, event.shared_endpoint_events[j].endpoint)) {
                throw std::invalid_argument("shared endpoint event endpoints must be unique");
            }
        }
        endpoint_unmet_volume += endpoint_event.unmet_volume;
        endpoint_repayment_volume += endpoint_event.repayment_volume;
    }
    if (!event.shared_endpoint_events.empty()) {
        if (std::abs(endpoint_unmet_volume - event.unmet_volume) > 1.0e-12) {
            throw std::invalid_argument("shared endpoint unmet volumes must match aggregate unmet_volume");
        }
        if (std::abs(endpoint_repayment_volume - event.repayment_volume) > 1.0e-12) {
            throw std::invalid_argument("shared endpoint repayment volumes must match aggregate repayment_volume");
        }
    }
    pending_events_.push_back(event);
}

void CouplingState::rollback(const CouplingSnapshot& snapshot) {
    cells_ = snapshot.cells_;
    runtime_counters_ = snapshot.runtime_counters_;
    pending_events_ = snapshot.pending_events_;
}

void CouplingState::replay_pending() {
    auto replayed_cells = cells_;
    for (const auto& event : pending_events_) {
        auto& cell = replayed_cells[event.exchange_cell_index];
        apply_volume_delta_to_cell(cell, event.direction, event.volume_delta);
        if (event.shared_endpoint_events.empty()) {
            cell.mass_deficit_account = roll_deficit(cell.mass_deficit_account, event.unmet_volume);
            cell.mass_deficit_account = apply_repayment(cell.mass_deficit_account, event.repayment_volume);
        } else {
            for (const auto& endpoint_event : event.shared_endpoint_events) {
                auto endpoint_account = endpoint_deficit_account(cell, endpoint_event.endpoint);
                endpoint_account = roll_deficit(endpoint_account, endpoint_event.unmet_volume);
                endpoint_account = apply_repayment(endpoint_account, endpoint_event.repayment_volume);
                upsert_endpoint_deficit_account(cell, endpoint_event.endpoint, endpoint_account);
            }
        }
        validate_exchange_cell_state(cell);
    }

    cells_ = std::move(replayed_cells);
    pending_events_.clear();
}

void CouplingState::record_pipeline_decision(const ExchangePipelineDecision& decision) {
    if (decision.drain_split_engaged) {
        ++runtime_counters_.count_drain_split;
    }
    if (decision.negative_depth_fix_engaged) {
        ++runtime_counters_.count_negative_depth_fix;
    }
}

ExchangePipelineDecision CouplingState::apply_exchange(
    std::size_t cell_index,
    const ExchangeRequest& request) {
    if (cell_index >= cells_.size()) {
        throw std::out_of_range("apply_exchange cell index is out of range");
    }

    const auto decision = evaluate_exchange_pipeline(cells_[cell_index], request);

    enqueue_event(CouplingEvent{
        .exchange_cell_index = cell_index,
        .direction = ExchangeDirection::surface_to_engine,
        .volume_delta = decision.exchange.v_granted + decision.exchange.v_repay,
        .unmet_volume = decision.exchange.v_unmet,
        .repayment_volume = decision.exchange.v_repay,
    });

    record_pipeline_decision(decision);

    return decision;
}

ReturnExchangeDecision CouplingState::apply_return_exchange(
    std::size_t cell_index,
    const ReturnExchangeRequest& request) {
    if (cell_index >= cells_.size()) {
        throw std::out_of_range("apply_return_exchange cell index is out of range");
    }
    if (!std::isfinite(request.q_return) || request.q_return < 0.0) {
        throw std::invalid_argument(
            "return exchange q_return must be finite and non-negative");
    }
    if (!std::isfinite(request.dt_sub) || request.dt_sub <= 0.0) {
        throw std::invalid_argument("return exchange dt_sub must be finite and positive");
    }

    const double v_returned = request.q_return * request.dt_sub;
    if (v_returned > 0.0) {
        enqueue_event(CouplingEvent{
            .exchange_cell_index = cell_index,
            .direction = ExchangeDirection::engine_to_surface,
            .volume_delta = v_returned,
        });
    }

    return ReturnExchangeDecision{
        .source = request.source,
        .q_returned = request.q_return,
        .v_returned = v_returned,
    };
}

std::vector<SharedExchangeDecision> CouplingState::apply_shared_exchange(
    std::size_t cell_index,
    const std::vector<SharedExchangeIntent>& intents,
    double dt_sub) {
    if (cell_index >= cells_.size()) {
        throw std::out_of_range("apply_shared_exchange cell index is out of range");
    }

    const auto pipeline = evaluate_shared_exchange_pipeline(cells_[cell_index], intents, dt_sub);
    if (pipeline.decisions.empty()) {
        return pipeline.decisions;
    }
    const auto decisions = pipeline.decisions;

    double volume_delta = 0.0;
    double unmet_volume = 0.0;
    double repayment_volume = 0.0;
    std::vector<SharedExchangeEndpointEvent> endpoint_events;
    endpoint_events.reserve(decisions.size());
    for (const auto& decision : decisions) {
        volume_delta += decision.exchange.v_granted + decision.exchange.v_repay;
        unmet_volume += decision.exchange.v_unmet;
        repayment_volume += decision.exchange.v_repay;
        endpoint_events.push_back({
            .endpoint = decision.endpoint,
            .unmet_volume = decision.exchange.v_unmet,
            .repayment_volume = decision.exchange.v_repay,
        });
    }

    enqueue_event(CouplingEvent{
        .exchange_cell_index = cell_index,
        .direction = ExchangeDirection::surface_to_engine,
        .volume_delta = volume_delta,
        .unmet_volume = unmet_volume,
        .repayment_volume = repayment_volume,
        .shared_endpoint_events = std::move(endpoint_events),
    });

    if (pipeline.drain_split_engaged) {
        ++runtime_counters_.count_drain_split;
    }
    if (pipeline.negative_depth_fix_engaged) {
        ++runtime_counters_.count_negative_depth_fix;
    }

    return decisions;
}

MockCouplingSchedulerStepResult CouplingState::run_mock_coupling_scheduler_step(
    std::size_t cell_index,
    const std::vector<SharedExchangeIntent>& intents,
    double dt_sub) {
    auto decisions = apply_shared_exchange(cell_index, intents, dt_sub);
    replay_pending();
    return MockCouplingSchedulerStepResult{.decisions = std::move(decisions)};
}

}  // namespace scau::coupling::core

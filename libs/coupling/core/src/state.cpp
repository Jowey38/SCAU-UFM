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
    for (const auto& shared_deficit : cell.shared_deficit_accounts) {
        if (!std::isfinite(shared_deficit.mass_deficit_account.volume) ||
            shared_deficit.mass_deficit_account.volume < 0.0) {
            throw std::invalid_argument("shared deficit volume must be finite and non-negative");
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
    for (const auto& intent : intents) {
        if (!std::isfinite(intent.q_request) || intent.q_request < 0.0) {
            throw std::invalid_argument("q_request must be finite and non-negative");
        }
        if (!std::isfinite(intent.priority_weight) || intent.priority_weight <= 0.0) {
            throw std::invalid_argument("priority_weight must be finite and positive");
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
    if (request.dt_sub <= 0.0) {
        throw std::invalid_argument("dt_sub must be positive");
    }
    if (request.q_request < 0.0) {
        throw std::invalid_argument("q_request must be non-negative");
    }
    if (decision.exchange.v_granted < 0.0) {
        throw std::invalid_argument("v_granted must be non-negative");
    }
    if (decision.exchange.v_unmet < 0.0) {
        throw std::invalid_argument("v_unmet must be non-negative");
    }

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
    if (h_wet <= 0.0) {
        throw std::invalid_argument("h_wet must be positive");
    }

    SystemMassAudit audit{};
    for (const auto& cell : cells) {
        if (cell.phi_t < 0.0) {
            throw std::invalid_argument("phi_t must be non-negative");
        }
        if (cell.h < 0.0) {
            throw std::invalid_argument("h must be non-negative");
        }
        if (cell.area < 0.0) {
            throw std::invalid_argument("area must be non-negative");
        }
        if (cell.mass_deficit_account.volume < 0.0) {
            throw std::invalid_argument("deficit volume must be non-negative");
        }

        if (cell.h >= h_wet) {
            audit.surface_mass += cell.phi_t * cell.h * cell.area;
            ++audit.wet_cell_count;
        }
        audit.deficit_mass += cell.mass_deficit_account.volume;
    }
    audit.total_mass = audit.surface_mass + audit.deficit_mass;
    return audit;
}

SystemMassDelta audit_system_mass_against_reference(
    const SystemMassAudit& baseline,
    const std::vector<ExchangeCellState>& current_cells,
    double h_wet) {
    if (baseline.total_mass < 0.0) {
        throw std::invalid_argument("baseline total_mass must be non-negative");
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
    if (account.volume < 0.0) {
        throw std::invalid_argument("deficit volume must be non-negative");
    }
    if (unmet_volume < 0.0) {
        throw std::invalid_argument("unmet volume must be non-negative");
    }

    return MassDeficitAccount{.volume = account.volume + unmet_volume};
}

MassDeficitAccount apply_repayment(const MassDeficitAccount& account, double applied_volume) {
    if (account.volume < 0.0) {
        throw std::invalid_argument("deficit volume must be non-negative");
    }
    if (applied_volume < 0.0) {
        throw std::invalid_argument("applied volume must be non-negative");
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
    for (const auto& endpoint_event : event.shared_endpoint_events) {
        if (!std::isfinite(endpoint_event.unmet_volume) || endpoint_event.unmet_volume < 0.0) {
            throw std::invalid_argument("shared endpoint unmet_volume must be finite and non-negative");
        }
        if (!std::isfinite(endpoint_event.repayment_volume) || endpoint_event.repayment_volume < 0.0) {
            throw std::invalid_argument("shared endpoint repayment_volume must be finite and non-negative");
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

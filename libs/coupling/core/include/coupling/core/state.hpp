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

[[nodiscard]] EngineHealthDiagnostic classify_engine_health(const EngineReport& report);
[[nodiscard]] EngineHealthAggregate aggregate_engine_health(
    const std::vector<EngineHealthDiagnostic>& diagnostics);

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

[[nodiscard]] ExchangeDecision evaluate_exchange(
    const ExchangeCellState& cell,
    const ExchangeRequest& request);

[[nodiscard]] std::vector<SharedExchangeDecision> evaluate_shared_exchange(
    const ExchangeCellState& cell,
    const std::vector<SharedExchangeIntent>& intents,
    double dt_sub);

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

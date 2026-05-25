#pragma once

#include <cstddef>
#include <vector>

namespace scau::coupling::core {

struct MassDeficitAccount {
    double volume{0.0};
};

struct ExchangeCellState {
    double volume{0.0};
    MassDeficitAccount mass_deficit_account{};
    double phi_t{0.0};
    double h{0.0};
    double area{0.0};
};

struct FlowLimit {
    double v_limit{0.0};
    double q_limit{0.0};
};

[[nodiscard]] FlowLimit compute_flow_limit(const ExchangeCellState& cell, double dt_sub);

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

[[nodiscard]] ExchangeDecision evaluate_exchange(
    const ExchangeCellState& cell,
    const ExchangeRequest& request);

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

[[nodiscard]] ExchangePipelineDecision evaluate_exchange_pipeline(
    const ExchangeCellState& cell,
    const ExchangeRequest& request);

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

[[nodiscard]] SystemMassAudit compute_system_mass(
    const std::vector<ExchangeCellState>& cells,
    double h_wet);

[[nodiscard]] SystemMassDelta audit_system_mass_against_reference(
    const SystemMassAudit& baseline,
    const std::vector<ExchangeCellState>& current_cells,
    double h_wet);

[[nodiscard]] MassDeficitAccount roll_deficit(const MassDeficitAccount& account, double unmet_volume);
[[nodiscard]] MassDeficitAccount apply_repayment(const MassDeficitAccount& account, double applied_volume);

struct CouplingEvent {
    std::size_t exchange_cell_index{0U};
    double volume_delta{0.0};
    double unmet_volume{0.0};
    double repayment_volume{0.0};
};

struct RuntimeCounters {
    std::size_t count_drain_split{0};
    std::size_t count_negative_depth_fix{0};
};

class CouplingSnapshot {
public:
    [[nodiscard]] const std::vector<ExchangeCellState>& cells() const noexcept;
    [[nodiscard]] const RuntimeCounters& runtime_counters() const noexcept;

private:
    friend class CouplingState;

    CouplingSnapshot(std::vector<ExchangeCellState> cells, RuntimeCounters counters);

    std::vector<ExchangeCellState> cells_;
    RuntimeCounters runtime_counters_;
};

class CouplingState {
public:
    explicit CouplingState(std::vector<ExchangeCellState> cells);

    [[nodiscard]] const std::vector<ExchangeCellState>& cells() const noexcept;
    [[nodiscard]] const RuntimeCounters& runtime_counters() const noexcept;
    [[nodiscard]] CouplingSnapshot snapshot() const;

    void enqueue_event(CouplingEvent event);
    void rollback(const CouplingSnapshot& snapshot);
    void replay_pending();
    void record_pipeline_decision(const ExchangePipelineDecision& decision);
    ExchangePipelineDecision apply_exchange(std::size_t cell_index, const ExchangeRequest& request);

private:
    std::vector<ExchangeCellState> cells_;
    RuntimeCounters runtime_counters_{};
    std::vector<CouplingEvent> pending_events_;
};

}  // namespace scau::coupling::core

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

[[nodiscard]] MassDeficitAccount roll_deficit(const MassDeficitAccount& account, double unmet_volume);
[[nodiscard]] MassDeficitAccount apply_repayment(const MassDeficitAccount& account, double applied_volume);

struct CouplingEvent {
    std::size_t exchange_cell_index{0U};
    double volume_delta{0.0};
    double unmet_volume{0.0};
    double repayment_volume{0.0};
};

class CouplingSnapshot {
public:
    [[nodiscard]] const std::vector<ExchangeCellState>& cells() const noexcept;

private:
    friend class CouplingState;

    explicit CouplingSnapshot(std::vector<ExchangeCellState> cells);

    std::vector<ExchangeCellState> cells_;
};

class CouplingState {
public:
    explicit CouplingState(std::vector<ExchangeCellState> cells);

    [[nodiscard]] const std::vector<ExchangeCellState>& cells() const noexcept;
    [[nodiscard]] CouplingSnapshot snapshot() const;

    void enqueue_event(CouplingEvent event);
    void rollback(const CouplingSnapshot& snapshot);
    void replay_pending();

private:
    std::vector<ExchangeCellState> cells_;
    std::vector<CouplingEvent> pending_events_;
};

}  // namespace scau::coupling::core

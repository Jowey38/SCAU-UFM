#include "coupling/core/state.hpp"

#include <stdexcept>
#include <utility>

namespace scau::coupling::core {

FlowLimit compute_flow_limit(const ExchangeCellState& cell, double dt_sub) {
    if (dt_sub <= 0.0) {
        throw std::invalid_argument("dt_sub must be positive");
    }
    if (cell.phi_t < 0.0) {
        throw std::invalid_argument("phi_t must be non-negative");
    }
    if (cell.h < 0.0) {
        throw std::invalid_argument("h must be non-negative");
    }
    if (cell.area < 0.0) {
        throw std::invalid_argument("area must be non-negative");
    }

    const double v_limit = 0.9 * cell.phi_t * cell.h * cell.area;
    return FlowLimit{
        .v_limit = v_limit,
        .q_limit = v_limit / dt_sub,
    };
}

CouplingSnapshot::CouplingSnapshot(std::vector<ExchangeCellState> cells) : cells_(std::move(cells)) {}

const std::vector<ExchangeCellState>& CouplingSnapshot::cells() const noexcept {
    return cells_;
}

CouplingState::CouplingState(std::vector<ExchangeCellState> cells) : cells_(std::move(cells)) {}

const std::vector<ExchangeCellState>& CouplingState::cells() const noexcept {
    return cells_;
}

CouplingSnapshot CouplingState::snapshot() const {
    return CouplingSnapshot{cells_};
}

void CouplingState::enqueue_event(CouplingEvent event) {
    if (event.exchange_cell_index >= cells_.size()) {
        throw std::out_of_range("coupling event exchange cell index is out of range");
    }
    pending_events_.push_back(event);
}

void CouplingState::rollback(const CouplingSnapshot& snapshot) {
    cells_ = snapshot.cells_;
}

void CouplingState::replay_pending() {
    for (const auto& event : pending_events_) {
        cells_[event.exchange_cell_index].volume += event.volume_delta;
    }
    pending_events_.clear();
}

}  // namespace scau::coupling::core

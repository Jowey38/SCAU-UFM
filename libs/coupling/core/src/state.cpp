#include "coupling/core/state.hpp"

#include <algorithm>
#include <cmath>
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

ExchangeDecision evaluate_exchange(const ExchangeCellState& cell, const ExchangeRequest& request) {
    if (request.q_request < 0.0) {
        throw std::invalid_argument("q_request must be non-negative");
    }
    if (cell.mass_deficit_account.volume < 0.0) {
        throw std::invalid_argument("deficit volume must be non-negative");
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

DrainSplit split_drain(const ExchangeCellState& cell, const ExchangeDecision& decision, double dt_sub) {
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
    if (decision.v_granted < 0.0) {
        throw std::invalid_argument("v_granted must be non-negative");
    }

    const double threshold = 0.2 * cell.phi_t * cell.h * cell.area;
    if (decision.v_granted <= threshold) {
        return DrainSplit{
            .micro_steps = 1,
            .dt_micro = dt_sub,
            .v_per_micro_step = decision.v_granted,
        };
    }
    if (threshold <= 0.0) {
        throw std::invalid_argument("geometric storage must be positive when v_granted is positive");
    }

    const int requested = static_cast<int>(std::ceil(decision.v_granted / threshold));
    const int micro_steps = std::min(5, requested);
    return DrainSplit{
        .micro_steps = micro_steps,
        .dt_micro = dt_sub / micro_steps,
        .v_per_micro_step = decision.v_granted / micro_steps,
    };
}

ExchangeDecision enforce_nonnegative_storage(
    const ExchangeCellState& cell,
    const ExchangeDecision& decision,
    double dt_sub) {
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
    if (decision.q_granted < 0.0) {
        throw std::invalid_argument("q_granted must be non-negative");
    }
    if (decision.v_granted < 0.0) {
        throw std::invalid_argument("v_granted must be non-negative");
    }
    if (decision.q_repay < 0.0) {
        throw std::invalid_argument("q_repay must be non-negative");
    }
    if (decision.v_repay < 0.0) {
        throw std::invalid_argument("v_repay must be non-negative");
    }
    if (decision.v_unmet < 0.0) {
        throw std::invalid_argument("v_unmet must be non-negative");
    }

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
        auto& cell = cells_[event.exchange_cell_index];
        cell.volume += event.volume_delta;
        cell.mass_deficit_account = roll_deficit(cell.mass_deficit_account, event.unmet_volume);
        cell.mass_deficit_account = apply_repayment(cell.mass_deficit_account, event.repayment_volume);
    }
    pending_events_.clear();
}

}  // namespace scau::coupling::core

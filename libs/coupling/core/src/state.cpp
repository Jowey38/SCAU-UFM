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

CouplingSnapshot::CouplingSnapshot(std::vector<ExchangeCellState> cells, RuntimeCounters counters)
    : cells_(std::move(cells)), runtime_counters_(counters) {}

const std::vector<ExchangeCellState>& CouplingSnapshot::cells() const noexcept {
    return cells_;
}

const RuntimeCounters& CouplingSnapshot::runtime_counters() const noexcept {
    return runtime_counters_;
}

SystemMassAudit CouplingSnapshot::compute_system_mass(double h_wet) const {
    return core::compute_system_mass(cells_, h_wet);
}

CouplingState::CouplingState(std::vector<ExchangeCellState> cells) : cells_(std::move(cells)) {}

const std::vector<ExchangeCellState>& CouplingState::cells() const noexcept {
    return cells_;
}

const RuntimeCounters& CouplingState::runtime_counters() const noexcept {
    return runtime_counters_;
}

CouplingSnapshot CouplingState::snapshot() const {
    return CouplingSnapshot{cells_, runtime_counters_};
}

SystemMassAudit CouplingState::compute_system_mass(double h_wet) const {
    return core::compute_system_mass(cells_, h_wet);
}

SystemMassDelta CouplingState::audit_system_mass_against_reference(
    const SystemMassAudit& baseline,
    double h_wet) const {
    return core::audit_system_mass_against_reference(baseline, cells_, h_wet);
}

void CouplingState::enqueue_event(CouplingEvent event) {
    if (event.exchange_cell_index >= cells_.size()) {
        throw std::out_of_range("coupling event exchange cell index is out of range");
    }
    pending_events_.push_back(event);
}

void CouplingState::rollback(const CouplingSnapshot& snapshot) {
    cells_ = snapshot.cells_;
    runtime_counters_ = snapshot.runtime_counters_;
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
        .volume_delta = decision.exchange.v_granted,
        .unmet_volume = decision.exchange.v_unmet,
        .repayment_volume = decision.exchange.v_repay,
    });

    record_pipeline_decision(decision);

    return decision;
}

}  // namespace scau::coupling::core

#include "coupling/driver/roof_exchange_gate.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace scau::coupling::driver {

RoofExchangeGate::RoofExchangeGate(
    ExchangeEndpointStateFn endpoint_state_fn,
    surface2d::RoofDrainageAcceptanceFn downstream,
    Real dt_sub)
    : endpoint_state_fn_(std::move(endpoint_state_fn)),
      downstream_(std::move(downstream)),
      dt_sub_(dt_sub) {
    if (!endpoint_state_fn_) {
        throw std::invalid_argument("endpoint_state_fn must be callable");
    }
    if (!downstream_) {
        throw std::invalid_argument("downstream acceptance fn must be callable");
    }
    if (!std::isfinite(dt_sub_) || dt_sub_ <= 0.0) {
        throw std::invalid_argument("dt_sub must be finite and positive");
    }
}

surface2d::RoofDrainageAcceptance RoofExchangeGate::operator()(
    const surface2d::RoofDrainageIntent& intent) {
    // Invalid volumes are the downstream adapter's fail-closed contract;
    // forward unchanged so there is a single source of truth for them.
    if (!std::isfinite(intent.requested_volume) || intent.requested_volume < 0.0) {
        return downstream_(intent);
    }

    const auto endpoint = endpoint_state_fn_(intent);
    if (!endpoint.has_value()) {
        // Arbitration cannot vouch for the endpoint: full fail-closed reject,
        // downstream never sees the intent.
        surface2d::RoofDrainageAcceptance acceptance;
        acceptance.requested_volume = intent.requested_volume;
        acceptance.accepted_volume = 0.0;
        acceptance.rejected_volume = intent.requested_volume;
        acceptance.rejection_reason = surface2d::RoofDrainageRejectionReason::HealthBlocked;
        return acceptance;
    }

    // Q_limit hard gate with deficit repayment reservation (core semantics).
    const core::ExchangeRequest request{
        .q_request = intent.requested_volume / dt_sub_,
        .dt_sub = dt_sub_,
    };
    auto arbitration_endpoint = *endpoint;
    const auto limit = core::compute_flow_limit(arbitration_endpoint, dt_sub_);
    const Real q_already_granted = granted_q_by_cell_[intent.source_cell_index];
    const auto decision = core::evaluate_exchange(arbitration_endpoint, request);
    // Within-substep accumulation: earlier grants on this endpoint consume
    // Q_limit headroom the same way the repayment reservation does.
    const Real q_headroom =
        std::max(0.0, std::min(decision.q_granted, limit.q_limit - decision.q_repay - q_already_granted));
    const Real v_granted = q_headroom * dt_sub_;

    surface2d::RoofDrainageAcceptance acceptance;
    acceptance.requested_volume = intent.requested_volume;

    if (v_granted <= 0.0) {
        acceptance.accepted_volume = 0.0;
        acceptance.rejected_volume = intent.requested_volume;
        acceptance.rejection_reason = surface2d::RoofDrainageRejectionReason::CapacityLimited;
        return acceptance;
    }

    surface2d::RoofDrainageIntent clamped = intent;
    clamped.requested_volume = v_granted;
    const auto downstream_acceptance = downstream_(clamped);

    // Compose against the ORIGINAL request. The downstream adapter accounts
    // for the clamped volume only; anything it rejects stays rejected, and
    // the clamp shortfall is CapacityLimited.
    acceptance.accepted_volume = downstream_acceptance.accepted_volume;
    acceptance.rejected_volume = intent.requested_volume - downstream_acceptance.accepted_volume;
    if (downstream_acceptance.rejection_reason != surface2d::RoofDrainageRejectionReason::None) {
        acceptance.rejection_reason = downstream_acceptance.rejection_reason;
    } else if (v_granted < intent.requested_volume) {
        acceptance.rejection_reason = surface2d::RoofDrainageRejectionReason::CapacityLimited;
    } else {
        acceptance.rejection_reason = surface2d::RoofDrainageRejectionReason::None;
    }

    if (downstream_acceptance.accepted_volume > 0.0) {
        granted_q_by_cell_[intent.source_cell_index] += downstream_acceptance.accepted_volume / dt_sub_;
    }

    return acceptance;
}

void RoofExchangeGate::begin_substep() {
    granted_q_by_cell_.clear();
}

}  // namespace scau::coupling::driver

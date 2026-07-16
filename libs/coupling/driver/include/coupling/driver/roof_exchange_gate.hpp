#pragma once

#include <functional>
#include <optional>
#include <unordered_map>

#include "coupling/core/state.hpp"
#include "surface2d/source_terms/runoff/result.hpp"
#include "surface2d/source_terms/runoff/roof_step.hpp"

namespace scau::coupling::driver {

// Within scau::coupling::driver, unqualified core:: finds coupling::core;
// spell the scalar type out to keep referring to the project-wide Real.
using Real = scau::core::Real;

// Resolves the CouplingLib exchange endpoint state backing a roof drainage
// intent (typically the mapped 2D exchange cell). Returning std::nullopt
// means arbitration cannot vouch for the endpoint: the gate fail-closes.
using ExchangeEndpointStateFn =
    std::function<std::optional<core::ExchangeCellState>(const surface2d::RoofDrainageIntent&)>;

// CouplingLib arbitration decorator in front of a roof drainage acceptance
// sink (the SWMM adapter). Owns the Q_limit hard gate and the deficit
// repayment reservation; the downstream adapter keeps engine-facing
// fail-closed semantics (invalid node, surcharge).
//
// Semantics (main spec / symbols reference):
// - V_limit = 0.9 * phi_t * h * A; Q_limit = V_limit / dt_sub (two-step form,
//   via core::compute_flow_limit).
// - q_repay = min(mass_deficit_account.volume / dt_sub, Q_limit) is RESERVED
//   headroom for deficit repayment; roof water never repays or accrues the
//   deficit ledger itself (M247 invariant: rejected roof water stays roof
//   pending/overflow on the surface2d side).
// - Grants accumulate per endpoint cell within a substep so multiple roof
//   intents cannot jointly exceed Q_limit; begin_substep() resets the ledger.
class RoofExchangeGate final {
public:
    RoofExchangeGate(
        ExchangeEndpointStateFn endpoint_state_fn,
        surface2d::RoofDrainageAcceptanceFn downstream,
        Real dt_sub);

    [[nodiscard]] surface2d::RoofDrainageAcceptance operator()(
        const surface2d::RoofDrainageIntent& intent);

    void begin_substep();

private:
    ExchangeEndpointStateFn endpoint_state_fn_;
    surface2d::RoofDrainageAcceptanceFn downstream_;
    Real dt_sub_{0.0};
    std::unordered_map<int, Real> granted_q_by_cell_;
};

}  // namespace scau::coupling::driver

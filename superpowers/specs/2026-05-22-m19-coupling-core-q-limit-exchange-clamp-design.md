# M19 CouplingLib Core Q_limit Exchange Clamp Design

**Date:** 2026-05-22

## Goal

Add the minimal pure `CouplingLib` core decision that clamps a single exchange request against the canonical `Q_limit` and accrues any unmet volume into the cell's `mass_deficit_account`.

M19 ties together the existing M16 hard-gate (`V_limit` / `Q_limit`), the M17 ledger update primitives (`roll_deficit` / `apply_repayment`), and the M18 ledger persistence (`CouplingEvent.unmet_volume` / `CouplingEvent.repayment_volume`) into one closed-loop pure-core function.

## Scope

M19 covers exactly one donor cell with one exchange request per substep. It deliberately excludes:

- multi-engine arbitration and `priority_weight` (spec §6.1.1)
- `drain_split` micro-step rule (spec §6.1.4)
- `epsilon_deficit` tolerance under performance path (spec §6.3)
- scheduler, fault controller, write-off accounting
- SWMM, D-Flow FM, `Surface2DCore`, or any 1D engine adapter

M19 only computes a pure decision. It does not mutate `CouplingState` or enqueue `CouplingEvent` itself; downstream call sites already have the M18 path for that.

## API

The public core API adds two value types and one pure decision function:

```cpp
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
```

Units:

- `q_request`, `q_granted`, `q_repay` in `m^3/s`.
- `v_granted`, `v_repay`, `v_unmet` in `m^3`.
- `dt_sub` in `s`.

## Behavior

For valid inputs the function follows spec §6.1 deficit-clearing-first arbitration on one cell:

1. `limit = compute_flow_limit(cell, request.dt_sub)` — reuses the M16 validation of `dt_sub > 0`, `phi_t >= 0`, `h >= 0`, `area >= 0`.
2. `q_repay = min(cell.mass_deficit_account.volume / request.dt_sub, limit.q_limit)` — clear pending deficit first, but never exceed `Q_limit`.
3. `q_granted = min(request.q_request, limit.q_limit - q_repay)` — new intent gets only the remaining hard-gate budget.
4. `v_granted = q_granted * request.dt_sub`.
5. `v_repay = q_repay * request.dt_sub`.
6. `v_unmet = (request.q_request - q_granted) * request.dt_sub` — captures spec §6.3 `V_deficit_new = V_deficit_old + (Q_target * dt_sub - Q_applied * dt_sub)`.

Validation boundaries:

- `request.q_request >= 0` — otherwise throw `std::invalid_argument`.
- `cell.mass_deficit_account.volume >= 0` — otherwise throw `std::invalid_argument`.
- `dt_sub`, `phi_t`, `h`, `area` validation comes from `compute_flow_limit(...)`.

Post-conditions that the focused tests assert:

- `0 <= q_granted <= q_request`.
- `0 <= q_granted + q_repay <= q_limit` (strict equality only when the request or deficit saturates the hard gate).
- `v_unmet >= 0`.
- When `q_request == 0`, `q_granted == 0` and `v_unmet == 0`.
- When `deficit_volume == 0`, `q_repay == 0`.
- When `deficit_volume / dt_sub >= q_limit`, `q_granted == 0` and `v_unmet == q_request * dt_sub`.

## Tests

Add focused `test_coupling_exchange_decision` coverage for:

- request below `Q_limit` with no deficit grants the full request.
- request above `Q_limit` with no deficit clamps to `Q_limit` and accrues unmet volume.
- deficit smaller than the hard gate is fully repaid alongside the new request.
- deficit larger than the hard gate consumes the full `Q_limit`, blocks the new request, and accrues unmet volume equal to the full request.
- negative request volume and negative deficit volume are rejected.

Existing focused tests stay untouched:

- `test_coupling_flow_limit` keeps proving `V_limit` / `Q_limit`.
- `test_coupling_mass_deficit_account` keeps proving ledger update primitives.
- `test_coupling_core_state` keeps proving snapshot, rollback, and replay with deficit ledger.

## Boundaries

- Do not introduce arbitration or `priority_weight` semantics; M19 is single-donor only.
- Do not introduce `drain_split` micro-step splitting.
- Do not introduce `epsilon_deficit` tolerance.
- Do not introduce scheduler, fault controller, or write-off logic.
- Do not connect M19 to SWMM, D-Flow FM, `Surface2DCore`, or any 1D engine adapter.
- Do not redefine `Q_limit`, `V_limit`, or `mass_deficit_account` semantics.
- Do not mutate `CouplingState` from inside `evaluate_exchange(...)`; the decision must remain a pure function.

# M22 CouplingLib Core Single Donor Pipeline Design

**Date:** 2026-05-24

## Goal

Add the minimal pure `CouplingLib` core pipeline that composes the single-donor exchange safeguards already implemented in M19-M21 into one final decision function.

M22 makes the execution order testable:

1. M19 `evaluate_exchange(...)` applies `Q_limit`, deficit repayment priority, and unmet accrual.
2. M21 `enforce_nonnegative_storage(...)` clamps the granted volume against donor-side physical storage `phi_t * h * A`.
3. M20 `split_drain(...)` computes the 20% smoothing micro-step plan for the final granted volume.

## Scope

M22 covers exactly one donor cell and one `ExchangeRequest`. It returns the final `ExchangeDecision` and `DrainSplit` together, but remains a pure function.

It deliberately excludes:

- multi-donor linear programming
- multi-engine arbitration and `priority_weight`
- shared-cell `V_limit_k` splitting
- runtime counters such as `count_drain_split` or `count_negative_depth_fix`
- scheduler, fault controller, write-off accounting
- SWMM, D-Flow FM, `Surface2DCore`, or any 1D engine adapter

M22 does not mutate `CouplingState`, update `mass_deficit_account`, or enqueue `CouplingEvent` itself. Downstream callers can feed the returned decision through the existing M18 event path.

## API

The public core API adds one value type and one pure function:

```cpp
struct ExchangePipelineDecision {
    ExchangeDecision exchange{};
    DrainSplit drain_split{};
};

[[nodiscard]] ExchangePipelineDecision evaluate_exchange_pipeline(
    const ExchangeCellState& cell,
    const ExchangeRequest& request);
```

## Behavior

For valid inputs:

```cpp
const auto initial = evaluate_exchange(cell, request);
const auto nonnegative = enforce_nonnegative_storage(cell, initial, request.dt_sub);
const auto split = split_drain(cell, nonnegative, request.dt_sub);
return ExchangePipelineDecision{.exchange = nonnegative, .drain_split = split};
```

Validation is intentionally delegated to the composed helpers:

- `evaluate_exchange(...)` validates `request.q_request`, `dt_sub`, storage fields, and deficit volume.
- `enforce_nonnegative_storage(...)` validates final decision fields and donor physical storage fields.
- `split_drain(...)` validates the final granted volume and 20% smoothing inputs.

The function should not add new validation rules or duplicate logic.

## Tests

Add focused `test_coupling_exchange_pipeline` coverage for:

- safe request below `Q_limit` and storage returns full grant with one drain micro-step.
- request above `Q_limit` clamps through M19 and reports unmet volume.
- storage below M19 grant triggers M21 backoff and increases unmet volume.
- large final granted volume uses M20 drain split count derived from the post-backoff grant.
- existing deficit is repaid before new request grant and final split is computed from the new grant.
- invalid negative request and invalid negative storage/deficit are rejected through composed helpers.

Existing focused tests stay untouched:

- `test_coupling_flow_limit` proves `V_limit` / `Q_limit`.
- `test_coupling_mass_deficit_account` proves ledger update primitives.
- `test_coupling_core_state` proves snapshot / rollback / replay with deficit ledger.
- `test_coupling_exchange_decision` proves M19.
- `test_coupling_drain_split` proves M20.
- `test_coupling_nonnegative_storage_backoff` proves M21.

## Boundaries

- Do not introduce new exchange semantics; compose M19-M21 helpers only.
- Do not implement multi-donor linear programming.
- Do not introduce arbitration, `priority_weight`, shared-cell splitting, or `V_limit_k`.
- Do not introduce runtime counters.
- Do not introduce `epsilon_deficit`, scheduler, fault, or write-off logic.
- Do not connect `evaluate_exchange_pipeline(...)` to SWMM, D-Flow FM, `Surface2DCore`, or any 1D engine adapter.
- Do not mutate `CouplingState` from inside `evaluate_exchange_pipeline(...)`; the pipeline remains pure.

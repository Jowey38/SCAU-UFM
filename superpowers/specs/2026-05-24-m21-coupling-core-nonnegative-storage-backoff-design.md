# M21 CouplingLib Core Nonnegative Storage Backoff Design

**Date:** 2026-05-24

## Goal

Add the minimal pure `CouplingLib` core decision that clamps a single donor exchange decision so its granted volume cannot drive donor-side physical storage below zero.

M21 implements the single-donor slice of spec §6.1 rule 5 (nonnegative storage backoff). It runs after the M19 `Q_limit` / deficit clamp and is compatible with the M20 `drain_split` micro-step decision.

## Scope

M21 covers exactly one donor cell and one already-arbitrated `ExchangeDecision`. It clamps only the new request grant (`q_granted` / `v_granted`) against donor-side available physical storage:

```text
available_storage = phi_t * h * area
```

It deliberately excludes:

- multi-donor linear programming from spec §6.1 rule 5
- multi-engine arbitration and `priority_weight`
- shared-cell `V_limit_k` splitting
- runtime counters such as `count_negative_depth_fix`
- scheduler, fault controller, write-off accounting
- SWMM, D-Flow FM, `Surface2DCore`, or any 1D engine adapter

M21 is a pure decision. It does not mutate `CouplingState`, alter `mass_deficit_account`, or enqueue `CouplingEvent` itself.

## API

The public core API adds one pure function:

```cpp
[[nodiscard]] ExchangeDecision enforce_nonnegative_storage(
    const ExchangeCellState& cell,
    const ExchangeDecision& decision,
    double dt_sub);
```

M21 reuses the existing `ExchangeDecision` type instead of introducing a new return type. The returned decision preserves repayment and unmet fields from the input decision except for additional unmet volume caused by the final donor-storage clamp.

## Behavior

For valid inputs:

1. Compute `available_storage = phi_t * h * area`.
2. If `decision.v_granted <= available_storage`, return the decision unchanged.
3. Otherwise clamp:
   - `clamped_v_granted = available_storage`
   - `clamped_q_granted = available_storage / dt_sub`
   - `additional_unmet = decision.v_granted - available_storage`
   - `v_unmet = decision.v_unmet + additional_unmet`
   - preserve `q_repay` and `v_repay`

Repayment preservation is intentional in this slice. M19 already gives deficit repayment priority over new intent. M21's final clamp only prevents the new granted donor extraction from exceeding physical storage. It does not reinterpret or write off prior deficit accounting.

Validation boundaries:

- `dt_sub > 0`.
- `cell.phi_t >= 0`, `cell.h >= 0`, `cell.area >= 0`.
- `decision.q_granted >= 0`, `decision.v_granted >= 0`, `decision.q_repay >= 0`, `decision.v_repay >= 0`, `decision.v_unmet >= 0`.

Post-conditions that focused tests assert:

- returned `v_granted <= phi_t * h * area`
- returned `q_granted == returned.v_granted / dt_sub`
- returned `v_unmet` increases by exactly the clamped-off granted volume
- repayment fields are preserved
- an already-safe decision is returned unchanged

## Tests

Add focused `test_coupling_nonnegative_storage_backoff` coverage for:

- decision below available storage is unchanged.
- decision exactly at available storage is unchanged.
- decision above available storage clamps `v_granted`, recomputes `q_granted`, and moves the difference into `v_unmet`.
- zero available storage clamps a positive grant to zero and moves all granted volume into `v_unmet`.
- repayment fields are preserved through clamp.
- invalid `dt_sub`, negative geometry, and negative decision fields are rejected.

Existing focused tests stay untouched:

- `test_coupling_flow_limit` proves `V_limit` / `Q_limit`.
- `test_coupling_mass_deficit_account` proves ledger update primitives.
- `test_coupling_core_state` proves snapshot / rollback / replay with deficit ledger.
- `test_coupling_exchange_decision` proves the single-donor request clamp with deficit accrual.
- `test_coupling_drain_split` proves the 20% smoothing micro-step split.

## Boundaries

- Do not implement the multi-donor linear programming form of spec §6.1 rule 5.
- Do not introduce arbitration, `priority_weight`, shared-cell splitting, or `V_limit_k`.
- Do not introduce `count_negative_depth_fix` or any runtime counter plumbing.
- Do not introduce `epsilon_deficit`, scheduler, fault, or write-off logic.
- Do not connect `enforce_nonnegative_storage(...)` to SWMM, D-Flow FM, `Surface2DCore`, or any 1D engine adapter.
- Do not mutate `CouplingState` from inside `enforce_nonnegative_storage(...)`; the decision remains pure.

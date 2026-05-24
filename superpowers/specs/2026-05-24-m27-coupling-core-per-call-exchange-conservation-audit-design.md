# M27 CouplingLib Core Per-Call Exchange Conservation Audit Design

**Date:** 2026-05-24

## Goal

Lift the implicit §6.3 deficit-ledger volume identity `v_granted + v_unmet == q_request * dt_sub` into a pure, machine-asserted audit helper, so any future change to `evaluate_exchange(...)`, `enforce_nonnegative_storage(...)`, or `evaluate_exchange_pipeline(...)` that breaks the per-call accounting balance is caught by CI on the correctness path with strict zero residual.

## Normative Anchors

- `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` §6.3 (deficit-ledger update identity):
  `V_deficit,new = V_deficit,old + (Q_target * dt_sub - Q_applied * dt_sub)` where `Q_target = Q_req` and `Q_applied = Q_granted`. The accounting requires `V_target - V_applied == V_unmet` exactly, which is equivalent to `v_granted + v_unmet == q_request * dt_sub`.
- §8.3 (deterministic-replay correctness path): strict zero diff; no tolerance allowed.
- §11.4: `epsilon_mass = max(1e-10, 1e-12 * M_ref)` is the **performance-path** tolerance, not the correctness-path tolerance. M27 stays on the correctness path → strict equality.

## Scope

M27 adds one struct and one pure function in `libs/coupling/core`:

```cpp
struct ExchangeConservationAudit {
    double request_volume{0.0};   // q_request * dt_sub
    double accounted_volume{0.0}; // v_granted + v_unmet
    double residual{0.0};         // request_volume - accounted_volume
    bool balanced{true};          // residual == 0.0 strictly
};

[[nodiscard]] ExchangeConservationAudit audit_exchange_decision(
    const ExchangeRequest& request,
    const ExchangePipelineDecision& decision);
```

The function:

1. Validates `request.dt_sub > 0.0` and `request.q_request >= 0.0`; throws `std::invalid_argument` otherwise (mirrors M19).
2. Validates `decision.exchange.v_granted >= 0.0` and `decision.exchange.v_unmet >= 0.0`; throws `std::invalid_argument` otherwise.
3. Computes `request_volume = request.q_request * request.dt_sub`.
4. Computes `accounted_volume = decision.exchange.v_granted + decision.exchange.v_unmet`.
5. Computes `residual = request_volume - accounted_volume`.
6. Sets `balanced = (residual == 0.0)` (strict equality, no tolerance).
7. Returns the struct; never throws on imbalance — surfacing the residual is the caller's contract.

M27 deliberately excludes:

- System-wide `M_ref` audit across multiple cells (reserved for ≥ M28).
- Performance-path tolerance (`epsilon_mass`) — correctness path uses strict 0.
- Repayment-side audit (`v_repay == q_repay * dt_sub`) — covered implicitly by M19 invariants and not part of §6.3's `Q_target - Q_applied` identity (repayment is a separate flow against the deficit ledger, not against the new request).
- Multi-donor arbitration, `priority_weight`, `V_limit_k`.
- Any state mutation; the audit is read-only.
- IO, schema, or persistence.

## Tests

Create `tests/unit/coupling/test_coupling_exchange_audit.cpp` with:

- `SafeRequestIsBalancedWithZeroResidual` — full grant case: `request_volume == 16.0`, `accounted_volume == 16.0`, `residual == 0.0`, `balanced == true`.
- `HardGateClampScenarioIsBalanced` — `q_request = 20.0` case: `request_volume == 80.0`, `accounted_volume == 36.0 + 44.0 == 80.0`, `balanced == true`.
- `DeficitRepaymentScenarioIsBalanced` — repayment does not affect the request-side balance: `request_volume == 20.0`, `accounted_volume == 20.0`, `balanced == true`.
- `NonnegativeStorageBackoffScenarioIsBalanced` — M22 evidence scenario: `request_volume == 20.0`, `accounted_volume == 9.0 + 11.0 == 20.0`, `balanced == true`.
- `ManuallyConstructedImbalancedDecisionFlagsResidual` — feed a hand-crafted decision where `v_granted + v_unmet != q_request * dt_sub`; expect `balanced == false` and `residual` reporting the exact gap.
- `RejectsNegativeRequestOrDecisionVolumes` — `q_request < 0`, `dt_sub <= 0`, `v_granted < 0`, `v_unmet < 0` each throw `std::invalid_argument`.

Register `test_coupling_exchange_audit` in `tests/unit/coupling/CMakeLists.txt`.

## Boundaries

- Pure function; no state mutation.
- Strict equality only; no tolerance on the correctness path.
- No system-wide `M_ref` accumulation; per-call only.
- No repayment-side identity audit (out of §6.3 request-side scope).
- No multi-donor logic, `priority_weight`, `V_limit_k`, or DTO refactor.
- No connection to SWMM, D-Flow FM, `Surface2DCore`, or any 1D engine ABI.

# M23 CouplingLib Core Pipeline Diagnostic Flags Design

**Date:** 2026-05-24

## Goal

Add the smallest machine-readable diagnostic surface on top of M22 so that the M20 `split_drain` and M21 `enforce_nonnegative_storage` safeguards are observable per pipeline invocation, satisfying the §14.1 in-line counter contract and unlocking the snapshot/replay coverage required by §6.5 and §8.3.

## Scope

M23 only extends the return type of `evaluate_exchange_pipeline(...)`. It adds two `bool` fields to `ExchangePipelineDecision`:

- `drain_split_engaged` — `true` iff the returned `drain_split.micro_steps > 1`.
- `negative_depth_fix_engaged` — `true` iff `enforce_nonnegative_storage(...)` clamped the post-M19 `v_granted`.

M23 deliberately excludes:

- Stateful accumulation into `CouplingState` (deferred to M24).
- Snapshot / rollback / replay updates for runtime counters (deferred to M24).
- Multi-donor arbitration, `priority_weight`, `V_limit_k` (deferred to ≥ M25).
- Any new exchange physics, validation rule, or DTO refactor.

## Normative Anchors

- `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` §14.1: runtime must produce `count_drain_split` and `count_negative_depth_fix` as machine-readable diagnostics.
- `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` §6.5: snapshot must cover `RuntimeCounters`.
- `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` §8.3: deterministic replay diff on counters must be exactly 0.
- `superpowers/specs/2026-04-22-symbols-and-terms-reference.md`: counter names are part of machine-facing naming constraints.

M23 produces the **per-invocation observation primitives** that downstream stateful counters (M24+) will accumulate.

## API

```cpp
struct ExchangePipelineDecision {
    ExchangeDecision exchange{};
    DrainSplit drain_split{};
    bool drain_split_engaged{false};
    bool negative_depth_fix_engaged{false};
};
```

`evaluate_exchange_pipeline(...)` signature is unchanged.

## Behavior

```cpp
const auto initial = evaluate_exchange(cell, request);
const auto nonnegative = enforce_nonnegative_storage(cell, initial, request.dt_sub);
const auto split = split_drain(cell, nonnegative, request.dt_sub);

return ExchangePipelineDecision{
    .exchange = nonnegative,
    .drain_split = split,
    .drain_split_engaged = split.micro_steps > 1,
    .negative_depth_fix_engaged = nonnegative.v_granted < initial.v_granted,
};
```

The strict `<` comparison is intentional: M21 only flags engagement when the clamp **lowered** the granted volume. Equality means M21 was a no-op pass-through.

## Tests

Extend `test_coupling_exchange_pipeline`:

- `SafeRequestReturnsFullGrantAndSingleMicroStep` asserts both flags are `false`.
- `RequestAboveHardGateClampsAndAccruesUnmet` asserts `drain_split_engaged == true` and `negative_depth_fix_engaged == false` (M19 clamp keeps `v_granted = V_limit < phi_t·h·A`).
- `NonnegativeStorageBackoffRunsAfterHardGateClamp` asserts `drain_split_engaged == true` and `negative_depth_fix_engaged == false` (canonical `V_limit = 0.9·phi_t·h·A` makes M21 unreachable here — the flag turns the M22 evidence observation into a machine-asserted invariant).
- `DeficitRepaymentPrecedesNewRequestGrant` asserts `drain_split_engaged == true` and `negative_depth_fix_engaged == false`.
- A new `EngagementFlagsRemainOffWhenSplitIsSingleMicroStep` test that exercises a request whose `v_granted ≤ 0.2·phi_t·h·A` and asserts both flags `false`. (Reuses the safe case if the threshold check is otherwise covered; otherwise add a slightly tighter scenario.)

The invalid-input test remains unchanged.

## Boundaries

- Do not introduce `count_drain_split` or `count_negative_depth_fix` integer counters yet — those belong on `CouplingState` and require snapshot/rollback/replay updates (M24).
- Do not change `evaluate_exchange(...)`, `enforce_nonnegative_storage(...)`, or `split_drain(...)`.
- Do not change `ExchangeDecision`, `DrainSplit`, or any DTO besides `ExchangePipelineDecision`.
- Do not introduce tolerances, `epsilon_deficit`, or scheduler logic.
- Do not connect the pipeline or its flags to SWMM, D-Flow FM, `Surface2DCore`, or any 1D engine ABI.

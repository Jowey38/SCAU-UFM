# M20 CouplingLib Core Drain Split Design

**Date:** 2026-05-23

## Goal

Add the minimal pure `CouplingLib` core function that decides how many micro-steps a single arbitrated exchange decision must run as, following spec §6.1 rule 4 ("drain_split / 20% smoothing rule").

## Scope

M20 covers exactly one donor cell with one already-arbitrated `ExchangeDecision`. It takes the granted volume from M19 and reports the micro-step count and per-micro-step distribution. It deliberately excludes:

- multi-engine arbitration and `priority_weight` (spec §6.1.1)
- shared-cell `V_limit_k` splitting (spec §6.1.1)
- `Q_repay`, `mass_deficit_account`, and request clamping (already in M17 / M18 / M19)
- `count_drain_split` runtime counter plumbing (spec §11)
- scheduler, fault controller, write-off, replay integration with micro-steps
- SWMM, D-Flow FM, `Surface2DCore`, or any 1D engine adapter

M20 is a pure decision. It does not mutate `CouplingState` and does not enqueue `CouplingEvent` itself.

## API

The public core API adds one value type and one pure function:

```cpp
struct DrainSplit {
    int micro_steps{1};
    double dt_micro{0.0};
    double v_per_micro_step{0.0};
};

[[nodiscard]] DrainSplit split_drain(
    const ExchangeCellState& cell,
    const ExchangeDecision& decision,
    double dt_sub);
```

Units:

- `dt_sub`, `dt_micro` in `s`.
- `v_per_micro_step` in `m^3`.

## Behavior

Spec §6.1 rule 4 reads:

> 若仲裁后单个子步 `Q_granted * dt > 0.2 * phi_t * h * A`，则将该子步拆分为多个微步执行（20% 平滑规则），微步数不超过 5。该阈值的参考底显式锁定为整体几何储量 `phi_t * h * A`（**而非** `V_limit` 或 `V_limit_k`）。

The pure function follows that text exactly:

1. `threshold = 0.2 * cell.phi_t * cell.h * cell.area`.
2. If `decision.v_granted <= threshold`, the substep does not split: `micro_steps = 1`, `dt_micro = dt_sub`, `v_per_micro_step = decision.v_granted`.
3. Otherwise compute `n = std::min(5, ceil(decision.v_granted / threshold))`, then `dt_micro = dt_sub / n` and `v_per_micro_step = decision.v_granted / n`.

Spec arithmetic guarantee: the M16 hard gate enforces `v_granted <= V_limit = 0.9 * phi_t * h * A = 4.5 * threshold`, so `ceil(v_granted / threshold) <= 5` is naturally satisfied for any `ExchangeDecision` produced by M19. The hard cap at 5 is kept verbatim so that hand-constructed decisions or future changes to the `V_limit` coefficient never silently emit more than 5 micro-steps.

When `decision.v_granted == 0` the function reports a single trivial micro-step.

## Validation

The function rejects unphysical inputs with `std::invalid_argument`:

- `dt_sub <= 0`.
- `cell.phi_t < 0`, `cell.h < 0`, `cell.area < 0`.
- `decision.v_granted < 0`.
- `threshold == 0` while `decision.v_granted > 0` — this can never come from a valid M19 decision (`v_granted` is then forced to `0`), but the explicit check makes the helper safe in isolation.

The `dt_sub > 0` check is intentionally local to `split_drain` rather than delegated to `compute_flow_limit`, because M20 must work on any pre-arbitrated decision regardless of how the upstream limit was computed.

## Tests

Add focused `test_coupling_drain_split` coverage for:

- granted volume well below the threshold reports a single micro-step.
- granted volume exactly equal to the threshold reports a single micro-step.
- granted volume slightly above the threshold reports two equal micro-steps.
- granted volume that would require more than 5 micro-steps is capped at 5.
- zero granted volume reports a single trivial micro-step.
- non-positive `dt_sub`, negative `phi_t` / `h` / `area`, and negative `v_granted` are rejected.
- zero geometric storage paired with positive `v_granted` is rejected.

Existing focused tests stay untouched:

- `test_coupling_flow_limit` keeps proving `V_limit` / `Q_limit`.
- `test_coupling_mass_deficit_account` keeps proving ledger update primitives.
- `test_coupling_core_state` keeps proving snapshot / rollback / replay with deficit ledger.
- `test_coupling_exchange_decision` keeps proving the single-donor request clamp with deficit accrual.

## Boundaries

- Do not introduce `count_drain_split` runtime counter or any counter plumbing.
- Do not introduce arbitration, `priority_weight`, or multi-engine shared-cell splitting.
- Do not introduce `epsilon_deficit`, scheduler, fault, or write-off logic.
- Do not connect `split_drain(...)` to SWMM, D-Flow FM, `Surface2DCore`, or any 1D engine adapter.
- Do not redefine `Q_limit`, `V_limit`, or `mass_deficit_account` semantics.
- Do not mutate `CouplingState` from inside `split_drain(...)`; the decision must remain a pure function.

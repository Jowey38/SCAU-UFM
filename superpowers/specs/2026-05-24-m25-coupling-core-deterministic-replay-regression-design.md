# M25 CouplingLib Core Deterministic Replay Regression Design

**Date:** 2026-05-24

## Goal

Lock the §8.3 deterministic-replay strict zero-diff invariant inside the current `CouplingLib core` API surface as machine-asserted regression tests, so that any future change to `snapshot()` / `rollback()` / `replay_pending()` / `record_pipeline_decision(...)` that breaks deterministic equality is caught by CI.

## Normative Anchor

`superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` §8.3 (deterministic replay strict rule, v5.3.3):

> 同一 snapshot、输入事件序列与 `dt_couple` 切分下，`h/hu/hv`、exchange buffer、`mass_deficit_account`、FaultController 状态与 `RuntimeCounters` 的逐字段差异必须严格为 0；任何非零差异均判定 replay 失败，不得使用容差掩盖。

M25 covers, in the current `CouplingLib core` API surface, the following §8.3 fields:

- `ExchangeCellState::volume`
- `ExchangeCellState::mass_deficit_account.volume`
- `RuntimeCounters::count_drain_split`
- `RuntimeCounters::count_negative_depth_fix`

§8.3 fields outside the current API surface (`h/hu/hv`, FaultController state, `dt_couple` slicing) are intentionally out of scope until their owning subsystems exist.

## Scope

M25 is pure-additive at the test layer. No production-code changes.

M25 adds three regression tests to `tests/unit/coupling/test_coupling_core_state.cpp`:

1. `DeterministicReplayPreservesCellsAndCountersWithIdenticalSequence` — two independent `CouplingState` instances driven by the same operation sequence yield byte-equal terminal state across every §8.3-covered field.
2. `RollbackAndReplayProducesIdenticalStateToFreshRun` — a state that takes a snapshot, makes some mutations, rolls back, then re-runs the same authoritative sequence ends in the same terminal state as another state that runs the authoritative sequence directly from the snapshot baseline.
3. `ReReplayAfterRollbackAndReEnqueueIsBitwiseIdentical` — captures the terminal state after one snapshot→enqueue→replay cycle, then rolls back to the snapshot, re-enqueues the same events, and replays again; both terminal states must be bitwise equal across every §8.3-covered field.

M25 deliberately excludes:

- Multi-donor arbitration, `priority_weight`, `V_limit_k`.
- `apply_exchange(...)` driver function or any combined entry point.
- Snapshot persistence, schema serialization, IO.
- Tolerance-based comparisons (§8.3 explicitly forbids them).
- Any §14.1 counter outside `count_drain_split` / `count_negative_depth_fix`.
- Connections to `Surface2DCore`, SWMM, D-Flow FM, or any 1D engine ABI.

## Assertion Discipline

All asserts must be strict equality:

- `EXPECT_DOUBLE_EQ` / `ASSERT_DOUBLE_EQ` for `double` fields (this is bitwise-equal for finite values that follow the same arithmetic path).
- `EXPECT_EQ` for `std::size_t` counter fields.

No `EXPECT_NEAR`, no `epsilon_deficit`, no `1e-12` slack. §8.3 mandates strict 0 diff on the correctness path.

## Pending-Events Semantics Note

After `replay_pending()` the internal pending-events queue is cleared. Therefore "rollback then replay again" with no re-enqueue is not the §8.3 contract; the contract is "given the same input event sequence, the resulting state is identical." Test 3 makes this explicit by re-enqueueing the exact same events before the second replay.

M18's `rollback(snapshot)` deliberately preserves `pending_events_` (covered by the existing `RollbackPreservesPendingEventsForReplay` test) so that an unfinished sub-step can resume after rollback. This means any off-path `enqueue_event` performed between snapshot and rollback survives the rollback and will be consumed by the next `replay_pending()`. Test 2 therefore drains any off-path events with an explicit `replay_pending()` **before** rolling back, so the rolled-back state and the fresh-baseline state truly diverge only by undo-able fields (`cells_` and `runtime_counters_`).

## Boundaries

- No edits to `libs/coupling/core/` headers or sources.
- No new test target; tests join the existing `test_coupling_core_state` binary.
- No new symbols, types, or APIs.
- No reliance on internal members; tests use only the public API.
- No relaxation of strict equality; if §8.3 is found to be violated by current code, that is a real bug to be fixed in a follow-up milestone — M25 surfaces it rather than papering over it.

# M25 CouplingLib Core Deterministic Replay Regression Evidence

**Date:** 2026-05-24
**Design:** `superpowers/specs/2026-05-24-m25-coupling-core-deterministic-replay-regression-design.md`
**Plan:** `superpowers/plans/2026-05-24-plan-m25-coupling-core-deterministic-replay-regression.md`

## Scope

M25 locks the §8.3 deterministic-replay strict zero-diff invariant inside the current `CouplingLib core` API as machine-asserted regression tests. No production code is touched.

## §8.3 Coverage Matrix

| §8.3 field | M25 coverage |
|---|---|
| `mass_deficit_account` | covered (all 3 tests) |
| `exchange buffer` (`pending_events_` semantics) | covered indirectly via `ReReplayAfterRollbackAndReEnqueueIsBitwiseIdentical` |
| `RuntimeCounters` | covered (all 3 tests) |
| `h/hu/hv` | out of scope until `Surface2DCore` snapshot landing |
| FaultController state | out of scope until `FaultController` lands |
| `dt_couple` slicing | out of scope until scheduler lands |

## Local Validation

- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_core_state --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_core_state.exe`: PASS, 18/18 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_exchange_pipeline --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_exchange_pipeline.exe`: PASS, 6/6 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_nonnegative_storage_backoff --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_nonnegative_storage_backoff.exe`: PASS, 5/5 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_drain_split --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_drain_split.exe`: PASS, 6/6 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_exchange_decision --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_exchange_decision.exe`: PASS, 5/5 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_mass_deficit_account --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_mass_deficit_account.exe`: PASS, 5/5 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_flow_limit --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_flow_limit.exe`: PASS, 4/4 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure`: PASS, full CTest passed 29/29 (8.23s).

## Assertion Discipline

All asserts use strict equality (`EXPECT_DOUBLE_EQ`, `EXPECT_EQ`). No tolerances. §8.3 explicitly forbids tolerance-based comparisons on the correctness path.

## Coverage

- `test_coupling_core_state.DeterministicReplayPreservesCellsAndCountersWithIdenticalSequence`: two independent states driven by the same sequence yield byte-equal cells, deficit, and counters.
- `test_coupling_core_state.RollbackAndReplayProducesIdenticalStateToFreshRun`: state that takes a snapshot, makes off-path mutations (including a replay-drained 999 event so `pending_events_` does not carry across rollback), rolls back, and re-runs the authoritative sequence ends bitwise-equal to a state that runs the authoritative sequence directly from the snapshot baseline.
- `test_coupling_core_state.ReReplayAfterRollbackAndReEnqueueIsBitwiseIdentical`: snapshot → enqueue → replay → record yields the same terminal state as snapshot → enqueue → replay → record (re-run after rollback with the same event sequence).

## Pending-Events Semantics Note

M18's `rollback(snapshot)` deliberately preserves `pending_events_` (covered by the existing `RollbackPreservesPendingEventsForReplay` test) so an unfinished sub-step can resume after rollback. Test 2 therefore drains any off-path events with an explicit `replay_pending()` **before** rolling back, so the rolled-back state and the fresh baseline truly diverge only by undo-able fields (`cells_` and `runtime_counters_`). Without this drain the off-path enqueue would survive rollback and pollute the authoritative replay path — which is exactly what the first run of this test exposed and informed the final shape.

## Boundaries

- M25 does not edit `libs/coupling/core/` headers or sources.
- M25 does not introduce multi-donor arbitration, `priority_weight`, `V_limit_k`, DTO refactor, or driver functions.
- M25 does not introduce snapshot persistence, schema serialization, or IO.
- M25 does not cover §8.3 fields outside the current `CouplingLib core` API (`h/hu/hv`, FaultController, `dt_couple`).
- M25 does not connect to `Surface2DCore`, SWMM, D-Flow FM, or any 1D engine ABI.

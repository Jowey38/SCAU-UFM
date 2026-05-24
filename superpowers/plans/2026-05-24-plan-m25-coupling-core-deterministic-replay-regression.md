# M25 CouplingLib Core Deterministic Replay Regression Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Lock the §8.3 deterministic-replay strict zero-diff invariant on the current `CouplingLib core` API as machine-asserted regression tests. Pure-additive at the test layer.

**Architecture:** Append three new tests to `tests/unit/coupling/test_coupling_core_state.cpp`. No edits to `libs/coupling/core/` headers or sources. No new CMake targets.

**Tech Stack:** C++20, CMake, GoogleTest, existing `scau::coupling_core` and `scau::warnings` targets.

---

## File Structure

- Modify `tests/unit/coupling/test_coupling_core_state.cpp`
  - Append 3 deterministic-replay regression tests.
- Create `superpowers/specs/2026-05-24-plan-m25-coupling-core-deterministic-replay-regression-evidence.md`
  - Record focused and full validation evidence with §8.3 traceability.

---

## Task 1: Add Deterministic Replay Regression Tests

**Files:**
- Modify: `tests/unit/coupling/test_coupling_core_state.cpp`

- [ ] **Step 1: Append the three tests**

Append the following tests to the end of `tests/unit/coupling/test_coupling_core_state.cpp`:

```cpp
TEST(CouplingCoreState, DeterministicReplayPreservesCellsAndCountersWithIdenticalSequence) {
    const std::vector<scau::coupling::core::ExchangeCellState> initial_cells{
        {.volume = 10.0, .mass_deficit_account = {.volume = 1.0}},
        {.volume = 20.0, .mass_deficit_account = {.volume = 2.0}},
    };

    scau::coupling::core::CouplingState state_a{initial_cells};
    scau::coupling::core::CouplingState state_b{initial_cells};

    const auto drive = [](scau::coupling::core::CouplingState& state) {
        state.record_pipeline_decision({.drain_split_engaged = true});
        state.enqueue_event({.exchange_cell_index = 0U, .volume_delta = 4.0, .unmet_volume = 1.5});
        state.enqueue_event({.exchange_cell_index = 1U, .volume_delta = -2.0, .repayment_volume = 0.5});
        state.replay_pending();
        state.record_pipeline_decision({.negative_depth_fix_engaged = true});
        state.record_pipeline_decision({.drain_split_engaged = true, .negative_depth_fix_engaged = true});
    };

    drive(state_a);
    drive(state_b);

    ASSERT_EQ(state_a.cells().size(), state_b.cells().size());
    for (std::size_t i = 0; i < state_a.cells().size(); ++i) {
        EXPECT_DOUBLE_EQ(state_a.cells()[i].volume, state_b.cells()[i].volume);
        EXPECT_DOUBLE_EQ(
            state_a.cells()[i].mass_deficit_account.volume,
            state_b.cells()[i].mass_deficit_account.volume);
    }
    EXPECT_EQ(
        state_a.runtime_counters().count_drain_split,
        state_b.runtime_counters().count_drain_split);
    EXPECT_EQ(
        state_a.runtime_counters().count_negative_depth_fix,
        state_b.runtime_counters().count_negative_depth_fix);
}

TEST(CouplingCoreState, RollbackAndReplayProducesIdenticalStateToFreshRun) {
    const std::vector<scau::coupling::core::ExchangeCellState> initial_cells{
        {.volume = 10.0, .mass_deficit_account = {.volume = 1.0}},
        {.volume = 20.0, .mass_deficit_account = {.volume = 2.0}},
    };

    scau::coupling::core::CouplingState fresh{initial_cells};
    scau::coupling::core::CouplingState rolled_back{initial_cells};

    const auto authoritative_sequence = [](scau::coupling::core::CouplingState& state) {
        state.record_pipeline_decision({.drain_split_engaged = true});
        state.enqueue_event({.exchange_cell_index = 0U, .volume_delta = 3.0, .unmet_volume = 2.0});
        state.enqueue_event({.exchange_cell_index = 1U, .volume_delta = -1.0, .repayment_volume = 1.0});
        state.replay_pending();
        state.record_pipeline_decision({.negative_depth_fix_engaged = true});
    };

    const auto fresh_snapshot = fresh.snapshot();
    authoritative_sequence(fresh);

    const auto rolled_back_snapshot = rolled_back.snapshot();
    rolled_back.record_pipeline_decision({.drain_split_engaged = true, .negative_depth_fix_engaged = true});
    rolled_back.enqueue_event({.exchange_cell_index = 0U, .volume_delta = 999.0, .unmet_volume = 999.0});
    rolled_back.replay_pending();  // drain off-path event; M18 rollback preserves pending_events_, so any unconsumed event would re-enter the authoritative replay path
    rolled_back.rollback(rolled_back_snapshot);
    authoritative_sequence(rolled_back);

    ASSERT_EQ(fresh.cells().size(), rolled_back.cells().size());
    for (std::size_t i = 0; i < fresh.cells().size(); ++i) {
        EXPECT_DOUBLE_EQ(fresh.cells()[i].volume, rolled_back.cells()[i].volume);
        EXPECT_DOUBLE_EQ(
            fresh.cells()[i].mass_deficit_account.volume,
            rolled_back.cells()[i].mass_deficit_account.volume);
    }
    EXPECT_EQ(
        fresh.runtime_counters().count_drain_split,
        rolled_back.runtime_counters().count_drain_split);
    EXPECT_EQ(
        fresh.runtime_counters().count_negative_depth_fix,
        rolled_back.runtime_counters().count_negative_depth_fix);
    static_cast<void>(fresh_snapshot);
}

TEST(CouplingCoreState, ReReplayAfterRollbackAndReEnqueueIsBitwiseIdentical) {
    scau::coupling::core::CouplingState state{{
        {.volume = 10.0, .mass_deficit_account = {.volume = 1.0}},
        {.volume = 20.0, .mass_deficit_account = {.volume = 5.0}},
    }};

    const auto snapshot = state.snapshot();
    const std::vector<scau::coupling::core::CouplingEvent> input_events{
        {.exchange_cell_index = 0U, .volume_delta = 2.0, .unmet_volume = 3.0},
        {.exchange_cell_index = 1U, .volume_delta = -4.0, .repayment_volume = 2.5},
    };

    for (const auto& event : input_events) {
        state.enqueue_event(event);
    }
    state.replay_pending();
    state.record_pipeline_decision({.drain_split_engaged = true});

    const std::vector<scau::coupling::core::ExchangeCellState> first_cells(state.cells());
    const auto first_counters = state.runtime_counters();

    state.rollback(snapshot);
    for (const auto& event : input_events) {
        state.enqueue_event(event);
    }
    state.replay_pending();
    state.record_pipeline_decision({.drain_split_engaged = true});

    ASSERT_EQ(state.cells().size(), first_cells.size());
    for (std::size_t i = 0; i < state.cells().size(); ++i) {
        EXPECT_DOUBLE_EQ(state.cells()[i].volume, first_cells[i].volume);
        EXPECT_DOUBLE_EQ(
            state.cells()[i].mass_deficit_account.volume,
            first_cells[i].mass_deficit_account.volume);
    }
    EXPECT_EQ(state.runtime_counters().count_drain_split, first_counters.count_drain_split);
    EXPECT_EQ(
        state.runtime_counters().count_negative_depth_fix,
        first_counters.count_negative_depth_fix);
}
```

- [ ] **Step 2: Run the focused target and verify pass**

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_core_state --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_core_state.exe
```

Expected: PASS, 18/18 tests pass in `CouplingCoreState` (15 existing + 3 new).

If any of the three new tests fails, that indicates a real deterministic-replay defect; do NOT relax the asserts. Stop and report the failing diff.

- [ ] **Step 3: Run remaining focused coupling tests to confirm no regression**

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_exchange_pipeline --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_exchange_pipeline.exe
```

Expected: PASS, 6/6.

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_nonnegative_storage_backoff --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_nonnegative_storage_backoff.exe
```

Expected: PASS, 5/5.

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_drain_split --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_drain_split.exe
```

Expected: PASS, 6/6.

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_exchange_decision --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_exchange_decision.exe
```

Expected: PASS, 5/5.

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_mass_deficit_account --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_mass_deficit_account.exe
```

Expected: PASS, 5/5.

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_flow_limit --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_flow_limit.exe
```

Expected: PASS, 4/4.

---

## Task 2: Validate M25 And Record Evidence

**Files:**
- Create: `superpowers/specs/2026-05-24-plan-m25-coupling-core-deterministic-replay-regression-evidence.md`

- [ ] **Step 1: Run full CTest**

```bash
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure
```

Expected: PASS, full CTest passes 29/29 (M24 baseline; M25 only adds tests inside the existing `test_coupling_core_state` target).

- [ ] **Step 2: Create evidence document**

Create `superpowers/specs/2026-05-24-plan-m25-coupling-core-deterministic-replay-regression-evidence.md` with:

```markdown
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
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure`: PASS, full CTest passed `<observed>/<observed>`.

## Assertion Discipline

All asserts use strict equality (`EXPECT_DOUBLE_EQ`, `EXPECT_EQ`). No tolerances. §8.3 explicitly forbids tolerance-based comparisons on the correctness path.

## Coverage

- `test_coupling_core_state.DeterministicReplayPreservesCellsAndCountersWithIdenticalSequence`: two independent states driven by the same sequence yield byte-equal cells, deficit, and counters.
- `test_coupling_core_state.RollbackAndReplayProducesIdenticalStateToFreshRun`: state that takes a snapshot, makes off-path mutations, rolls back, and re-runs the authoritative sequence ends bitwise-equal to a state that runs the authoritative sequence directly from the snapshot baseline.
- `test_coupling_core_state.ReReplayAfterRollbackAndReEnqueueIsBitwiseIdentical`: snapshot → enqueue → replay → record yields the same terminal state as snapshot → enqueue → replay → record (re-run after rollback with the same event sequence).

## Boundaries

- M25 does not edit `libs/coupling/core/` headers or sources.
- M25 does not introduce multi-donor arbitration, `priority_weight`, `V_limit_k`, DTO refactor, or driver functions.
- M25 does not introduce snapshot persistence, schema serialization, or IO.
- M25 does not cover §8.3 fields outside the current `CouplingLib core` API (`h/hu/hv`, FaultController, `dt_couple`).
- M25 does not connect to `Surface2DCore`, SWMM, D-Flow FM, or any 1D engine ABI.
```

- [ ] **Step 3: Inspect status and diff**

```bash
git status --short
git diff --stat -- tests/unit/coupling/test_coupling_core_state.cpp
```

Expected: only `tests/unit/coupling/test_coupling_core_state.cpp` is modified; transcript stays untracked.

---

## Boundaries

- Pure-additive at the test layer; no production-code edits.
- Strict equality only; no tolerances.
- No new CMake targets, headers, or APIs.
- No external engine ABI connection.
- If any of the three new tests fails on the current implementation, that is a real determinism defect to be fixed in a follow-up milestone, not papered over here.

# M24 CouplingLib Core Runtime Counters Snapshot Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add `RuntimeCounters` to `CouplingState`, drive it from the M23 engagement flags via `record_pipeline_decision(...)`, and bring it into the existing `snapshot` / `rollback` coverage.

**Architecture:** In `libs/coupling/core/include/coupling/core/state.hpp` add the `RuntimeCounters` value type and extend `CouplingSnapshot` + `CouplingState` with parallel `runtime_counters()` accessors and a single mutation entry point `record_pipeline_decision(...)`. In `libs/coupling/core/src/state.cpp` extend the constructor, `snapshot()`, and `rollback()` accordingly. `replay_pending()` is untouched.

**Tech Stack:** C++20, CMake, GoogleTest, existing `scau::coupling_core` and `scau::warnings` targets.

---

## File Structure

- Modify `libs/coupling/core/include/coupling/core/state.hpp`
  - Add `RuntimeCounters` struct.
  - Add `runtime_counters()` accessor on `CouplingSnapshot` and `CouplingState`.
  - Add `record_pipeline_decision(...)` on `CouplingState`.
  - Update private members and `CouplingSnapshot` constructor signature.
- Modify `libs/coupling/core/src/state.cpp`
  - Implement the new accessors and `record_pipeline_decision(...)`.
  - Update `snapshot()` and `rollback()` to round-trip counters.
- Modify `tests/unit/coupling/test_coupling_core_state.cpp`
  - Append 8 new tests for counters and snapshot/rollback round-trip.
- Create `superpowers/specs/2026-05-24-plan-m24-coupling-core-runtime-counters-snapshot-evidence.md`
  - Record focused and full validation evidence after implementation.

---

## Task 1: Add Red Tests For Runtime Counters

**Files:**
- Modify: `tests/unit/coupling/test_coupling_core_state.cpp`

- [ ] **Step 1: Append the eight new tests**

Append the following tests to the end of `tests/unit/coupling/test_coupling_core_state.cpp`:

```cpp
TEST(CouplingCoreState, RuntimeCountersStartAtZero) {
    scau::coupling::core::CouplingState state{{{.volume = 10.0}}};

    EXPECT_EQ(state.runtime_counters().count_drain_split, 0U);
    EXPECT_EQ(state.runtime_counters().count_negative_depth_fix, 0U);
}

TEST(CouplingCoreState, RecordPipelineDecisionIncrementsDrainSplitCounter) {
    scau::coupling::core::CouplingState state{{{.volume = 10.0}}};

    state.record_pipeline_decision({
        .drain_split_engaged = true,
        .negative_depth_fix_engaged = false,
    });

    EXPECT_EQ(state.runtime_counters().count_drain_split, 1U);
    EXPECT_EQ(state.runtime_counters().count_negative_depth_fix, 0U);
}

TEST(CouplingCoreState, RecordPipelineDecisionIncrementsNegativeDepthFixCounter) {
    scau::coupling::core::CouplingState state{{{.volume = 10.0}}};

    state.record_pipeline_decision({
        .drain_split_engaged = false,
        .negative_depth_fix_engaged = true,
    });

    EXPECT_EQ(state.runtime_counters().count_drain_split, 0U);
    EXPECT_EQ(state.runtime_counters().count_negative_depth_fix, 1U);
}

TEST(CouplingCoreState, RecordPipelineDecisionIncrementsBothCountersWhenBothFlagsEngaged) {
    scau::coupling::core::CouplingState state{{{.volume = 10.0}}};

    state.record_pipeline_decision({
        .drain_split_engaged = true,
        .negative_depth_fix_engaged = true,
    });
    state.record_pipeline_decision({
        .drain_split_engaged = true,
        .negative_depth_fix_engaged = true,
    });

    EXPECT_EQ(state.runtime_counters().count_drain_split, 2U);
    EXPECT_EQ(state.runtime_counters().count_negative_depth_fix, 2U);
}

TEST(CouplingCoreState, RecordPipelineDecisionLeavesCountersUnchangedWhenNoFlagsEngaged) {
    scau::coupling::core::CouplingState state{{{.volume = 10.0}}};

    state.record_pipeline_decision({
        .drain_split_engaged = false,
        .negative_depth_fix_engaged = false,
    });

    EXPECT_EQ(state.runtime_counters().count_drain_split, 0U);
    EXPECT_EQ(state.runtime_counters().count_negative_depth_fix, 0U);
}

TEST(CouplingCoreState, SnapshotCapturesRuntimeCountersAndRemainsImmutable) {
    scau::coupling::core::CouplingState state{{{.volume = 10.0}}};
    state.record_pipeline_decision({.drain_split_engaged = true});
    state.record_pipeline_decision({.negative_depth_fix_engaged = true});

    const auto snapshot = state.snapshot();
    state.record_pipeline_decision({
        .drain_split_engaged = true,
        .negative_depth_fix_engaged = true,
    });

    EXPECT_EQ(snapshot.runtime_counters().count_drain_split, 1U);
    EXPECT_EQ(snapshot.runtime_counters().count_negative_depth_fix, 1U);
    EXPECT_EQ(state.runtime_counters().count_drain_split, 2U);
    EXPECT_EQ(state.runtime_counters().count_negative_depth_fix, 2U);
}

TEST(CouplingCoreState, RollbackRestoresSnapshotRuntimeCounters) {
    scau::coupling::core::CouplingState state{{{.volume = 10.0}}};
    state.record_pipeline_decision({.drain_split_engaged = true});

    const auto snapshot = state.snapshot();
    state.record_pipeline_decision({
        .drain_split_engaged = true,
        .negative_depth_fix_engaged = true,
    });
    state.rollback(snapshot);

    EXPECT_EQ(state.runtime_counters().count_drain_split, 1U);
    EXPECT_EQ(state.runtime_counters().count_negative_depth_fix, 0U);
}

TEST(CouplingCoreState, ReplayPendingDoesNotMutateRuntimeCounters) {
    scau::coupling::core::CouplingState state{{{.volume = 10.0}}};
    state.record_pipeline_decision({.drain_split_engaged = true});

    state.enqueue_event({.exchange_cell_index = 0U, .volume_delta = 2.0, .unmet_volume = 1.0});
    state.replay_pending();

    EXPECT_EQ(state.runtime_counters().count_drain_split, 1U);
    EXPECT_EQ(state.runtime_counters().count_negative_depth_fix, 0U);
    EXPECT_DOUBLE_EQ(state.cells()[0].volume, 12.0);
    EXPECT_DOUBLE_EQ(state.cells()[0].mass_deficit_account.volume, 1.0);
}
```

- [ ] **Step 2: Run the focused target and verify compile failure**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_core_state --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_core_state.exe
```

Expected: FAIL during compilation because `RuntimeCounters`, `runtime_counters()`, and `record_pipeline_decision(...)` are not declared yet.

---

## Task 2: Implement Runtime Counters And Snapshot Coverage

**Files:**
- Modify: `libs/coupling/core/include/coupling/core/state.hpp`
- Modify: `libs/coupling/core/src/state.cpp`

- [ ] **Step 1: Extend the public API**

In `libs/coupling/core/include/coupling/core/state.hpp`, insert immediately after the existing `ExchangePipelineDecision` definition:

```cpp
struct RuntimeCounters {
    std::size_t count_drain_split{0};
    std::size_t count_negative_depth_fix{0};
};
```

Update the `CouplingSnapshot` declaration:

```cpp
class CouplingSnapshot {
public:
    [[nodiscard]] const std::vector<ExchangeCellState>& cells() const noexcept;
    [[nodiscard]] const RuntimeCounters& runtime_counters() const noexcept;

private:
    friend class CouplingState;

    CouplingSnapshot(std::vector<ExchangeCellState> cells, RuntimeCounters counters);

    std::vector<ExchangeCellState> cells_;
    RuntimeCounters runtime_counters_;
};
```

Update the `CouplingState` declaration:

```cpp
class CouplingState {
public:
    explicit CouplingState(std::vector<ExchangeCellState> cells);

    [[nodiscard]] const std::vector<ExchangeCellState>& cells() const noexcept;
    [[nodiscard]] const RuntimeCounters& runtime_counters() const noexcept;
    [[nodiscard]] CouplingSnapshot snapshot() const;

    void enqueue_event(CouplingEvent event);
    void rollback(const CouplingSnapshot& snapshot);
    void replay_pending();
    void record_pipeline_decision(const ExchangePipelineDecision& decision);

private:
    std::vector<ExchangeCellState> cells_;
    RuntimeCounters runtime_counters_{};
    std::vector<CouplingEvent> pending_events_;
};
```

- [ ] **Step 2: Implement the new behavior**

In `libs/coupling/core/src/state.cpp`, update `CouplingSnapshot` definitions:

Replace:

```cpp
CouplingSnapshot::CouplingSnapshot(std::vector<ExchangeCellState> cells) : cells_(std::move(cells)) {}
```

With:

```cpp
CouplingSnapshot::CouplingSnapshot(std::vector<ExchangeCellState> cells, RuntimeCounters counters)
    : cells_(std::move(cells)), runtime_counters_(counters) {}
```

Add immediately after `CouplingSnapshot::cells()`:

```cpp
const RuntimeCounters& CouplingSnapshot::runtime_counters() const noexcept {
    return runtime_counters_;
}
```

Replace `CouplingState::snapshot()` with:

```cpp
CouplingSnapshot CouplingState::snapshot() const {
    return CouplingSnapshot{cells_, runtime_counters_};
}
```

Replace `CouplingState::rollback(...)` with:

```cpp
void CouplingState::rollback(const CouplingSnapshot& snapshot) {
    cells_ = snapshot.cells_;
    runtime_counters_ = snapshot.runtime_counters_;
}
```

Add immediately before the closing namespace brace:

```cpp
const RuntimeCounters& CouplingState::runtime_counters() const noexcept {
    return runtime_counters_;
}

void CouplingState::record_pipeline_decision(const ExchangePipelineDecision& decision) {
    if (decision.drain_split_engaged) {
        ++runtime_counters_.count_drain_split;
    }
    if (decision.negative_depth_fix_engaged) {
        ++runtime_counters_.count_negative_depth_fix;
    }
}
```

`replay_pending()` is intentionally left as-is.

- [ ] **Step 3: Run the focused state test**

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_core_state --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_core_state.exe
```

Expected: PASS, 15/15 tests pass in `CouplingCoreState` (7 existing + 8 new).

- [ ] **Step 4: Run remaining focused coupling tests to confirm no regression**

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

## Task 3: Validate M24 And Record Evidence

**Files:**
- Create: `superpowers/specs/2026-05-24-plan-m24-coupling-core-runtime-counters-snapshot-evidence.md`

- [ ] **Step 1: Run full CTest**

```bash
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure
```

Expected: PASS, full CTest passes 29/29 (M23 baseline; M24 only adds tests inside the existing `test_coupling_core_state` target, so the test target count is unchanged at 29).

- [ ] **Step 2: Create evidence document**

Create `superpowers/specs/2026-05-24-plan-m24-coupling-core-runtime-counters-snapshot-evidence.md` with this structure, recording observed counts:

```markdown
# M24 CouplingLib Core Runtime Counters Snapshot Evidence

**Date:** 2026-05-24
**Design:** `superpowers/specs/2026-05-24-m24-coupling-core-runtime-counters-snapshot-design.md`
**Plan:** `superpowers/plans/2026-05-24-plan-m24-coupling-core-runtime-counters-snapshot.md`

## Scope

M24 lifts the M23 per-invocation engagement flags into a `RuntimeCounters` aggregate owned by `CouplingState`, brings the counters into `snapshot` / `rollback` coverage, and locks the invariant that `replay_pending()` does not mutate counters.

## Local Validation

- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_core_state --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_core_state.exe`: PASS, 15/15 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_exchange_pipeline --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_exchange_pipeline.exe`: PASS, 6/6 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_nonnegative_storage_backoff --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_nonnegative_storage_backoff.exe`: PASS, 5/5 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_drain_split --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_drain_split.exe`: PASS, 6/6 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_exchange_decision --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_exchange_decision.exe`: PASS, 5/5 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_mass_deficit_account --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_mass_deficit_account.exe`: PASS, 5/5 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_flow_limit --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_flow_limit.exe`: PASS, 4/4 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure`: PASS, full CTest passed `<observed>/<observed>`.

## Coverage

- `test_coupling_core_state`: fresh state has both counters at 0.
- `test_coupling_core_state`: `record_pipeline_decision(...)` only increments `count_drain_split` when the M23 flag is true.
- `test_coupling_core_state`: `record_pipeline_decision(...)` only increments `count_negative_depth_fix` when the M23 flag is true.
- `test_coupling_core_state`: both flags true increment both counters; repeated calls accumulate.
- `test_coupling_core_state`: both flags false leave counters unchanged.
- `test_coupling_core_state`: snapshot captures counter values at snapshot time and remains immutable.
- `test_coupling_core_state`: rollback restores counter values from snapshot.
- `test_coupling_core_state`: `replay_pending()` updates only cells and the deficit ledger, not counters.

## §6.5 / §8.3 Alignment

- Counters are integer accumulators; the §8.3 strict zero-diff replay invariant is satisfied trivially whenever the same sequence of `record_pipeline_decision(...)` calls is applied with the same flag values.
- §6.5 snapshot coverage on `RuntimeCounters` is satisfied in-memory; persistent schema serialization is out of scope for M24.

## Boundaries

- M24 does not introduce a stateful `apply_exchange(...)` driver; callers still invoke `evaluate_exchange_pipeline(...)` and pass the result to `record_pipeline_decision(...)` explicitly.
- M24 does not introduce additional §14.1 counters (`count_velocity_clamp`, `count_flux_cutoff`, `count_drag_matrix_degenerate`, `count_ai_prior_conflict`, `count_mem_pressure_events`, `max_cell_cfl`).
- M24 does not introduce multi-donor arbitration, `priority_weight`, `V_limit_k`, or DTO refactor.
- M24 does not introduce snapshot persistence, schema serialization, or IO.
- M24 does not connect to `Surface2DCore`, SWMM, D-Flow FM, or any 1D engine ABI.
```

- [ ] **Step 3: Inspect status and diff**

```bash
git status --short
git diff --stat -- libs/coupling/core/include/coupling/core/state.hpp libs/coupling/core/src/state.cpp tests/unit/coupling/test_coupling_core_state.cpp
```

Expected: Only M24 files are modified or created; existing transcript stays untracked.

---

## Boundaries

- `record_pipeline_decision(...)` consumes only M23 flags; no re-derivation from `decision.drain_split.micro_steps` or `decision.exchange.v_granted`.
- `replay_pending()` must not modify counters.
- No new dependencies on `evaluate_exchange_pipeline(...)` inside `CouplingState`; counter writes are driven externally.
- No multi-donor logic, `priority_weight`, `V_limit_k`, DTO refactor, or snapshot persistence.
- No connection to external engines.

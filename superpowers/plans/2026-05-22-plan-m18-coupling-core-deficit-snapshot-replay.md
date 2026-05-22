# M18 CouplingLib Core Deficit Snapshot Replay Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Integrate `mass_deficit_account` into `CouplingState` snapshot, rollback, and replay so deficit ledger state follows the same pure-core persistence path as exchange cell volume.

**Architecture:** Embed one `MassDeficitAccount` in each `ExchangeCellState` and extend `CouplingEvent` with unmet and repayment volumes. Reuse M17's pure `roll_deficit(...)` and `apply_repayment(...)` functions during replay, leaving existing volume replay behavior unchanged.

**Tech Stack:** C++20, CMake, GoogleTest, existing `scau::coupling_core` and `scau::warnings` targets.

---

## File Structure

- Modify `libs/coupling/core/include/coupling/core/state.hpp`
  - Move `MassDeficitAccount` before `ExchangeCellState`.
  - Add `MassDeficitAccount mass_deficit_account{}` to `ExchangeCellState`.
  - Add `double unmet_volume{0.0}` and `double repayment_volume{0.0}` to `CouplingEvent`.
- Modify `libs/coupling/core/src/state.cpp`
  - Extend `CouplingState::replay_pending()` to update each cell's deficit ledger using M17 helpers.
- Modify `tests/unit/coupling/test_coupling_core_state.cpp`
  - Add focused snapshot/rollback/replay tests for deficit ledger state.
- Use existing `tests/unit/coupling/test_coupling_mass_deficit_account.cpp`
  - Keep pure ledger behavior tests unchanged.
- Create `superpowers/specs/2026-05-22-plan-m18-coupling-core-deficit-snapshot-replay-evidence.md`
  - Record focused and full validation evidence after implementation.

---

## Task 1: Add Red Tests For Deficit Snapshot Rollback Replay

**Files:**
- Modify: `tests/unit/coupling/test_coupling_core_state.cpp`

- [ ] **Step 1: Add snapshot immutability coverage for deficit ledger state**

Append this test to `tests/unit/coupling/test_coupling_core_state.cpp`:

```cpp
TEST(CouplingCoreState, SnapshotCapturesDeficitAccountsAndRemainsImmutable) {
    scau::coupling::core::CouplingState state{{
        {.volume = 10.0, .mass_deficit_account = {.volume = 1.0}},
        {.volume = 20.0, .mass_deficit_account = {.volume = 2.0}},
    }};

    const auto snapshot = state.snapshot();
    state.enqueue_event({.exchange_cell_index = 0U, .volume_delta = 5.0, .unmet_volume = 3.0});
    state.replay_pending();

    ASSERT_EQ(snapshot.cells().size(), 2U);
    EXPECT_DOUBLE_EQ(snapshot.cells()[0].mass_deficit_account.volume, 1.0);
    EXPECT_DOUBLE_EQ(snapshot.cells()[1].mass_deficit_account.volume, 2.0);
    EXPECT_DOUBLE_EQ(state.cells()[0].mass_deficit_account.volume, 4.0);
}
```

- [ ] **Step 2: Add rollback restore coverage for deficit ledger state**

Append this test to `tests/unit/coupling/test_coupling_core_state.cpp`:

```cpp
TEST(CouplingCoreState, RollbackRestoresSnapshotDeficitAccounts) {
    scau::coupling::core::CouplingState state{{
        {.volume = 10.0, .mass_deficit_account = {.volume = 1.0}},
        {.volume = 20.0, .mass_deficit_account = {.volume = 2.0}},
    }};

    const auto snapshot = state.snapshot();
    state.enqueue_event({.exchange_cell_index = 0U, .volume_delta = -3.0, .unmet_volume = 4.0});
    state.enqueue_event({.exchange_cell_index = 1U, .volume_delta = 7.0, .repayment_volume = 1.5});
    state.replay_pending();
    state.rollback(snapshot);

    ASSERT_EQ(state.cells().size(), 2U);
    EXPECT_DOUBLE_EQ(state.cells()[0].mass_deficit_account.volume, 1.0);
    EXPECT_DOUBLE_EQ(state.cells()[1].mass_deficit_account.volume, 2.0);
}
```

- [ ] **Step 3: Add rollback replay coverage for pending deficit events**

Append this test to `tests/unit/coupling/test_coupling_core_state.cpp`:

```cpp
TEST(CouplingCoreState, RollbackPreservesPendingDeficitEventsForReplay) {
    scau::coupling::core::CouplingState state{{
        {.volume = 10.0, .mass_deficit_account = {.volume = 1.0}},
        {.volume = 20.0, .mass_deficit_account = {.volume = 5.0}},
    }};

    const auto snapshot = state.snapshot();
    state.enqueue_event({.exchange_cell_index = 0U, .volume_delta = 2.0, .unmet_volume = 3.0});
    state.enqueue_event({.exchange_cell_index = 1U, .volume_delta = -4.0, .repayment_volume = 2.5});
    state.rollback(snapshot);
    state.replay_pending();

    ASSERT_EQ(state.cells().size(), 2U);
    EXPECT_DOUBLE_EQ(state.cells()[0].volume, 12.0);
    EXPECT_DOUBLE_EQ(state.cells()[0].mass_deficit_account.volume, 4.0);
    EXPECT_DOUBLE_EQ(state.cells()[1].volume, 16.0);
    EXPECT_DOUBLE_EQ(state.cells()[1].mass_deficit_account.volume, 2.5);
}
```

- [ ] **Step 4: Run focused core state test and verify it fails before implementation**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_core_state --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_core_state.exe
```

Expected: FAIL during compilation because `ExchangeCellState` has no `mass_deficit_account` field and `CouplingEvent` has no `unmet_volume` or `repayment_volume` fields.

---

## Task 2: Implement Deficit State Integration

**Files:**
- Modify: `libs/coupling/core/include/coupling/core/state.hpp`
- Modify: `libs/coupling/core/src/state.cpp`

- [ ] **Step 1: Move and embed `MassDeficitAccount` in the public state API**

In `libs/coupling/core/include/coupling/core/state.hpp`, change the top of the namespace to this shape:

```cpp
namespace scau::coupling::core {

struct MassDeficitAccount {
    double volume{0.0};
};

struct ExchangeCellState {
    double volume{0.0};
    MassDeficitAccount mass_deficit_account{};
    double phi_t{0.0};
    double h{0.0};
    double area{0.0};
};
```

Keep these declarations after `compute_flow_limit(...)`:

```cpp
[[nodiscard]] MassDeficitAccount roll_deficit(const MassDeficitAccount& account, double unmet_volume);
[[nodiscard]] MassDeficitAccount apply_repayment(const MassDeficitAccount& account, double applied_volume);
```

- [ ] **Step 2: Extend `CouplingEvent` with deficit replay fields**

In `libs/coupling/core/include/coupling/core/state.hpp`, change `CouplingEvent` to:

```cpp
struct CouplingEvent {
    std::size_t exchange_cell_index{0U};
    double volume_delta{0.0};
    double unmet_volume{0.0};
    double repayment_volume{0.0};
};
```

- [ ] **Step 3: Replay deficit ledger deltas with existing M17 helpers**

In `libs/coupling/core/src/state.cpp`, change `CouplingState::replay_pending()` to:

```cpp
void CouplingState::replay_pending() {
    for (const auto& event : pending_events_) {
        auto& cell = cells_[event.exchange_cell_index];
        cell.volume += event.volume_delta;
        cell.mass_deficit_account = roll_deficit(cell.mass_deficit_account, event.unmet_volume);
        cell.mass_deficit_account = apply_repayment(cell.mass_deficit_account, event.repayment_volume);
    }
    pending_events_.clear();
}
```

- [ ] **Step 4: Run focused core state test and verify it passes**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_core_state --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_core_state.exe
```

Expected: PASS, `7 tests` pass in `CouplingCoreState`.

- [ ] **Step 5: Run focused mass deficit account test and flow limit test**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_mass_deficit_account --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_mass_deficit_account.exe
```

Expected: PASS, `5 tests` pass in `CouplingMassDeficitAccount`.

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_flow_limit --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_flow_limit.exe
```

Expected: PASS, `4 tests` pass in `CouplingFlowLimit`.

---

## Task 3: Validate M18 And Record Evidence

**Files:**
- Create: `superpowers/specs/2026-05-22-plan-m18-coupling-core-deficit-snapshot-replay-evidence.md`

- [ ] **Step 1: Run full CTest**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure
```

Expected: PASS, full CTest passes with all tests successful.

- [ ] **Step 2: Create M18 evidence document**

Create `superpowers/specs/2026-05-22-plan-m18-coupling-core-deficit-snapshot-replay-evidence.md` with this structure, filling the observed pass counts from the command output:

```markdown
# M18 CouplingLib Core Deficit Snapshot Replay Evidence

**Date:** 2026-05-22
**Design:** `superpowers/specs/2026-05-22-m18-coupling-core-deficit-snapshot-replay-design.md`
**Plan:** `superpowers/plans/2026-05-22-plan-m18-coupling-core-deficit-snapshot-replay.md`

## Scope

M18 integrates `mass_deficit_account` into the pure `CouplingState` snapshot, rollback, and replay path.

## Local Validation

- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_core_state --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_core_state.exe`: PASS, 7/7 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_mass_deficit_account --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_mass_deficit_account.exe`: PASS, 5/5 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_flow_limit --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_flow_limit.exe`: PASS, 4/4 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure`: PASS, full CTest passed `<observed>/<observed>`.

## Coverage

- `test_coupling_core_state`: snapshots capture deficit account values and remain immutable after replay.
- `test_coupling_core_state`: rollback restores deficit account values from the snapshot.
- `test_coupling_core_state`: rollback preserves pending deficit events for replay.
- `test_coupling_mass_deficit_account`: existing roll-forward and repayment semantics remain intact.
- `test_coupling_flow_limit`: existing `V_limit` / `Q_limit` behavior remains intact.

## Boundaries

- M18 does not connect to `Surface2DCore`, SWMM, D-Flow FM, or any 1D engine ABI.
- M18 does not add shared-cell arbitration, `priority_weight`, scheduler behavior, fault orchestration, or write-off logic.
- M18 does not redefine `Q_limit`, `V_limit`, or `epsilon_deficit` semantics.
```

- [ ] **Step 3: Inspect status and diff**

Run:

```bash
git status --short
git diff -- libs/coupling/core/include/coupling/core/state.hpp libs/coupling/core/src/state.cpp tests/unit/coupling/test_coupling_core_state.cpp superpowers/specs/2026-05-22-m18-coupling-core-deficit-snapshot-replay-design.md superpowers/plans/2026-05-22-plan-m18-coupling-core-deficit-snapshot-replay.md superpowers/specs/2026-05-22-plan-m18-coupling-core-deficit-snapshot-replay-evidence.md
```

Expected: Only M18 files are modified or created, plus any pre-existing untracked transcript remains excluded from future commits.

---

## Boundaries

- Preserve `Q_limit` / `V_limit` behavior and machine-facing names.
- Keep `mass_deficit_account` correctness strict; do not introduce `epsilon_deficit` tolerance in M18.
- Do not introduce arbitration, `priority_weight`, shared-cell splitting, scheduler, fault, or write-off logic.
- Do not connect `mass_deficit_account` to SWMM, D-Flow FM, `Surface2DCore`, or any 1D engine adapter in this slice.

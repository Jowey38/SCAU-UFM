# M15 CouplingLib Core Snapshot/Rollback/Replay Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Keep M15 limited to pure `CouplingLib` core state container semantics. Do not connect SWMM, D-Flow FM, `Surface2DCore`, or any 1D engine ABI.

**Goal:** Add the minimal `CouplingLib` core library slice that supports exchange-cell state snapshots, rollback, and deterministic replay of queued core events.

**Architecture:** Create a new `scau::coupling_core` library under `libs/coupling/core`. `CouplingState` owns exchange-cell volumes and queued replay events, `CouplingSnapshot` owns an immutable copy of cell state, and rollback restores saved cells while preserving pending replay work.

**Tech Stack:** C++20, CMake, GoogleTest, existing `scau::warnings` target.

---

## File Structure

- Create `libs/coupling/core/CMakeLists.txt`
  - Defines `scau_coupling_core` and alias `scau::coupling_core`.
- Create `libs/coupling/core/include/coupling/core/state.hpp`
  - Declares `ExchangeCellState`, `CouplingEvent`, `CouplingSnapshot`, and `CouplingState`.
- Create `libs/coupling/core/src/state.cpp`
  - Implements snapshot, enqueue, rollback, and replay behavior.
- Modify `CMakeLists.txt`
  - Adds `add_subdirectory(libs/coupling/core)`.
- Create `tests/unit/coupling/CMakeLists.txt`
  - Registers `test_coupling_core_state`.
- Create `tests/unit/coupling/test_coupling_core_state.cpp`
  - Adds focused unit tests for M15 semantics.
- Modify `tests/CMakeLists.txt`
  - Adds `add_subdirectory(unit/coupling)`.
- Create `superpowers/specs/2026-05-20-plan-m15-coupling-core-snapshot-rollback-replay-evidence.md`
  - Records focused and full validation evidence after implementation.

---

## Task 1: Add Coupling Core Library Skeleton And Tests

**Files:**
- Create: `libs/coupling/core/CMakeLists.txt`
- Create: `libs/coupling/core/include/coupling/core/state.hpp`
- Create: `libs/coupling/core/src/state.cpp`
- Modify: `CMakeLists.txt`
- Create: `tests/unit/coupling/CMakeLists.txt`
- Create: `tests/unit/coupling/test_coupling_core_state.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Create the coupling core CMake target**

Create `libs/coupling/core/CMakeLists.txt`:

```cmake
add_library(scau_coupling_core
    src/state.cpp
)
add_library(scau::coupling_core ALIAS scau_coupling_core)

target_include_directories(scau_coupling_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(scau_coupling_core
    PRIVATE
        scau::warnings
)

target_compile_features(scau_coupling_core PUBLIC cxx_std_20)
```

- [ ] **Step 2: Register the coupling core library in the root build**

In `CMakeLists.txt`, replace:

```cmake
add_subdirectory(libs/surface2d)
add_subdirectory(tests)
```

with:

```cmake
add_subdirectory(libs/surface2d)
add_subdirectory(libs/coupling/core)
add_subdirectory(tests)
```

- [ ] **Step 3: Create the public state API header**

Create `libs/coupling/core/include/coupling/core/state.hpp`:

```cpp
#pragma once

#include <cstddef>
#include <vector>

namespace scau::coupling::core {

struct ExchangeCellState {
    double volume{0.0};
};

struct CouplingEvent {
    std::size_t exchange_cell_index{0U};
    double volume_delta{0.0};
};

class CouplingSnapshot {
public:
    [[nodiscard]] const std::vector<ExchangeCellState>& cells() const noexcept;

private:
    friend class CouplingState;

    explicit CouplingSnapshot(std::vector<ExchangeCellState> cells);

    std::vector<ExchangeCellState> cells_;
};

class CouplingState {
public:
    explicit CouplingState(std::vector<ExchangeCellState> cells);

    [[nodiscard]] const std::vector<ExchangeCellState>& cells() const noexcept;
    [[nodiscard]] CouplingSnapshot snapshot() const;

    void enqueue_event(CouplingEvent event);
    void rollback(const CouplingSnapshot& snapshot);
    void replay_pending();

private:
    std::vector<ExchangeCellState> cells_;
    std::vector<CouplingEvent> pending_events_;
};

}  // namespace scau::coupling::core
```

- [ ] **Step 4: Create the initial implementation**

Create `libs/coupling/core/src/state.cpp`:

```cpp
#include "coupling/core/state.hpp"

#include <stdexcept>
#include <utility>

namespace scau::coupling::core {

CouplingSnapshot::CouplingSnapshot(std::vector<ExchangeCellState> cells) : cells_(std::move(cells)) {}

const std::vector<ExchangeCellState>& CouplingSnapshot::cells() const noexcept {
    return cells_;
}

CouplingState::CouplingState(std::vector<ExchangeCellState> cells) : cells_(std::move(cells)) {}

const std::vector<ExchangeCellState>& CouplingState::cells() const noexcept {
    return cells_;
}

CouplingSnapshot CouplingState::snapshot() const {
    return CouplingSnapshot{cells_};
}

void CouplingState::enqueue_event(CouplingEvent event) {
    if (event.exchange_cell_index >= cells_.size()) {
        throw std::out_of_range("coupling event exchange cell index is out of range");
    }
    pending_events_.push_back(event);
}

void CouplingState::rollback(const CouplingSnapshot& snapshot) {
    cells_ = snapshot.cells_;
}

void CouplingState::replay_pending() {
    for (const auto& event : pending_events_) {
        cells_[event.exchange_cell_index].volume += event.volume_delta;
    }
    pending_events_.clear();
}

}  // namespace scau::coupling::core
```

- [ ] **Step 5: Register coupling unit tests**

Create `tests/unit/coupling/CMakeLists.txt`:

```cmake
add_executable(test_coupling_core_state test_coupling_core_state.cpp)
target_link_libraries(test_coupling_core_state
    PRIVATE
        scau::coupling_core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_core_state COMMAND test_coupling_core_state)
```

- [ ] **Step 6: Register the coupling test directory**

In `tests/CMakeLists.txt`, replace:

```cmake
add_subdirectory(unit/mesh)
add_subdirectory(unit/surface2d)
```

with:

```cmake
add_subdirectory(unit/mesh)
add_subdirectory(unit/surface2d)
add_subdirectory(unit/coupling)
```

- [ ] **Step 7: Create focused M15 tests**

Create `tests/unit/coupling/test_coupling_core_state.cpp`:

```cpp
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "coupling/core/state.hpp"

TEST(CouplingCoreState, SnapshotCapturesCurrentCellsAndRemainsImmutable) {
    scau::coupling::core::CouplingState state{{
        {.volume = 10.0},
        {.volume = 20.0},
    }};

    const auto snapshot = state.snapshot();
    state.enqueue_event({.exchange_cell_index = 0U, .volume_delta = 5.0});
    state.replay_pending();

    ASSERT_EQ(snapshot.cells().size(), 2U);
    EXPECT_DOUBLE_EQ(snapshot.cells()[0].volume, 10.0);
    EXPECT_DOUBLE_EQ(snapshot.cells()[1].volume, 20.0);
    EXPECT_DOUBLE_EQ(state.cells()[0].volume, 15.0);
}

TEST(CouplingCoreState, RollbackRestoresSnapshotVolumes) {
    scau::coupling::core::CouplingState state{{
        {.volume = 10.0},
        {.volume = 20.0},
    }};

    const auto snapshot = state.snapshot();
    state.enqueue_event({.exchange_cell_index = 0U, .volume_delta = -3.0});
    state.enqueue_event({.exchange_cell_index = 1U, .volume_delta = 7.0});
    state.replay_pending();
    state.rollback(snapshot);

    ASSERT_EQ(state.cells().size(), 2U);
    EXPECT_DOUBLE_EQ(state.cells()[0].volume, 10.0);
    EXPECT_DOUBLE_EQ(state.cells()[1].volume, 20.0);
}

TEST(CouplingCoreState, RollbackPreservesPendingEventsForReplay) {
    scau::coupling::core::CouplingState state{{
        {.volume = 10.0},
        {.volume = 20.0},
    }};

    const auto snapshot = state.snapshot();
    state.enqueue_event({.exchange_cell_index = 0U, .volume_delta = 2.0});
    state.enqueue_event({.exchange_cell_index = 0U, .volume_delta = 3.0});
    state.enqueue_event({.exchange_cell_index = 1U, .volume_delta = -4.0});
    state.rollback(snapshot);
    state.replay_pending();

    ASSERT_EQ(state.cells().size(), 2U);
    EXPECT_DOUBLE_EQ(state.cells()[0].volume, 15.0);
    EXPECT_DOUBLE_EQ(state.cells()[1].volume, 16.0);
}

TEST(CouplingCoreState, EnqueueRejectsOutOfRangeExchangeCellIndex) {
    scau::coupling::core::CouplingState state{{
        {.volume = 10.0},
    }};

    EXPECT_THROW(
        state.enqueue_event({.exchange_cell_index = 1U, .volume_delta = 1.0}),
        std::out_of_range);
}
```

- [ ] **Step 8: Build and run the focused test**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_core_state --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_core_state.exe
```

Expected: 4/4 tests pass.

---

## Task 2: Validate M15 And Record Evidence

**Files:**
- Create: `superpowers/specs/2026-05-20-plan-m15-coupling-core-snapshot-rollback-replay-evidence.md`

- [ ] **Step 1: Run full CTest**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure
```

Expected: full test suite passes, including `test_coupling_core_state`.

- [ ] **Step 2: Create evidence document**

Create `superpowers/specs/2026-05-20-plan-m15-coupling-core-snapshot-rollback-replay-evidence.md` with observed pass counts:

```markdown
# M15 CouplingLib Core Snapshot/Rollback/Replay Evidence

**Date:** 2026-05-20
**Design:** `superpowers/specs/2026-05-20-m15-coupling-core-snapshot-rollback-replay-design.md`
**Plan:** `superpowers/plans/2026-05-20-plan-m15-coupling-core-snapshot-rollback-replay.md`

## Scope

M15 adds a minimal pure `CouplingLib` core state container with snapshot, rollback, and replay semantics for exchange-cell volume events.

## Local Validation

- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_core_state --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_core_state.exe`: PASS, replace with observed pass count.
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure`: PASS, replace with observed pass count.

## Coverage

- `test_coupling_core_state`: snapshots capture exchange-cell volumes and remain immutable after later replayed events.
- `test_coupling_core_state`: rollback restores exchange-cell volumes from a saved snapshot.
- `test_coupling_core_state`: rollback preserves pending events and replay reapplies them in enqueue order.
- `test_coupling_core_state`: invalid exchange-cell event targets are rejected at enqueue time.

## Boundaries

- M15 does not connect to `Surface2DCore`, SWMM, D-Flow FM, or any 1D engine ABI.
- M15 does not implement `Q_limit`, `V_limit`, `mass_deficit_account`, shared-cell arbitration, adaptive timestep selection, fault-state orchestration, or GoldenSuite release evidence.
```

---

## Boundaries

- Do not add SWMM, D-Flow FM, or 1D engine dependencies.
- Do not add Surface2D dependencies.
- Do not implement `Q_limit`, `V_limit`, deficit ledger, or shared-cell arbitration.
- Do not claim release/merge readiness from this milestone alone.

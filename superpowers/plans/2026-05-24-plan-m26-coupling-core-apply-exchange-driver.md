# M26 CouplingLib Core Apply Exchange Single Donor Driver Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add `CouplingState::apply_exchange(cell_index, request)` that composes pipeline evaluation + event enqueue + counter record into a single entry point.

**Tech Stack:** C++20, CMake, GoogleTest, existing `scau::coupling_core` and `scau::warnings` targets.

---

## Task 1: Add Red Tests And Register Target

**Files:**
- Create: `tests/unit/coupling/test_coupling_apply_exchange.cpp`
- Modify: `tests/unit/coupling/CMakeLists.txt`

- [ ] **Step 1: Create test file**

Create `tests/unit/coupling/test_coupling_apply_exchange.cpp`:

```cpp
#include <stdexcept>

#include <gtest/gtest.h>

#include "coupling/core/state.hpp"

TEST(CouplingApplyExchange, ReturnsDecisionConsistentWithPipeline) {
    scau::coupling::core::CouplingState state{{
        {.volume = 100.0, .mass_deficit_account = {.volume = 0.0},
         .phi_t = 0.4, .h = 2.0, .area = 50.0},
    }};
    const scau::coupling::core::ExchangeRequest request{.q_request = 4.0, .dt_sub = 4.0};

    const auto expected = scau::coupling::core::evaluate_exchange_pipeline(
        state.cells()[0], request);
    const auto actual = state.apply_exchange(0U, request);

    EXPECT_DOUBLE_EQ(actual.exchange.q_granted, expected.exchange.q_granted);
    EXPECT_DOUBLE_EQ(actual.exchange.v_granted, expected.exchange.v_granted);
    EXPECT_DOUBLE_EQ(actual.exchange.v_unmet, expected.exchange.v_unmet);
    EXPECT_DOUBLE_EQ(actual.exchange.q_repay, expected.exchange.q_repay);
    EXPECT_DOUBLE_EQ(actual.exchange.v_repay, expected.exchange.v_repay);
    EXPECT_EQ(actual.drain_split.micro_steps, expected.drain_split.micro_steps);
    EXPECT_DOUBLE_EQ(actual.drain_split.dt_micro, expected.drain_split.dt_micro);
    EXPECT_DOUBLE_EQ(actual.drain_split.v_per_micro_step, expected.drain_split.v_per_micro_step);
    EXPECT_EQ(actual.drain_split_engaged, expected.drain_split_engaged);
    EXPECT_EQ(actual.negative_depth_fix_engaged, expected.negative_depth_fix_engaged);
}

TEST(CouplingApplyExchange, EnqueuesEventButDoesNotMutateVolume) {
    scau::coupling::core::CouplingState state{{
        {.volume = 100.0, .mass_deficit_account = {.volume = 0.0},
         .phi_t = 0.4, .h = 2.0, .area = 50.0},
    }};
    const scau::coupling::core::ExchangeRequest request{.q_request = 4.0, .dt_sub = 4.0};

    const auto decision = state.apply_exchange(0U, request);
    EXPECT_DOUBLE_EQ(state.cells()[0].volume, 100.0);

    state.replay_pending();
    EXPECT_DOUBLE_EQ(state.cells()[0].volume, 100.0 + decision.exchange.v_granted);
}

TEST(CouplingApplyExchange, UpdatesRuntimeCounters) {
    scau::coupling::core::CouplingState state{{
        {.volume = 100.0, .mass_deficit_account = {.volume = 0.0},
         .phi_t = 0.4, .h = 2.0, .area = 50.0},
    }};
    const scau::coupling::core::ExchangeRequest request{.q_request = 4.0, .dt_sub = 4.0};

    const auto decision = state.apply_exchange(0U, request);

    if (decision.drain_split_engaged) {
        EXPECT_EQ(state.runtime_counters().count_drain_split, 1U);
    } else {
        EXPECT_EQ(state.runtime_counters().count_drain_split, 0U);
    }
    EXPECT_EQ(state.runtime_counters().count_negative_depth_fix, 0U);
}

TEST(CouplingApplyExchange, ReplayUpdatesMassDeficitAccount) {
    scau::coupling::core::CouplingState state{{
        {.volume = 100.0, .mass_deficit_account = {.volume = 5.0},
         .phi_t = 0.4, .h = 2.0, .area = 50.0},
    }};
    const scau::coupling::core::ExchangeRequest request{.q_request = 4.0, .dt_sub = 4.0};

    const auto decision = state.apply_exchange(0U, request);
    state.replay_pending();

    const double expected_deficit =
        5.0 + decision.exchange.v_unmet - decision.exchange.v_repay;
    const double clamped = expected_deficit < 0.0 ? 0.0 : expected_deficit;
    EXPECT_DOUBLE_EQ(state.cells()[0].mass_deficit_account.volume, clamped);
}

TEST(CouplingApplyExchange, RejectsOutOfRangeCellIndex) {
    scau::coupling::core::CouplingState state{{
        {.volume = 100.0, .phi_t = 0.4, .h = 2.0, .area = 50.0},
    }};
    const scau::coupling::core::ExchangeRequest request{.q_request = 1.0, .dt_sub = 4.0};

    EXPECT_THROW(
        static_cast<void>(state.apply_exchange(1U, request)),
        std::out_of_range);
}
```

- [ ] **Step 2: Register the test target**

Append to `tests/unit/coupling/CMakeLists.txt`:

```cmake

add_executable(test_coupling_apply_exchange test_coupling_apply_exchange.cpp)
target_link_libraries(test_coupling_apply_exchange
    PRIVATE
        scau::coupling_core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_apply_exchange COMMAND test_coupling_apply_exchange)
```

- [ ] **Step 3: Run focused target and verify compile failure**

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_apply_exchange --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_apply_exchange.exe
```

Expected: Compile failure because `CouplingState::apply_exchange(...)` is not declared.

---

## Task 2: Implement apply_exchange

**Files:**
- Modify: `libs/coupling/core/include/coupling/core/state.hpp`
- Modify: `libs/coupling/core/src/state.cpp`

- [ ] **Step 1: Declare the method**

In `libs/coupling/core/include/coupling/core/state.hpp`, in the `CouplingState` public section, add after `record_pipeline_decision`:

```cpp
    ExchangePipelineDecision apply_exchange(std::size_t cell_index, const ExchangeRequest& request);
```

- [ ] **Step 2: Implement the method**

In `libs/coupling/core/src/state.cpp`, add before the closing namespace brace:

```cpp
ExchangePipelineDecision CouplingState::apply_exchange(
    std::size_t cell_index,
    const ExchangeRequest& request) {
    if (cell_index >= cells_.size()) {
        throw std::out_of_range("apply_exchange cell index is out of range");
    }

    const auto decision = evaluate_exchange_pipeline(cells_[cell_index], request);

    enqueue_event(CouplingEvent{
        .exchange_cell_index = cell_index,
        .volume_delta = decision.exchange.v_granted,
        .unmet_volume = decision.exchange.v_unmet,
        .repayment_volume = decision.exchange.v_repay,
    });

    record_pipeline_decision(decision);

    return decision;
}
```

- [ ] **Step 3: Run focused apply_exchange test**

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_apply_exchange --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_apply_exchange.exe
```

Expected: PASS, 5/5 tests.

- [ ] **Step 4: Run existing focused tests to confirm no regression**

Run all 7 existing coupling targets (same as M24 plan Step 4).

---

## Task 3: Validate And Record Evidence

- [ ] **Step 1: Run full CTest**

```bash
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure
```

Expected: PASS, 30/30 (M25 was 29; M26 adds one new test target).

- [ ] **Step 2: Create evidence document**

- [ ] **Step 3: Inspect status and diff**

---

## Boundaries

- `apply_exchange(...)` does not call `replay_pending()`.
- No multi-donor, no FaultController, no scheduler, no mapping.
- No SWMM / D-Flow FM / Surface2DCore connection.
- No snapshot persistence or IO.

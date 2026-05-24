# M22 CouplingLib Core Single Donor Pipeline Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a pure `evaluate_exchange_pipeline(...)` function that composes M19 `evaluate_exchange(...)`, M21 `enforce_nonnegative_storage(...)`, and M20 `split_drain(...)` for one donor cell and one exchange request.

**Architecture:** Add a small `ExchangePipelineDecision` return type and one wrapper function in `libs/coupling/core`. The wrapper must not add new physics or mutate state; it only preserves the intended execution order and returns both the final exchange decision and the final drain-split micro-step plan.

**Tech Stack:** C++20, CMake, GoogleTest, existing `scau::coupling_core` and `scau::warnings` targets.

---

## File Structure

- Modify `libs/coupling/core/include/coupling/core/state.hpp`
  - Add `ExchangePipelineDecision` struct.
  - Declare `[[nodiscard]] ExchangePipelineDecision evaluate_exchange_pipeline(const ExchangeCellState&, const ExchangeRequest&);`.
- Modify `libs/coupling/core/src/state.cpp`
  - Implement `evaluate_exchange_pipeline(...)` by composing existing helpers.
- Create `tests/unit/coupling/test_coupling_exchange_pipeline.cpp`
  - Add focused M22 regression tests.
- Modify `tests/unit/coupling/CMakeLists.txt`
  - Register `test_coupling_exchange_pipeline`.
- Create `superpowers/specs/2026-05-24-plan-m22-coupling-core-single-donor-pipeline-evidence.md`
  - Record focused and full validation evidence after implementation.

---

## Task 1: Add Red Tests And Register Focused Target

**Files:**
- Create: `tests/unit/coupling/test_coupling_exchange_pipeline.cpp`
- Modify: `tests/unit/coupling/CMakeLists.txt`

- [ ] **Step 1: Create focused exchange pipeline tests**

Create `tests/unit/coupling/test_coupling_exchange_pipeline.cpp` with this content:

```cpp
#include <stdexcept>

#include <gtest/gtest.h>

#include "coupling/core/state.hpp"

namespace {

scau::coupling::core::ExchangeCellState make_cell(
    double deficit_volume = 0.0,
    double phi_t = 0.4,
    double h = 2.0,
    double area = 50.0) {
    return scau::coupling::core::ExchangeCellState{
        .volume = 0.0,
        .mass_deficit_account = {.volume = deficit_volume},
        .phi_t = phi_t,
        .h = h,
        .area = area,
    };
}

}  // namespace

TEST(CouplingExchangePipeline, SafeRequestReturnsFullGrantAndSingleMicroStep) {
    const auto cell = make_cell();
    const scau::coupling::core::ExchangeRequest request{.q_request = 4.0, .dt_sub = 4.0};

    const auto decision = scau::coupling::core::evaluate_exchange_pipeline(cell, request);

    EXPECT_DOUBLE_EQ(decision.exchange.q_granted, 4.0);
    EXPECT_DOUBLE_EQ(decision.exchange.v_granted, 16.0);
    EXPECT_DOUBLE_EQ(decision.exchange.q_repay, 0.0);
    EXPECT_DOUBLE_EQ(decision.exchange.v_repay, 0.0);
    EXPECT_DOUBLE_EQ(decision.exchange.v_unmet, 0.0);
    EXPECT_EQ(decision.drain_split.micro_steps, 2);
    EXPECT_DOUBLE_EQ(decision.drain_split.dt_micro, 2.0);
    EXPECT_DOUBLE_EQ(decision.drain_split.v_per_micro_step, 8.0);
}

TEST(CouplingExchangePipeline, RequestAboveHardGateClampsAndAccruesUnmet) {
    const auto cell = make_cell();
    const scau::coupling::core::ExchangeRequest request{.q_request = 20.0, .dt_sub = 4.0};

    const auto decision = scau::coupling::core::evaluate_exchange_pipeline(cell, request);

    EXPECT_DOUBLE_EQ(decision.exchange.q_granted, 9.0);
    EXPECT_DOUBLE_EQ(decision.exchange.v_granted, 36.0);
    EXPECT_DOUBLE_EQ(decision.exchange.v_unmet, 44.0);
    EXPECT_EQ(decision.drain_split.micro_steps, 5);
    EXPECT_DOUBLE_EQ(decision.drain_split.dt_micro, 0.8);
    EXPECT_DOUBLE_EQ(decision.drain_split.v_per_micro_step, 7.2);
}

TEST(CouplingExchangePipeline, NonnegativeStorageBackoffRunsAfterHardGateClamp) {
    const auto cell = make_cell(0.0, 0.2, 1.0, 50.0);
    const scau::coupling::core::ExchangeRequest request{.q_request = 5.0, .dt_sub = 4.0};

    const auto decision = scau::coupling::core::evaluate_exchange_pipeline(cell, request);

    EXPECT_DOUBLE_EQ(decision.exchange.q_granted, 2.5);
    EXPECT_DOUBLE_EQ(decision.exchange.v_granted, 10.0);
    EXPECT_DOUBLE_EQ(decision.exchange.v_unmet, 10.0);
    EXPECT_EQ(decision.drain_split.micro_steps, 5);
    EXPECT_DOUBLE_EQ(decision.drain_split.dt_micro, 0.8);
    EXPECT_DOUBLE_EQ(decision.drain_split.v_per_micro_step, 2.0);
}

TEST(CouplingExchangePipeline, DeficitRepaymentPrecedesNewRequestGrant) {
    const auto cell = make_cell(8.0);
    const scau::coupling::core::ExchangeRequest request{.q_request = 5.0, .dt_sub = 4.0};

    const auto decision = scau::coupling::core::evaluate_exchange_pipeline(cell, request);

    EXPECT_DOUBLE_EQ(decision.exchange.q_repay, 2.0);
    EXPECT_DOUBLE_EQ(decision.exchange.v_repay, 8.0);
    EXPECT_DOUBLE_EQ(decision.exchange.q_granted, 5.0);
    EXPECT_DOUBLE_EQ(decision.exchange.v_granted, 20.0);
    EXPECT_DOUBLE_EQ(decision.exchange.v_unmet, 0.0);
    EXPECT_EQ(decision.drain_split.micro_steps, 3);
    EXPECT_DOUBLE_EQ(decision.drain_split.dt_micro, 4.0 / 3.0);
    EXPECT_DOUBLE_EQ(decision.drain_split.v_per_micro_step, 20.0 / 3.0);
}

TEST(CouplingExchangePipeline, InvalidInputsAreRejectedByComposedHelpers) {
    const auto cell = make_cell();
    const auto negative_deficit_cell = make_cell(-0.1);
    const auto negative_phi_t_cell = make_cell(0.0, -0.1, 2.0, 50.0);

    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::evaluate_exchange_pipeline(
            cell, scau::coupling::core::ExchangeRequest{.q_request = -0.1, .dt_sub = 4.0})),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::evaluate_exchange_pipeline(
            cell, scau::coupling::core::ExchangeRequest{.q_request = 1.0, .dt_sub = 0.0})),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::evaluate_exchange_pipeline(
            negative_deficit_cell, scau::coupling::core::ExchangeRequest{.q_request = 1.0, .dt_sub = 4.0})),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::evaluate_exchange_pipeline(
            negative_phi_t_cell, scau::coupling::core::ExchangeRequest{.q_request = 1.0, .dt_sub = 4.0})),
        std::invalid_argument);
}
```

- [ ] **Step 2: Register the focused test target**

Append the following block to `tests/unit/coupling/CMakeLists.txt`:

```cmake

add_executable(test_coupling_exchange_pipeline test_coupling_exchange_pipeline.cpp)
target_link_libraries(test_coupling_exchange_pipeline
    PRIVATE
        scau::coupling_core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_exchange_pipeline COMMAND test_coupling_exchange_pipeline)
```

- [ ] **Step 3: Run the focused test and verify it fails before implementation**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_exchange_pipeline --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_exchange_pipeline.exe
```

Expected: FAIL during compilation because `ExchangePipelineDecision` and `evaluate_exchange_pipeline(...)` are not declared yet.

---

## Task 2: Implement Single Donor Pipeline

**Files:**
- Modify: `libs/coupling/core/include/coupling/core/state.hpp`
- Modify: `libs/coupling/core/src/state.cpp`

- [ ] **Step 1: Extend the public API**

In `libs/coupling/core/include/coupling/core/state.hpp`, add the following block immediately after the `enforce_nonnegative_storage(...)` declaration and before `[[nodiscard]] MassDeficitAccount roll_deficit(...)`:

```cpp
struct ExchangePipelineDecision {
    ExchangeDecision exchange{};
    DrainSplit drain_split{};
};

[[nodiscard]] ExchangePipelineDecision evaluate_exchange_pipeline(
    const ExchangeCellState& cell,
    const ExchangeRequest& request);
```

- [ ] **Step 2: Implement the helper composition**

In `libs/coupling/core/src/state.cpp`, add the following function definition after `enforce_nonnegative_storage(...)` and before `MassDeficitAccount roll_deficit(...)`:

```cpp
ExchangePipelineDecision evaluate_exchange_pipeline(
    const ExchangeCellState& cell,
    const ExchangeRequest& request) {
    const auto initial = evaluate_exchange(cell, request);
    const auto nonnegative = enforce_nonnegative_storage(cell, initial, request.dt_sub);
    const auto split = split_drain(cell, nonnegative, request.dt_sub);

    return ExchangePipelineDecision{
        .exchange = nonnegative,
        .drain_split = split,
    };
}
```

- [ ] **Step 3: Run focused exchange pipeline test**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_exchange_pipeline --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_exchange_pipeline.exe
```

Expected: PASS, `5 tests` pass in `CouplingExchangePipeline`.

- [ ] **Step 4: Run existing focused coupling tests to confirm no regression**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_nonnegative_storage_backoff --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_nonnegative_storage_backoff.exe
```

Expected: PASS, `5 tests` pass in `CouplingNonnegativeStorageBackoff`.

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_drain_split --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_drain_split.exe
```

Expected: PASS, `6 tests` pass in `CouplingDrainSplit`.

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_exchange_decision --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_exchange_decision.exe
```

Expected: PASS, `5 tests` pass in `CouplingExchangeDecision`.

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_core_state --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_core_state.exe
```

Expected: PASS, `7 tests` pass in `CouplingCoreState`.

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

## Task 3: Validate M22 And Record Evidence

**Files:**
- Create: `superpowers/specs/2026-05-24-plan-m22-coupling-core-single-donor-pipeline-evidence.md`

- [ ] **Step 1: Run full CTest**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure
```

Expected: PASS, full CTest passes with all tests successful (M21 baseline was 28/28; M22 adds one focused target, so 29/29).

- [ ] **Step 2: Create M22 evidence document**

Create `superpowers/specs/2026-05-24-plan-m22-coupling-core-single-donor-pipeline-evidence.md` with this structure, filling the observed pass counts from the command output:

```markdown
# M22 CouplingLib Core Single Donor Pipeline Evidence

**Date:** 2026-05-24
**Design:** `superpowers/specs/2026-05-24-m22-coupling-core-single-donor-pipeline-design.md`
**Plan:** `superpowers/plans/2026-05-24-plan-m22-coupling-core-single-donor-pipeline.md`

## Scope

M22 adds the pure `CouplingLib` core `evaluate_exchange_pipeline(...)` decision that composes `evaluate_exchange(...)`, `enforce_nonnegative_storage(...)`, and `split_drain(...)` for one donor cell and one exchange request.

## Local Validation

- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_exchange_pipeline --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_exchange_pipeline.exe`: PASS, 5/5 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_nonnegative_storage_backoff --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_nonnegative_storage_backoff.exe`: PASS, 5/5 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_drain_split --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_drain_split.exe`: PASS, 6/6 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_exchange_decision --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_exchange_decision.exe`: PASS, 5/5 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_core_state --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_core_state.exe`: PASS, 7/7 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_mass_deficit_account --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_mass_deficit_account.exe`: PASS, 5/5 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_flow_limit --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_flow_limit.exe`: PASS, 4/4 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure`: PASS, full CTest passed `<observed>/<observed>`.

## Coverage

- `test_coupling_exchange_pipeline`: safe request returns final grant and drain split.
- `test_coupling_exchange_pipeline`: request above `Q_limit` clamps and accrues unmet volume.
- `test_coupling_exchange_pipeline`: nonnegative storage backoff runs after hard-gate clamp.
- `test_coupling_exchange_pipeline`: deficit repayment precedes new request grant.
- `test_coupling_exchange_pipeline`: invalid inputs are rejected through composed helpers.
- Existing M16-M21 focused tests remain intact.

## Boundaries

- M22 does not introduce new exchange semantics beyond composing M19-M21 helpers.
- M22 does not implement multi-donor linear programming.
- M22 does not introduce arbitration, `priority_weight`, shared-cell splitting, or `V_limit_k`.
- M22 does not introduce runtime counters.
- M22 does not introduce `epsilon_deficit`, scheduler, fault, or write-off logic.
- M22 does not connect `evaluate_exchange_pipeline(...)` to `Surface2DCore`, SWMM, D-Flow FM, or any 1D engine ABI.
- M22 does not mutate `CouplingState`.
```

- [ ] **Step 3: Inspect status and diff**

Run:

```bash
git status --short
git diff -- libs/coupling/core/include/coupling/core/state.hpp libs/coupling/core/src/state.cpp tests/unit/coupling/CMakeLists.txt tests/unit/coupling/test_coupling_exchange_pipeline.cpp superpowers/specs/2026-05-24-m22-coupling-core-single-donor-pipeline-design.md superpowers/plans/2026-05-24-plan-m22-coupling-core-single-donor-pipeline.md superpowers/specs/2026-05-24-plan-m22-coupling-core-single-donor-pipeline-evidence.md
```

Expected: Only M22 files are modified or created, plus any pre-existing untracked transcript remains excluded from future commits.

---

## Boundaries

- Preserve `Q_limit` / `V_limit`, `mass_deficit_account`, `drain_split`, and nonnegative storage semantics by composition only.
- Do not introduce new exchange semantics in this slice.
- Do not implement multi-donor linear programming.
- Do not introduce arbitration, `priority_weight`, shared-cell splitting, `V_limit_k`, runtime counters, scheduler, fault, or write-off logic.
- Do not connect `evaluate_exchange_pipeline(...)` to SWMM, D-Flow FM, `Surface2DCore`, or any 1D engine adapter; M22 stays pure-core.
- `evaluate_exchange_pipeline(...)` must remain a pure function and must not mutate `CouplingState`.

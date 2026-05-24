# M23 CouplingLib Core Pipeline Diagnostic Flags Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend `ExchangePipelineDecision` with two diagnostic boolean fields `drain_split_engaged` and `negative_depth_fix_engaged` so M20 / M21 engagement is observable per call. Pure-additive change; no new physics, no DTO refactor, no state mutation.

**Architecture:** Add two `bool` fields to `ExchangePipelineDecision` in `libs/coupling/core/include/coupling/core/state.hpp`. Set them inside `evaluate_exchange_pipeline(...)` in `libs/coupling/core/src/state.cpp` by comparing the pre/post-M21 `v_granted` and reading `drain_split.micro_steps`.

**Tech Stack:** C++20, CMake, GoogleTest, existing `scau::coupling_core` and `scau::warnings` targets.

---

## File Structure

- Modify `libs/coupling/core/include/coupling/core/state.hpp`
  - Add `drain_split_engaged` and `negative_depth_fix_engaged` to `ExchangePipelineDecision`.
- Modify `libs/coupling/core/src/state.cpp`
  - Set the new flags inside `evaluate_exchange_pipeline(...)`.
- Modify `tests/unit/coupling/test_coupling_exchange_pipeline.cpp`
  - Add flag assertions to existing tests and one dedicated test.
- Create `superpowers/specs/2026-05-24-plan-m23-coupling-core-pipeline-diagnostic-flags-evidence.md`
  - Record focused and full validation evidence after implementation.

---

## Task 1: Add Red Tests For Diagnostic Flags

**Files:**
- Modify: `tests/unit/coupling/test_coupling_exchange_pipeline.cpp`

- [ ] **Step 1: Extend existing tests and add a dedicated flag test**

Add the following assertions to each existing test (after the existing assertions):

- `SafeRequestReturnsFullGrantAndSingleMicroStep` (note: this case actually splits into 2 micro-steps because `v_granted = 16.0 > 0.2·phi_t·h·A = 8.0`; the test name is historical but the engagement flag must reflect the real micro_steps value):
  ```cpp
  EXPECT_TRUE(decision.drain_split_engaged);
  EXPECT_FALSE(decision.negative_depth_fix_engaged);
  ```
- `RequestAboveHardGateClampsAndAccruesUnmet`:
  ```cpp
  EXPECT_TRUE(decision.drain_split_engaged);
  EXPECT_FALSE(decision.negative_depth_fix_engaged);
  ```
- `NonnegativeStorageBackoffRunsAfterHardGateClamp`:
  ```cpp
  EXPECT_TRUE(decision.drain_split_engaged);
  EXPECT_FALSE(decision.negative_depth_fix_engaged);
  ```
- `DeficitRepaymentPrecedesNewRequestGrant`:
  ```cpp
  EXPECT_TRUE(decision.drain_split_engaged);
  EXPECT_FALSE(decision.negative_depth_fix_engaged);
  ```

Add this new test at the end of the file (before the `InvalidInputs...` test, to keep invalid-input cases last):

```cpp
TEST(CouplingExchangePipeline, EngagementFlagsRemainOffForSafeRequestExactlyAtSplitThreshold) {
    const auto cell = make_cell();
    const scau::coupling::core::ExchangeRequest request{.q_request = 2.0, .dt_sub = 4.0};

    const auto decision = scau::coupling::core::evaluate_exchange_pipeline(cell, request);

    EXPECT_DOUBLE_EQ(decision.exchange.q_granted, 2.0);
    EXPECT_DOUBLE_EQ(decision.exchange.v_granted, 8.0);
    EXPECT_DOUBLE_EQ(decision.exchange.v_unmet, 0.0);
    EXPECT_EQ(decision.drain_split.micro_steps, 1);
    EXPECT_FALSE(decision.drain_split_engaged);
    EXPECT_FALSE(decision.negative_depth_fix_engaged);
}
```

Rationale: default `make_cell()` has `phi_t=0.4, h=2.0, area=50.0`, so `0.2·phi_t·h·A = 8.0`. With `v_granted = 8.0` exactly at the split threshold, M20 returns a single micro-step. This locks the boundary case.

- [ ] **Step 2: Run the focused target and verify it fails to compile**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_exchange_pipeline --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_exchange_pipeline.exe
```

Expected: FAIL during compilation because `drain_split_engaged` and `negative_depth_fix_engaged` are not declared in `ExchangePipelineDecision` yet.

---

## Task 2: Implement Diagnostic Flags

**Files:**
- Modify: `libs/coupling/core/include/coupling/core/state.hpp`
- Modify: `libs/coupling/core/src/state.cpp`

- [ ] **Step 1: Extend the public type**

In `libs/coupling/core/include/coupling/core/state.hpp`, replace the existing `ExchangePipelineDecision` definition with:

```cpp
struct ExchangePipelineDecision {
    ExchangeDecision exchange{};
    DrainSplit drain_split{};
    bool drain_split_engaged{false};
    bool negative_depth_fix_engaged{false};
};
```

- [ ] **Step 2: Populate the flags in the pipeline**

In `libs/coupling/core/src/state.cpp`, replace the body of `evaluate_exchange_pipeline(...)` with:

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
        .drain_split_engaged = split.micro_steps > 1,
        .negative_depth_fix_engaged = nonnegative.v_granted < initial.v_granted,
    };
}
```

- [ ] **Step 3: Run focused exchange pipeline test**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_exchange_pipeline --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_exchange_pipeline.exe
```

Expected: PASS, 6/6 tests pass in `CouplingExchangePipeline`.

- [ ] **Step 4: Run existing focused coupling tests to confirm no regression**

Run each:

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
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_core_state --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_core_state.exe
```

Expected: PASS, 7/7.

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_mass_deficit_account --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_mass_deficit_account.exe
```

Expected: PASS, 5/5.

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_flow_limit --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_flow_limit.exe
```

Expected: PASS, 4/4.

---

## Task 3: Validate M23 And Record Evidence

**Files:**
- Create: `superpowers/specs/2026-05-24-plan-m23-coupling-core-pipeline-diagnostic-flags-evidence.md`

- [ ] **Step 1: Run full CTest**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure
```

Expected: PASS, full CTest passes 29/29 (M22 baseline; M23 only adds tests inside the existing `test_coupling_exchange_pipeline` target, so the target count is unchanged).

- [ ] **Step 2: Create M23 evidence document**

Create `superpowers/specs/2026-05-24-plan-m23-coupling-core-pipeline-diagnostic-flags-evidence.md` with:

```markdown
# M23 CouplingLib Core Pipeline Diagnostic Flags Evidence

**Date:** 2026-05-24
**Design:** `superpowers/specs/2026-05-24-m23-coupling-core-pipeline-diagnostic-flags-design.md`
**Plan:** `superpowers/plans/2026-05-24-plan-m23-coupling-core-pipeline-diagnostic-flags.md`

## Scope

M23 extends `ExchangePipelineDecision` with two diagnostic booleans:

- `drain_split_engaged` — whether M20 returned `micro_steps > 1`.
- `negative_depth_fix_engaged` — whether M21 actually lowered the post-M19 `v_granted`.

The flags are pure observations on the M22 composition; no state, no new physics.

## Local Validation

- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_exchange_pipeline --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_exchange_pipeline.exe`: PASS, 6/6 tests passed.
- Full focused regression: M21 5/5, M20 6/6, M19 5/5, M18/M15 7/7, M17 5/5, M16 4/4.
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure`: PASS, full CTest passed `<observed>/<observed>`.

## Coverage

- `test_coupling_exchange_pipeline`: safe request keeps both flags off.
- `test_coupling_exchange_pipeline`: request above `Q_limit` engages `drain_split_engaged`; `negative_depth_fix_engaged` stays off (M21 unreachable under canonical V_limit factor).
- `test_coupling_exchange_pipeline`: nonnegative-storage scenario from M22 keeps `negative_depth_fix_engaged` off — turning the M22 evidence observation into a machine-asserted invariant under canonical `V_limit = 0.9·phi_t·h·A`.
- `test_coupling_exchange_pipeline`: deficit repayment scenario engages `drain_split_engaged`.
- `test_coupling_exchange_pipeline`: safe request exactly at the M20 split threshold keeps both flags off.

## Boundaries

- M23 does not introduce stateful counters or modify `CouplingState`.
- M23 does not change snapshot / rollback / replay coverage; those updates are reserved for M24.
- M23 does not introduce multi-donor arbitration, `priority_weight`, `V_limit_k`, or any new DTO.
- M23 does not change `evaluate_exchange(...)`, `enforce_nonnegative_storage(...)`, or `split_drain(...)`.
- M23 does not connect the pipeline to `Surface2DCore`, SWMM, D-Flow FM, or any 1D engine ABI.
```

- [ ] **Step 3: Inspect status and diff**

Run:

```bash
git status --short
git diff --stat -- libs/coupling/core/include/coupling/core/state.hpp libs/coupling/core/src/state.cpp tests/unit/coupling/test_coupling_exchange_pipeline.cpp
```

Expected: Only M23 files are modified or created; existing transcript stays untracked and out of commit scope.

---

## Boundaries

- Pure-additive flag fields only; no semantic change to M19-M22 composition.
- No counters, no `CouplingState` mutation, no snapshot/rollback/replay edits.
- No multi-donor logic, `priority_weight`, `V_limit_k`, or DTO refactor.
- No external engine ABI connection.
- `evaluate_exchange_pipeline(...)` remains a pure function.

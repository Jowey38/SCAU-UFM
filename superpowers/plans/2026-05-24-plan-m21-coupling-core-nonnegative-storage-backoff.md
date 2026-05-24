# M21 CouplingLib Core Nonnegative Storage Backoff Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a pure `enforce_nonnegative_storage(...)` function that clamps a single donor exchange decision so the granted volume cannot exceed donor-side physical storage `phi_t * h * A`.

**Architecture:** Reuse the existing `ExchangeDecision` type from M19 and add one pure helper in `libs/coupling/core`. The helper runs after exchange arbitration and before downstream execution, preserving repayment fields and moving any additionally clamped grant volume into `v_unmet`.

**Tech Stack:** C++20, CMake, GoogleTest, existing `scau::coupling_core` and `scau::warnings` targets.

---

## File Structure

- Modify `libs/coupling/core/include/coupling/core/state.hpp`
  - Declare `[[nodiscard]] ExchangeDecision enforce_nonnegative_storage(const ExchangeCellState&, const ExchangeDecision&, double dt_sub);`.
- Modify `libs/coupling/core/src/state.cpp`
  - Implement the single-donor nonnegative storage backoff rule.
- Create `tests/unit/coupling/test_coupling_nonnegative_storage_backoff.cpp`
  - Add focused M21 regression tests.
- Modify `tests/unit/coupling/CMakeLists.txt`
  - Register `test_coupling_nonnegative_storage_backoff`.
- Create `superpowers/specs/2026-05-24-plan-m21-coupling-core-nonnegative-storage-backoff-evidence.md`
  - Record focused and full validation evidence after implementation.

---

## Task 1: Add Red Tests And Register Focused Target

**Files:**
- Create: `tests/unit/coupling/test_coupling_nonnegative_storage_backoff.cpp`
- Modify: `tests/unit/coupling/CMakeLists.txt`

- [ ] **Step 1: Create focused nonnegative storage backoff tests**

Create `tests/unit/coupling/test_coupling_nonnegative_storage_backoff.cpp` with this content:

```cpp
#include <stdexcept>

#include <gtest/gtest.h>

#include "coupling/core/state.hpp"

namespace {

scau::coupling::core::ExchangeCellState make_cell(double phi_t = 0.4, double h = 2.0, double area = 50.0) {
    return scau::coupling::core::ExchangeCellState{
        .volume = 0.0,
        .mass_deficit_account = {},
        .phi_t = phi_t,
        .h = h,
        .area = area,
    };
}

scau::coupling::core::ExchangeDecision make_decision(double q_granted, double v_granted, double v_unmet = 0.0) {
    return scau::coupling::core::ExchangeDecision{
        .q_granted = q_granted,
        .v_granted = v_granted,
        .q_repay = 1.5,
        .v_repay = 6.0,
        .v_unmet = v_unmet,
    };
}

}  // namespace

TEST(CouplingNonnegativeStorageBackoff, LeavesDecisionBelowAvailableStorageUnchanged) {
    const auto cell = make_cell();
    const auto decision = make_decision(5.0, 20.0, 2.0);

    const auto enforced = scau::coupling::core::enforce_nonnegative_storage(cell, decision, 4.0);

    EXPECT_DOUBLE_EQ(enforced.q_granted, 5.0);
    EXPECT_DOUBLE_EQ(enforced.v_granted, 20.0);
    EXPECT_DOUBLE_EQ(enforced.q_repay, 1.5);
    EXPECT_DOUBLE_EQ(enforced.v_repay, 6.0);
    EXPECT_DOUBLE_EQ(enforced.v_unmet, 2.0);
}

TEST(CouplingNonnegativeStorageBackoff, LeavesDecisionAtAvailableStorageUnchanged) {
    const auto cell = make_cell();
    const auto decision = make_decision(10.0, 40.0, 3.0);

    const auto enforced = scau::coupling::core::enforce_nonnegative_storage(cell, decision, 4.0);

    EXPECT_DOUBLE_EQ(enforced.q_granted, 10.0);
    EXPECT_DOUBLE_EQ(enforced.v_granted, 40.0);
    EXPECT_DOUBLE_EQ(enforced.q_repay, 1.5);
    EXPECT_DOUBLE_EQ(enforced.v_repay, 6.0);
    EXPECT_DOUBLE_EQ(enforced.v_unmet, 3.0);
}

TEST(CouplingNonnegativeStorageBackoff, ClampsGrantAboveAvailableStorageAndAccruesAdditionalUnmet) {
    const auto cell = make_cell();
    const auto decision = make_decision(12.5, 50.0, 4.0);

    const auto enforced = scau::coupling::core::enforce_nonnegative_storage(cell, decision, 4.0);

    EXPECT_DOUBLE_EQ(enforced.q_granted, 10.0);
    EXPECT_DOUBLE_EQ(enforced.v_granted, 40.0);
    EXPECT_DOUBLE_EQ(enforced.q_repay, 1.5);
    EXPECT_DOUBLE_EQ(enforced.v_repay, 6.0);
    EXPECT_DOUBLE_EQ(enforced.v_unmet, 14.0);
}

TEST(CouplingNonnegativeStorageBackoff, ZeroAvailableStorageMovesAllGrantedVolumeToUnmet) {
    const auto cell = make_cell(0.0, 2.0, 50.0);
    const auto decision = make_decision(2.0, 8.0, 1.0);

    const auto enforced = scau::coupling::core::enforce_nonnegative_storage(cell, decision, 4.0);

    EXPECT_DOUBLE_EQ(enforced.q_granted, 0.0);
    EXPECT_DOUBLE_EQ(enforced.v_granted, 0.0);
    EXPECT_DOUBLE_EQ(enforced.q_repay, 1.5);
    EXPECT_DOUBLE_EQ(enforced.v_repay, 6.0);
    EXPECT_DOUBLE_EQ(enforced.v_unmet, 9.0);
}

TEST(CouplingNonnegativeStorageBackoff, RejectsInvalidInputs) {
    const auto cell = make_cell();
    const auto decision = make_decision(5.0, 20.0, 0.0);

    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::enforce_nonnegative_storage(cell, decision, 0.0)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::enforce_nonnegative_storage(cell, decision, -1.0)),
        std::invalid_argument);

    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::enforce_nonnegative_storage(make_cell(-0.1, 2.0, 50.0), decision, 4.0)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::enforce_nonnegative_storage(make_cell(0.4, -1.0, 50.0), decision, 4.0)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::enforce_nonnegative_storage(make_cell(0.4, 2.0, -1.0), decision, 4.0)),
        std::invalid_argument);

    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::enforce_nonnegative_storage(
            cell, make_decision(-0.1, 20.0, 0.0), 4.0)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::enforce_nonnegative_storage(
            cell, make_decision(5.0, -0.1, 0.0), 4.0)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::enforce_nonnegative_storage(
            cell,
            scau::coupling::core::ExchangeDecision{
                .q_granted = 5.0,
                .v_granted = 20.0,
                .q_repay = -0.1,
                .v_repay = 6.0,
                .v_unmet = 0.0,
            },
            4.0)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::enforce_nonnegative_storage(
            cell,
            scau::coupling::core::ExchangeDecision{
                .q_granted = 5.0,
                .v_granted = 20.0,
                .q_repay = 1.5,
                .v_repay = -0.1,
                .v_unmet = 0.0,
            },
            4.0)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::enforce_nonnegative_storage(
            cell, make_decision(5.0, 20.0, -0.1), 4.0)),
        std::invalid_argument);
}
```

- [ ] **Step 2: Register the focused test target**

Append the following block to `tests/unit/coupling/CMakeLists.txt`:

```cmake

add_executable(test_coupling_nonnegative_storage_backoff test_coupling_nonnegative_storage_backoff.cpp)
target_link_libraries(test_coupling_nonnegative_storage_backoff
    PRIVATE
        scau::coupling_core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_nonnegative_storage_backoff COMMAND test_coupling_nonnegative_storage_backoff)
```

- [ ] **Step 3: Run the focused test and verify it fails before implementation**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_nonnegative_storage_backoff --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_nonnegative_storage_backoff.exe
```

Expected: FAIL during compilation because `enforce_nonnegative_storage(...)` is not declared yet.

---

## Task 2: Implement Nonnegative Storage Backoff

**Files:**
- Modify: `libs/coupling/core/include/coupling/core/state.hpp`
- Modify: `libs/coupling/core/src/state.cpp`

- [ ] **Step 1: Extend the public API**

In `libs/coupling/core/include/coupling/core/state.hpp`, add the following declaration immediately after the `split_drain(...)` declaration and before `[[nodiscard]] MassDeficitAccount roll_deficit(...)`:

```cpp
[[nodiscard]] ExchangeDecision enforce_nonnegative_storage(
    const ExchangeCellState& cell,
    const ExchangeDecision& decision,
    double dt_sub);
```

- [ ] **Step 2: Implement single-donor storage backoff**

In `libs/coupling/core/src/state.cpp`, add the following function definition after `split_drain(...)` and before `MassDeficitAccount roll_deficit(...)`:

```cpp
ExchangeDecision enforce_nonnegative_storage(
    const ExchangeCellState& cell,
    const ExchangeDecision& decision,
    double dt_sub) {
    if (dt_sub <= 0.0) {
        throw std::invalid_argument("dt_sub must be positive");
    }
    if (cell.phi_t < 0.0) {
        throw std::invalid_argument("phi_t must be non-negative");
    }
    if (cell.h < 0.0) {
        throw std::invalid_argument("h must be non-negative");
    }
    if (cell.area < 0.0) {
        throw std::invalid_argument("area must be non-negative");
    }
    if (decision.q_granted < 0.0) {
        throw std::invalid_argument("q_granted must be non-negative");
    }
    if (decision.v_granted < 0.0) {
        throw std::invalid_argument("v_granted must be non-negative");
    }
    if (decision.q_repay < 0.0) {
        throw std::invalid_argument("q_repay must be non-negative");
    }
    if (decision.v_repay < 0.0) {
        throw std::invalid_argument("v_repay must be non-negative");
    }
    if (decision.v_unmet < 0.0) {
        throw std::invalid_argument("v_unmet must be non-negative");
    }

    const double available_storage = cell.phi_t * cell.h * cell.area;
    if (decision.v_granted <= available_storage) {
        return decision;
    }

    const double additional_unmet = decision.v_granted - available_storage;
    return ExchangeDecision{
        .q_granted = available_storage / dt_sub,
        .v_granted = available_storage,
        .q_repay = decision.q_repay,
        .v_repay = decision.v_repay,
        .v_unmet = decision.v_unmet + additional_unmet,
    };
}
```

- [ ] **Step 3: Run focused nonnegative storage backoff test**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_nonnegative_storage_backoff --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_nonnegative_storage_backoff.exe
```

Expected: PASS, `5 tests` pass in `CouplingNonnegativeStorageBackoff`.

- [ ] **Step 4: Run existing focused coupling tests to confirm no regression**

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

## Task 3: Validate M21 And Record Evidence

**Files:**
- Create: `superpowers/specs/2026-05-24-plan-m21-coupling-core-nonnegative-storage-backoff-evidence.md`

- [ ] **Step 1: Run full CTest**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure
```

Expected: PASS, full CTest passes with all tests successful (M20 baseline was 27/27; M21 adds one focused target, so 28/28).

- [ ] **Step 2: Create M21 evidence document**

Create `superpowers/specs/2026-05-24-plan-m21-coupling-core-nonnegative-storage-backoff-evidence.md` with this structure, filling the observed pass counts from the command output:

```markdown
# M21 CouplingLib Core Nonnegative Storage Backoff Evidence

**Date:** 2026-05-24
**Design:** `superpowers/specs/2026-05-24-m21-coupling-core-nonnegative-storage-backoff-design.md`
**Plan:** `superpowers/plans/2026-05-24-plan-m21-coupling-core-nonnegative-storage-backoff.md`

## Scope

M21 adds the pure `CouplingLib` core `enforce_nonnegative_storage(...)` decision that clamps a single donor exchange decision so `v_granted` cannot exceed donor-side physical storage `phi_t * h * A`.

## Local Validation

- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_nonnegative_storage_backoff --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_nonnegative_storage_backoff.exe`: PASS, 5/5 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_drain_split --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_drain_split.exe`: PASS, 6/6 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_exchange_decision --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_exchange_decision.exe`: PASS, 5/5 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_core_state --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_core_state.exe`: PASS, 7/7 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_mass_deficit_account --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_mass_deficit_account.exe`: PASS, 5/5 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_flow_limit --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_flow_limit.exe`: PASS, 4/4 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure`: PASS, full CTest passed `<observed>/<observed>`.

## Coverage

- `test_coupling_nonnegative_storage_backoff`: leaves decisions below available storage unchanged.
- `test_coupling_nonnegative_storage_backoff`: leaves decisions exactly at available storage unchanged.
- `test_coupling_nonnegative_storage_backoff`: clamps grants above available storage and accrues additional unmet volume.
- `test_coupling_nonnegative_storage_backoff`: zero available storage moves all granted volume to unmet.
- `test_coupling_nonnegative_storage_backoff`: rejects invalid `dt_sub`, negative geometry, and negative decision fields.
- `test_coupling_drain_split`, `test_coupling_exchange_decision`, `test_coupling_core_state`, `test_coupling_mass_deficit_account`, `test_coupling_flow_limit`: existing behavior remains intact.

## Boundaries

- M21 does not implement the multi-donor linear programming form of spec §6.1 rule 5.
- M21 does not introduce arbitration, `priority_weight`, shared-cell splitting, or `V_limit_k`.
- M21 does not introduce `count_negative_depth_fix` or any runtime counter plumbing.
- M21 does not introduce `epsilon_deficit`, scheduler, fault, or write-off logic.
- M21 does not connect `enforce_nonnegative_storage(...)` to `Surface2DCore`, SWMM, D-Flow FM, or any 1D engine ABI.
- M21 does not mutate `CouplingState`.
```

- [ ] **Step 3: Inspect status and diff**

Run:

```bash
git status --short
git diff -- libs/coupling/core/include/coupling/core/state.hpp libs/coupling/core/src/state.cpp tests/unit/coupling/CMakeLists.txt tests/unit/coupling/test_coupling_nonnegative_storage_backoff.cpp superpowers/specs/2026-05-24-m21-coupling-core-nonnegative-storage-backoff-design.md superpowers/plans/2026-05-24-plan-m21-coupling-core-nonnegative-storage-backoff.md superpowers/specs/2026-05-24-plan-m21-coupling-core-nonnegative-storage-backoff-evidence.md
```

Expected: Only M21 files are modified or created, plus any pre-existing untracked transcript remains excluded from future commits.

---

## Boundaries

- Preserve `Q_limit` / `V_limit` definition and machine-facing names.
- Clamp against donor physical storage `phi_t * h * A`, not raw geometric `h * A` and not `V_limit`.
- Do not implement multi-donor linear programming in this slice.
- Do not introduce arbitration, `priority_weight`, shared-cell splitting, `count_negative_depth_fix` counter, scheduler, fault, or write-off logic.
- Do not connect `enforce_nonnegative_storage(...)` to SWMM, D-Flow FM, `Surface2DCore`, or any 1D engine adapter; M21 stays pure-core.
- `enforce_nonnegative_storage(...)` must remain a pure function and must not mutate `CouplingState`.

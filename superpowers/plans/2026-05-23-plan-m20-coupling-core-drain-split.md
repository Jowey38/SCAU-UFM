# M20 CouplingLib Core Drain Split Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a pure `split_drain(...)` function that converts a single arbitrated exchange decision into a micro-step plan obeying spec §6.1 rule 4: split when `Q_granted * dt > 0.2 * phi_t * h * A`, with the micro-step count capped at 5.

**Architecture:** Introduce a small `DrainSplit` value type and one pure function in `libs/coupling/core`. Reuse the M19 `ExchangeDecision` and the existing `ExchangeCellState` fields without touching M16-M19 logic. The function is pure and never mutates `CouplingState`.

**Tech Stack:** C++20, CMake, GoogleTest, existing `scau::coupling_core` and `scau::warnings` targets.

---

## File Structure

- Modify `libs/coupling/core/include/coupling/core/state.hpp`
  - Add `DrainSplit` struct after `ExchangeDecision`.
  - Declare `[[nodiscard]] DrainSplit split_drain(const ExchangeCellState&, const ExchangeDecision&, double dt_sub);`.
- Modify `libs/coupling/core/src/state.cpp`
  - Implement `split_drain(...)` using the 20 % smoothing rule with a hard cap of 5 micro-steps.
- Create `tests/unit/coupling/test_coupling_drain_split.cpp`
  - Add focused M20 regression tests.
- Modify `tests/unit/coupling/CMakeLists.txt`
  - Register `test_coupling_drain_split`.
- Create `superpowers/specs/2026-05-23-plan-m20-coupling-core-drain-split-evidence.md`
  - Record focused and full validation evidence after implementation.

---

## Task 1: Add Red Tests And Register Focused Target

**Files:**
- Create: `tests/unit/coupling/test_coupling_drain_split.cpp`
- Modify: `tests/unit/coupling/CMakeLists.txt`

- [ ] **Step 1: Create focused drain split tests**

Create `tests/unit/coupling/test_coupling_drain_split.cpp` with this content:

```cpp
#include <stdexcept>

#include <gtest/gtest.h>

#include "coupling/core/state.hpp"

namespace {

scau::coupling::core::ExchangeCellState make_cell() {
    return scau::coupling::core::ExchangeCellState{
        .volume = 0.0,
        .mass_deficit_account = {},
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
    };
}

scau::coupling::core::ExchangeDecision make_decision(double v_granted) {
    return scau::coupling::core::ExchangeDecision{
        .q_granted = 0.0,
        .v_granted = v_granted,
        .q_repay = 0.0,
        .v_repay = 0.0,
        .v_unmet = 0.0,
    };
}

}  // namespace

TEST(CouplingDrainSplit, KeepsSingleMicroStepWellBelowThreshold) {
    const auto cell = make_cell();
    const auto decision = make_decision(4.0);

    const auto split = scau::coupling::core::split_drain(cell, decision, 4.0);

    EXPECT_EQ(split.micro_steps, 1);
    EXPECT_DOUBLE_EQ(split.dt_micro, 4.0);
    EXPECT_DOUBLE_EQ(split.v_per_micro_step, 4.0);
}

TEST(CouplingDrainSplit, KeepsSingleMicroStepAtExactThreshold) {
    const auto cell = make_cell();
    const auto decision = make_decision(8.0);

    const auto split = scau::coupling::core::split_drain(cell, decision, 4.0);

    EXPECT_EQ(split.micro_steps, 1);
    EXPECT_DOUBLE_EQ(split.dt_micro, 4.0);
    EXPECT_DOUBLE_EQ(split.v_per_micro_step, 8.0);
}

TEST(CouplingDrainSplit, SplitsIntoTwoMicroStepsSlightlyAboveThreshold) {
    const auto cell = make_cell();
    const auto decision = make_decision(10.0);

    const auto split = scau::coupling::core::split_drain(cell, decision, 4.0);

    EXPECT_EQ(split.micro_steps, 2);
    EXPECT_DOUBLE_EQ(split.dt_micro, 2.0);
    EXPECT_DOUBLE_EQ(split.v_per_micro_step, 5.0);
}

TEST(CouplingDrainSplit, CapsMicroStepsAtFive) {
    const auto cell = make_cell();
    const auto decision = make_decision(60.0);

    const auto split = scau::coupling::core::split_drain(cell, decision, 4.0);

    EXPECT_EQ(split.micro_steps, 5);
    EXPECT_DOUBLE_EQ(split.dt_micro, 0.8);
    EXPECT_DOUBLE_EQ(split.v_per_micro_step, 12.0);
}

TEST(CouplingDrainSplit, ReportsTrivialMicroStepForZeroGrantedVolume) {
    const auto cell = make_cell();
    const auto decision = make_decision(0.0);

    const auto split = scau::coupling::core::split_drain(cell, decision, 4.0);

    EXPECT_EQ(split.micro_steps, 1);
    EXPECT_DOUBLE_EQ(split.dt_micro, 4.0);
    EXPECT_DOUBLE_EQ(split.v_per_micro_step, 0.0);
}

TEST(CouplingDrainSplit, RejectsInvalidInputs) {
    const auto cell = make_cell();
    const auto valid_decision = make_decision(4.0);

    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::split_drain(cell, valid_decision, 0.0)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::split_drain(cell, valid_decision, -1.0)),
        std::invalid_argument);

    auto negative_phi_t = cell;
    negative_phi_t.phi_t = -0.1;
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::split_drain(negative_phi_t, valid_decision, 4.0)),
        std::invalid_argument);

    auto negative_h = cell;
    negative_h.h = -1.0;
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::split_drain(negative_h, valid_decision, 4.0)),
        std::invalid_argument);

    auto negative_area = cell;
    negative_area.area = -1.0;
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::split_drain(negative_area, valid_decision, 4.0)),
        std::invalid_argument);

    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::split_drain(cell, make_decision(-0.1), 4.0)),
        std::invalid_argument);

    auto zero_storage = cell;
    zero_storage.phi_t = 0.0;
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::split_drain(zero_storage, make_decision(1.0), 4.0)),
        std::invalid_argument);
}
```

- [ ] **Step 2: Register the focused test target**

Append the following block to `tests/unit/coupling/CMakeLists.txt`:

```cmake

add_executable(test_coupling_drain_split test_coupling_drain_split.cpp)
target_link_libraries(test_coupling_drain_split
    PRIVATE
        scau::coupling_core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_drain_split COMMAND test_coupling_drain_split)
```

- [ ] **Step 3: Run the focused test and verify it fails before implementation**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_drain_split --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_drain_split.exe
```

Expected: FAIL during compilation because `DrainSplit` and `split_drain(...)` are not declared yet.

---

## Task 2: Implement Drain Split Decision

**Files:**
- Modify: `libs/coupling/core/include/coupling/core/state.hpp`
- Modify: `libs/coupling/core/src/state.cpp`

- [ ] **Step 1: Extend the public API**

In `libs/coupling/core/include/coupling/core/state.hpp`, add the following block immediately after the `evaluate_exchange(...)` declaration and before `[[nodiscard]] MassDeficitAccount roll_deficit(...)`:

```cpp
struct DrainSplit {
    int micro_steps{1};
    double dt_micro{0.0};
    double v_per_micro_step{0.0};
};

[[nodiscard]] DrainSplit split_drain(
    const ExchangeCellState& cell,
    const ExchangeDecision& decision,
    double dt_sub);
```

- [ ] **Step 2: Implement the 20% smoothing rule with a hard cap of 5**

In `libs/coupling/core/src/state.cpp`, add `#include <cmath>` next to the existing `<algorithm>` include if it is not already present, then add the following function definition after `evaluate_exchange(...)` and before `MassDeficitAccount roll_deficit(...)`:

```cpp
DrainSplit split_drain(const ExchangeCellState& cell, const ExchangeDecision& decision, double dt_sub) {
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
    if (decision.v_granted < 0.0) {
        throw std::invalid_argument("v_granted must be non-negative");
    }

    const double threshold = 0.2 * cell.phi_t * cell.h * cell.area;
    if (decision.v_granted <= threshold) {
        return DrainSplit{
            .micro_steps = 1,
            .dt_micro = dt_sub,
            .v_per_micro_step = decision.v_granted,
        };
    }
    if (threshold <= 0.0) {
        throw std::invalid_argument("geometric storage must be positive when v_granted is positive");
    }

    const int requested = static_cast<int>(std::ceil(decision.v_granted / threshold));
    const int micro_steps = std::min(5, requested);
    return DrainSplit{
        .micro_steps = micro_steps,
        .dt_micro = dt_sub / micro_steps,
        .v_per_micro_step = decision.v_granted / micro_steps,
    };
}
```

- [ ] **Step 3: Run focused drain split test**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_drain_split --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_drain_split.exe
```

Expected: PASS, `6 tests` pass in `CouplingDrainSplit`.

- [ ] **Step 4: Run existing focused coupling tests to confirm no regression**

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

## Task 3: Validate M20 And Record Evidence

**Files:**
- Create: `superpowers/specs/2026-05-23-plan-m20-coupling-core-drain-split-evidence.md`

- [ ] **Step 1: Run full CTest**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure
```

Expected: PASS, full CTest passes with all tests successful (M19 baseline was 26/26; M20 adds one focused target, so 27/27).

- [ ] **Step 2: Create M20 evidence document**

Create `superpowers/specs/2026-05-23-plan-m20-coupling-core-drain-split-evidence.md` with this structure, filling the observed pass counts from the command output:

```markdown
# M20 CouplingLib Core Drain Split Evidence

**Date:** 2026-05-23
**Design:** `superpowers/specs/2026-05-23-m20-coupling-core-drain-split-design.md`
**Plan:** `superpowers/plans/2026-05-23-plan-m20-coupling-core-drain-split.md`

## Scope

M20 adds the pure `CouplingLib` core `split_drain(...)` decision that splits a single arbitrated exchange decision into micro-steps following spec §6.1 rule 4 (20% smoothing rule), with the micro-step count capped at 5.

## Local Validation

- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_drain_split --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_drain_split.exe`: PASS, 6/6 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_exchange_decision --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_exchange_decision.exe`: PASS, 5/5 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_core_state --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_core_state.exe`: PASS, 7/7 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_mass_deficit_account --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_mass_deficit_account.exe`: PASS, 5/5 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_flow_limit --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_flow_limit.exe`: PASS, 4/4 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure`: PASS, full CTest passed `<observed>/<observed>`.

## Coverage

- `test_coupling_drain_split`: granted volume well below the threshold stays a single micro-step.
- `test_coupling_drain_split`: granted volume exactly at the threshold stays a single micro-step.
- `test_coupling_drain_split`: granted volume slightly above the threshold splits into two equal micro-steps.
- `test_coupling_drain_split`: granted volume requiring more than five micro-steps is capped at five.
- `test_coupling_drain_split`: zero granted volume reports a single trivial micro-step.
- `test_coupling_drain_split`: rejects non-positive `dt_sub`, negative geometry, negative `v_granted`, and zero storage with positive `v_granted`.
- `test_coupling_exchange_decision`, `test_coupling_core_state`, `test_coupling_mass_deficit_account`, `test_coupling_flow_limit`: existing behavior remains intact.

## Boundaries

- M20 does not introduce `count_drain_split` runtime counter or any counter plumbing.
- M20 does not introduce arbitration, `priority_weight`, or multi-engine shared-cell splitting.
- M20 does not introduce `epsilon_deficit`, scheduler, fault, or write-off logic.
- M20 does not connect `split_drain(...)` to `Surface2DCore`, SWMM, D-Flow FM, or any 1D engine ABI.
- M20 does not mutate `CouplingState`; downstream callers must still feed micro-step decisions through `CouplingEvent` and the existing snapshot/rollback/replay path.
```

- [ ] **Step 3: Inspect status and diff**

Run:

```bash
git status --short
git diff -- libs/coupling/core/include/coupling/core/state.hpp libs/coupling/core/src/state.cpp tests/unit/coupling/CMakeLists.txt tests/unit/coupling/test_coupling_drain_split.cpp superpowers/specs/2026-05-23-m20-coupling-core-drain-split-design.md superpowers/plans/2026-05-23-plan-m20-coupling-core-drain-split.md superpowers/specs/2026-05-23-plan-m20-coupling-core-drain-split-evidence.md
```

Expected: Only M20 files are modified or created, plus any pre-existing untracked transcript remains excluded from future commits.

---

## Boundaries

- Preserve `Q_limit` / `V_limit` definition and machine-facing names.
- Keep `drain_split` threshold reference base on the overall geometric storage `phi_t * h * A`, never on `V_limit` or `V_limit_k`.
- Keep the micro-step count cap at 5 verbatim per spec §6.1 rule 4.
- Do not introduce arbitration, `priority_weight`, multi-engine shared-cell splitting, `count_drain_split` counter, scheduler, fault, or write-off logic in this slice.
- Do not connect `split_drain(...)` to SWMM, D-Flow FM, `Surface2DCore`, or any 1D engine adapter; M20 stays pure-core.
- `split_drain(...)` must remain a pure function and must not mutate `CouplingState`.

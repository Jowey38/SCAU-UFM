# M16 CouplingLib Core Q_limit Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Keep M16 limited to pure `CouplingLib` core `V_limit` / `Q_limit` calculations. Do not connect SWMM, D-Flow FM, `Surface2DCore`, or any 1D engine ABI.

**Goal:** Add the canonical pure core `V_limit = 0.9 * phi_t * h * A` and `Q_limit = V_limit / dt_sub` calculation for exchange-cell hard-gate flow limits.

**Architecture:** Extend `ExchangeCellState` with the canonical fields needed by the limit formula, add a small `FlowLimit` value type, and implement `compute_flow_limit(...)` in `libs/coupling/core`. Tests stay under `tests/unit/coupling` and verify formula, zero-limit cases, and invalid boundary inputs.

**Tech Stack:** C++20, CMake, GoogleTest, existing `scau::coupling_core` and `scau::warnings` targets.

---

## File Structure

- Modify `libs/coupling/core/include/coupling/core/state.hpp`
  - Adds `phi_t`, `h`, and `area` to `ExchangeCellState`.
  - Adds `FlowLimit` and `compute_flow_limit(...)` declaration.
- Modify `libs/coupling/core/src/state.cpp`
  - Implements `compute_flow_limit(...)` and input validation.
- Create `tests/unit/coupling/test_coupling_flow_limit.cpp`
  - Adds focused M16 regression tests.
- Modify `tests/unit/coupling/CMakeLists.txt`
  - Registers `test_coupling_flow_limit`.
- Create `superpowers/specs/2026-05-20-plan-m16-coupling-core-q-limit-evidence.md`
  - Records focused and full validation evidence.

---

## Task 1: Add Flow-Limit Tests And API

**Files:**
- Modify: `libs/coupling/core/include/coupling/core/state.hpp`
- Modify: `libs/coupling/core/src/state.cpp`
- Create: `tests/unit/coupling/test_coupling_flow_limit.cpp`
- Modify: `tests/unit/coupling/CMakeLists.txt`

- [ ] **Step 1: Create focused flow-limit tests**

Create `tests/unit/coupling/test_coupling_flow_limit.cpp`:

```cpp
#include <stdexcept>

#include <gtest/gtest.h>

#include "coupling/core/state.hpp"

TEST(CouplingFlowLimit, ComputesCanonicalVolumeAndFlowLimit) {
    const scau::coupling::core::ExchangeCellState cell{
        .volume = 0.0,
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
    };

    const auto limit = scau::coupling::core::compute_flow_limit(cell, 4.0);

    EXPECT_DOUBLE_EQ(limit.v_limit, 36.0);
    EXPECT_DOUBLE_EQ(limit.q_limit, 9.0);
}

TEST(CouplingFlowLimit, ZeroStorageInputsProduceZeroLimits) {
    const scau::coupling::core::ExchangeCellState zero_phi_t{
        .volume = 0.0,
        .phi_t = 0.0,
        .h = 2.0,
        .area = 50.0,
    };
    const scau::coupling::core::ExchangeCellState zero_h{
        .volume = 0.0,
        .phi_t = 0.4,
        .h = 0.0,
        .area = 50.0,
    };
    const scau::coupling::core::ExchangeCellState zero_area{
        .volume = 0.0,
        .phi_t = 0.4,
        .h = 2.0,
        .area = 0.0,
    };

    EXPECT_DOUBLE_EQ(scau::coupling::core::compute_flow_limit(zero_phi_t, 4.0).v_limit, 0.0);
    EXPECT_DOUBLE_EQ(scau::coupling::core::compute_flow_limit(zero_phi_t, 4.0).q_limit, 0.0);
    EXPECT_DOUBLE_EQ(scau::coupling::core::compute_flow_limit(zero_h, 4.0).v_limit, 0.0);
    EXPECT_DOUBLE_EQ(scau::coupling::core::compute_flow_limit(zero_h, 4.0).q_limit, 0.0);
    EXPECT_DOUBLE_EQ(scau::coupling::core::compute_flow_limit(zero_area, 4.0).v_limit, 0.0);
    EXPECT_DOUBLE_EQ(scau::coupling::core::compute_flow_limit(zero_area, 4.0).q_limit, 0.0);
}

TEST(CouplingFlowLimit, RejectsNonPositiveDtSub) {
    const scau::coupling::core::ExchangeCellState cell{
        .volume = 0.0,
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
    };

    EXPECT_THROW(scau::coupling::core::compute_flow_limit(cell, 0.0), std::invalid_argument);
    EXPECT_THROW(scau::coupling::core::compute_flow_limit(cell, -1.0), std::invalid_argument);
}

TEST(CouplingFlowLimit, RejectsNegativeStorageInputs) {
    const scau::coupling::core::ExchangeCellState negative_phi_t{
        .volume = 0.0,
        .phi_t = -0.1,
        .h = 2.0,
        .area = 50.0,
    };
    const scau::coupling::core::ExchangeCellState negative_h{
        .volume = 0.0,
        .phi_t = 0.4,
        .h = -2.0,
        .area = 50.0,
    };
    const scau::coupling::core::ExchangeCellState negative_area{
        .volume = 0.0,
        .phi_t = 0.4,
        .h = 2.0,
        .area = -50.0,
    };

    EXPECT_THROW(scau::coupling::core::compute_flow_limit(negative_phi_t, 4.0), std::invalid_argument);
    EXPECT_THROW(scau::coupling::core::compute_flow_limit(negative_h, 4.0), std::invalid_argument);
    EXPECT_THROW(scau::coupling::core::compute_flow_limit(negative_area, 4.0), std::invalid_argument);
}
```

- [ ] **Step 2: Register the focused test target**

Append this block to `tests/unit/coupling/CMakeLists.txt`:

```cmake
add_executable(test_coupling_flow_limit test_coupling_flow_limit.cpp)
target_link_libraries(test_coupling_flow_limit
    PRIVATE
        scau::coupling_core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_flow_limit COMMAND test_coupling_flow_limit)
```

- [ ] **Step 3: Run the focused test and verify it fails before implementation**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_flow_limit --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_flow_limit.exe
```

Expected before implementation: compile failure because `ExchangeCellState` does not yet expose `phi_t`, `h`, `area`, `FlowLimit`, or `compute_flow_limit(...)`.

- [ ] **Step 4: Extend the public API**

In `libs/coupling/core/include/coupling/core/state.hpp`, replace:

```cpp
struct ExchangeCellState {
    double volume{0.0};
};

struct CouplingEvent {
```

with:

```cpp
struct ExchangeCellState {
    double volume{0.0};
    double phi_t{0.0};
    double h{0.0};
    double area{0.0};
};

struct FlowLimit {
    double v_limit{0.0};
    double q_limit{0.0};
};

[[nodiscard]] FlowLimit compute_flow_limit(const ExchangeCellState& cell, double dt_sub);

struct CouplingEvent {
```

- [ ] **Step 5: Implement the formula and validation**

In `libs/coupling/core/src/state.cpp`, after the namespace opening, insert:

```cpp
FlowLimit compute_flow_limit(const ExchangeCellState& cell, double dt_sub) {
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

    const double v_limit = 0.9 * cell.phi_t * cell.h * cell.area;
    return FlowLimit{
        .v_limit = v_limit,
        .q_limit = v_limit / dt_sub,
    };
}

```

- [ ] **Step 6: Run focused coupling tests**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_flow_limit --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_flow_limit.exe
```

Expected: 4/4 tests pass.

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_core_state --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_core_state.exe
```

Expected: 4/4 tests pass.

---

## Task 2: Validate M16 And Record Evidence

**Files:**
- Create: `superpowers/specs/2026-05-20-plan-m16-coupling-core-q-limit-evidence.md`

- [ ] **Step 1: Run full CTest**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure
```

Expected: full test suite passes, including `test_coupling_flow_limit`.

- [ ] **Step 2: Create evidence document**

Create `superpowers/specs/2026-05-20-plan-m16-coupling-core-q-limit-evidence.md` with observed pass counts:

```markdown
# M16 CouplingLib Core Q_limit Evidence

**Date:** 2026-05-20
**Design:** `superpowers/specs/2026-05-20-m16-coupling-core-q-limit-design.md`
**Plan:** `superpowers/plans/2026-05-20-plan-m16-coupling-core-q-limit.md`

## Scope

M16 adds the canonical pure core `V_limit = 0.9 * phi_t * h * A` and `Q_limit = V_limit / dt_sub` calculation for exchange-cell hard-gate flow limits.

## Local Validation

- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_flow_limit --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_flow_limit.exe`: PASS, replace with observed pass count.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_core_state --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_core_state.exe`: PASS, replace with observed pass count.
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure`: PASS, replace with observed pass count.

## Coverage

- `test_coupling_flow_limit`: computes `V_limit = 0.9 * phi_t * h * A` and `Q_limit = V_limit / dt_sub`.
- `test_coupling_flow_limit`: zero `phi_t`, `h`, or `area` produces zero limits.
- `test_coupling_flow_limit`: non-positive `dt_sub` is rejected.
- `test_coupling_flow_limit`: negative `phi_t`, `h`, or `area` is rejected.
- `test_coupling_core_state`: existing snapshot, rollback, and replay behavior remains intact after extending `ExchangeCellState`.

## Boundaries

- M16 does not connect to `Surface2DCore`, SWMM, D-Flow FM, or any 1D engine ABI.
- M16 does not introduce `Q_max_safe` or any alias for `Q_limit`.
- M16 does not implement `mass_deficit_account`, shared-cell arbitration, adaptive timestep selection, fault-state orchestration, or GoldenSuite release evidence.
```

---

## Boundaries

- Preserve the two-step expression: `V_limit = 0.9 * phi_t * h * A`, then `Q_limit = V_limit / dt_sub`.
- Do not introduce `Q_max_safe`.
- Do not add Surface2D, SWMM, D-Flow FM, or 1D engine dependencies.
- Do not implement deficit ledger, arbitration, or scheduler semantics.

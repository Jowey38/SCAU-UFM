# M19 CouplingLib Core Q_limit Exchange Clamp Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a pure `evaluate_exchange(...)` function that clamps a single donor request against the canonical `Q_limit`, repays existing deficit first, and emits the unmet volume to be accrued into `mass_deficit_account`.

**Architecture:** Introduce two small value types (`ExchangeRequest`, `ExchangeDecision`) and one pure function in `libs/coupling/core`. Reuse M16's `compute_flow_limit(...)` for hard-gate validation, leave M17 ledger primitives unchanged, and stay compatible with M18's `CouplingEvent.unmet_volume` / `repayment_volume` so call sites can feed the decision back through the existing snapshot/rollback/replay path.

**Tech Stack:** C++20, CMake, GoogleTest, existing `scau::coupling_core` and `scau::warnings` targets.

---

## File Structure

- Modify `libs/coupling/core/include/coupling/core/state.hpp`
  - Add `ExchangeRequest`, `ExchangeDecision` types after `FlowLimit`.
  - Declare `[[nodiscard]] ExchangeDecision evaluate_exchange(const ExchangeCellState&, const ExchangeRequest&);`.
- Modify `libs/coupling/core/src/state.cpp`
  - Implement `evaluate_exchange(...)` using `compute_flow_limit(...)` and `std::min`.
- Create `tests/unit/coupling/test_coupling_exchange_decision.cpp`
  - Add focused M19 regression tests.
- Modify `tests/unit/coupling/CMakeLists.txt`
  - Register `test_coupling_exchange_decision`.
- Create `superpowers/specs/2026-05-22-plan-m19-coupling-core-q-limit-exchange-clamp-evidence.md`
  - Record focused and full validation evidence after implementation.

---

## Task 1: Add Red Tests And Register Focused Target

**Files:**
- Create: `tests/unit/coupling/test_coupling_exchange_decision.cpp`
- Modify: `tests/unit/coupling/CMakeLists.txt`

- [ ] **Step 1: Create focused exchange decision tests**

Create `tests/unit/coupling/test_coupling_exchange_decision.cpp` with this content:

```cpp
#include <stdexcept>

#include <gtest/gtest.h>

#include "coupling/core/state.hpp"

namespace {

scau::coupling::core::ExchangeCellState make_cell(double deficit_volume) {
    return scau::coupling::core::ExchangeCellState{
        .volume = 0.0,
        .mass_deficit_account = {.volume = deficit_volume},
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
    };
}

}  // namespace

TEST(CouplingExchangeDecision, GrantsFullRequestBelowHardGateWithNoDeficit) {
    const auto cell = make_cell(0.0);
    const scau::coupling::core::ExchangeRequest request{.q_request = 4.0, .dt_sub = 4.0};

    const auto decision = scau::coupling::core::evaluate_exchange(cell, request);

    EXPECT_DOUBLE_EQ(decision.q_granted, 4.0);
    EXPECT_DOUBLE_EQ(decision.v_granted, 16.0);
    EXPECT_DOUBLE_EQ(decision.q_repay, 0.0);
    EXPECT_DOUBLE_EQ(decision.v_repay, 0.0);
    EXPECT_DOUBLE_EQ(decision.v_unmet, 0.0);
}

TEST(CouplingExchangeDecision, ClampsRequestAboveHardGateAndAccruesUnmetVolume) {
    const auto cell = make_cell(0.0);
    const scau::coupling::core::ExchangeRequest request{.q_request = 20.0, .dt_sub = 4.0};

    const auto decision = scau::coupling::core::evaluate_exchange(cell, request);

    EXPECT_DOUBLE_EQ(decision.q_granted, 9.0);
    EXPECT_DOUBLE_EQ(decision.v_granted, 36.0);
    EXPECT_DOUBLE_EQ(decision.q_repay, 0.0);
    EXPECT_DOUBLE_EQ(decision.v_repay, 0.0);
    EXPECT_DOUBLE_EQ(decision.v_unmet, 44.0);
}

TEST(CouplingExchangeDecision, RepaysDeficitBeforeGrantingRequest) {
    const auto cell = make_cell(8.0);
    const scau::coupling::core::ExchangeRequest request{.q_request = 5.0, .dt_sub = 4.0};

    const auto decision = scau::coupling::core::evaluate_exchange(cell, request);

    EXPECT_DOUBLE_EQ(decision.q_repay, 2.0);
    EXPECT_DOUBLE_EQ(decision.v_repay, 8.0);
    EXPECT_DOUBLE_EQ(decision.q_granted, 5.0);
    EXPECT_DOUBLE_EQ(decision.v_granted, 20.0);
    EXPECT_DOUBLE_EQ(decision.v_unmet, 0.0);
}

TEST(CouplingExchangeDecision, DeficitAtOrAboveHardGateBlocksNewRequest) {
    const auto cell = make_cell(40.0);
    const scau::coupling::core::ExchangeRequest request{.q_request = 5.0, .dt_sub = 4.0};

    const auto decision = scau::coupling::core::evaluate_exchange(cell, request);

    EXPECT_DOUBLE_EQ(decision.q_repay, 9.0);
    EXPECT_DOUBLE_EQ(decision.v_repay, 36.0);
    EXPECT_DOUBLE_EQ(decision.q_granted, 0.0);
    EXPECT_DOUBLE_EQ(decision.v_granted, 0.0);
    EXPECT_DOUBLE_EQ(decision.v_unmet, 20.0);
}

TEST(CouplingExchangeDecision, RejectsNegativeRequestOrDeficitVolume) {
    const auto cell = make_cell(1.0);
    const auto negative_cell = scau::coupling::core::ExchangeCellState{
        .volume = 0.0,
        .mass_deficit_account = {.volume = -0.1},
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
    };

    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::evaluate_exchange(
            cell, scau::coupling::core::ExchangeRequest{.q_request = -0.1, .dt_sub = 4.0})),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::evaluate_exchange(
            negative_cell, scau::coupling::core::ExchangeRequest{.q_request = 1.0, .dt_sub = 4.0})),
        std::invalid_argument);
}
```

- [ ] **Step 2: Register the focused test target**

Append the following block to `tests/unit/coupling/CMakeLists.txt`:

```cmake

add_executable(test_coupling_exchange_decision test_coupling_exchange_decision.cpp)
target_link_libraries(test_coupling_exchange_decision
    PRIVATE
        scau::coupling_core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_exchange_decision COMMAND test_coupling_exchange_decision)
```

- [ ] **Step 3: Run the focused test and verify it fails before implementation**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_exchange_decision --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_exchange_decision.exe
```

Expected: FAIL during compilation because `ExchangeRequest`, `ExchangeDecision`, and `evaluate_exchange(...)` are not declared yet.

---

## Task 2: Implement Exchange Clamp And Deficit Accrual

**Files:**
- Modify: `libs/coupling/core/include/coupling/core/state.hpp`
- Modify: `libs/coupling/core/src/state.cpp`

- [ ] **Step 1: Extend the public API**

In `libs/coupling/core/include/coupling/core/state.hpp`, add the following two structs and one declaration immediately after the existing `FlowLimit` block and before `[[nodiscard]] MassDeficitAccount roll_deficit(...)`:

```cpp
struct ExchangeRequest {
    double q_request{0.0};
    double dt_sub{0.0};
};

struct ExchangeDecision {
    double q_granted{0.0};
    double v_granted{0.0};
    double q_repay{0.0};
    double v_repay{0.0};
    double v_unmet{0.0};
};

[[nodiscard]] ExchangeDecision evaluate_exchange(
    const ExchangeCellState& cell,
    const ExchangeRequest& request);
```

- [ ] **Step 2: Implement deficit-first clamp**

In `libs/coupling/core/src/state.cpp`, add the following function definition after `compute_flow_limit(...)` and before `MassDeficitAccount roll_deficit(...)`:

```cpp
ExchangeDecision evaluate_exchange(const ExchangeCellState& cell, const ExchangeRequest& request) {
    if (request.q_request < 0.0) {
        throw std::invalid_argument("q_request must be non-negative");
    }
    if (cell.mass_deficit_account.volume < 0.0) {
        throw std::invalid_argument("deficit volume must be non-negative");
    }

    const auto limit = compute_flow_limit(cell, request.dt_sub);
    const double q_repay = std::min(cell.mass_deficit_account.volume / request.dt_sub, limit.q_limit);
    const double q_granted = std::min(request.q_request, limit.q_limit - q_repay);
    const double v_granted = q_granted * request.dt_sub;
    const double v_repay = q_repay * request.dt_sub;
    const double v_unmet = (request.q_request - q_granted) * request.dt_sub;

    return ExchangeDecision{
        .q_granted = q_granted,
        .v_granted = v_granted,
        .q_repay = q_repay,
        .v_repay = v_repay,
        .v_unmet = v_unmet,
    };
}
```

- [ ] **Step 3: Run focused exchange decision test**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_exchange_decision --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_exchange_decision.exe
```

Expected: PASS, `5 tests` pass in `CouplingExchangeDecision`.

- [ ] **Step 4: Run existing focused coupling tests to confirm no regression**

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

## Task 3: Validate M19 And Record Evidence

**Files:**
- Create: `superpowers/specs/2026-05-22-plan-m19-coupling-core-q-limit-exchange-clamp-evidence.md`

- [ ] **Step 1: Run full CTest**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure
```

Expected: PASS, full CTest passes with all tests successful (M18 baseline was 25/25; M19 adds one focused target, so 26/26).

- [ ] **Step 2: Create M19 evidence document**

Create `superpowers/specs/2026-05-22-plan-m19-coupling-core-q-limit-exchange-clamp-evidence.md` with this structure, filling the observed pass counts from the command output:

```markdown
# M19 CouplingLib Core Q_limit Exchange Clamp Evidence

**Date:** 2026-05-22
**Design:** `superpowers/specs/2026-05-22-m19-coupling-core-q-limit-exchange-clamp-design.md`
**Plan:** `superpowers/plans/2026-05-22-plan-m19-coupling-core-q-limit-exchange-clamp.md`

## Scope

M19 adds the pure `CouplingLib` core `evaluate_exchange(...)` decision that clamps a single exchange request against `Q_limit` and accrues unmet volume into the cell's `mass_deficit_account`.

## Local Validation

- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_exchange_decision --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_exchange_decision.exe`: PASS, 5/5 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_core_state --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_core_state.exe`: PASS, 7/7 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_mass_deficit_account --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_mass_deficit_account.exe`: PASS, 5/5 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_flow_limit --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_flow_limit.exe`: PASS, 4/4 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure`: PASS, full CTest passed `<observed>/<observed>`.

## Coverage

- `test_coupling_exchange_decision`: request below `Q_limit` with no deficit grants the full request.
- `test_coupling_exchange_decision`: request above `Q_limit` with no deficit clamps to `Q_limit` and accrues unmet volume.
- `test_coupling_exchange_decision`: deficit below the hard gate is fully repaid alongside the new request.
- `test_coupling_exchange_decision`: deficit at or above the hard gate consumes the full `Q_limit` and blocks the new request.
- `test_coupling_exchange_decision`: rejects negative request volume and negative deficit volume.
- `test_coupling_core_state`, `test_coupling_mass_deficit_account`, `test_coupling_flow_limit`: existing behavior remains intact.

## Boundaries

- M19 does not introduce arbitration, `priority_weight`, or multi-engine shared-cell splitting.
- M19 does not introduce `drain_split`, `epsilon_deficit`, scheduler, fault, or write-off logic.
- M19 does not connect `evaluate_exchange(...)` to `Surface2DCore`, SWMM, D-Flow FM, or any 1D engine ABI.
- M19 does not mutate `CouplingState`; downstream callers must feed the decision through `CouplingEvent` and the existing snapshot/rollback/replay path.
```

- [ ] **Step 3: Inspect status and diff**

Run:

```bash
git status --short
git diff -- libs/coupling/core/include/coupling/core/state.hpp libs/coupling/core/src/state.cpp tests/unit/coupling/CMakeLists.txt tests/unit/coupling/test_coupling_exchange_decision.cpp superpowers/specs/2026-05-22-m19-coupling-core-q-limit-exchange-clamp-design.md superpowers/plans/2026-05-22-plan-m19-coupling-core-q-limit-exchange-clamp.md superpowers/specs/2026-05-22-plan-m19-coupling-core-q-limit-exchange-clamp-evidence.md
```

Expected: Only M19 files are modified or created, plus any pre-existing untracked transcript remains excluded from future commits.

---

## Boundaries

- Preserve `Q_limit` / `V_limit` definition and machine-facing names.
- Keep `mass_deficit_account` correctness strict; do not introduce `epsilon_deficit` tolerance in M19.
- Do not introduce arbitration, `priority_weight`, multi-engine shared-cell splitting, `drain_split`, scheduler, fault, or write-off logic in this slice.
- Do not connect `evaluate_exchange(...)` to SWMM, D-Flow FM, `Surface2DCore`, or any 1D engine adapter; M19 stays pure-core.
- `evaluate_exchange(...)` must remain a pure function and must not mutate `CouplingState`.

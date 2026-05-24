# M27 CouplingLib Core Per-Call Exchange Conservation Audit Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add a pure `audit_exchange_decision(...)` helper and `ExchangeConservationAudit` struct that lock the §6.3 request-side identity `v_granted + v_unmet == q_request * dt_sub` as a machine-asserted invariant with strict zero residual.

**Tech Stack:** C++20, CMake, GoogleTest, existing `scau::coupling_core` and `scau::warnings` targets.

---

## Task 1: Add Red Tests And Register Target

**Files:**
- Create: `tests/unit/coupling/test_coupling_exchange_audit.cpp`
- Modify: `tests/unit/coupling/CMakeLists.txt`

- [ ] **Step 1: Create test file**

Create `tests/unit/coupling/test_coupling_exchange_audit.cpp`:

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

TEST(CouplingExchangeAudit, SafeRequestIsBalancedWithZeroResidual) {
    const auto cell = make_cell();
    const scau::coupling::core::ExchangeRequest request{.q_request = 4.0, .dt_sub = 4.0};
    const auto decision = scau::coupling::core::evaluate_exchange_pipeline(cell, request);

    const auto audit = scau::coupling::core::audit_exchange_decision(request, decision);

    EXPECT_DOUBLE_EQ(audit.request_volume, 16.0);
    EXPECT_DOUBLE_EQ(audit.accounted_volume, 16.0);
    EXPECT_DOUBLE_EQ(audit.residual, 0.0);
    EXPECT_TRUE(audit.balanced);
}

TEST(CouplingExchangeAudit, HardGateClampScenarioIsBalanced) {
    const auto cell = make_cell();
    const scau::coupling::core::ExchangeRequest request{.q_request = 20.0, .dt_sub = 4.0};
    const auto decision = scau::coupling::core::evaluate_exchange_pipeline(cell, request);

    const auto audit = scau::coupling::core::audit_exchange_decision(request, decision);

    EXPECT_DOUBLE_EQ(audit.request_volume, 80.0);
    EXPECT_DOUBLE_EQ(audit.accounted_volume, 80.0);
    EXPECT_DOUBLE_EQ(audit.residual, 0.0);
    EXPECT_TRUE(audit.balanced);
}

TEST(CouplingExchangeAudit, DeficitRepaymentScenarioIsBalanced) {
    const auto cell = make_cell(8.0);
    const scau::coupling::core::ExchangeRequest request{.q_request = 5.0, .dt_sub = 4.0};
    const auto decision = scau::coupling::core::evaluate_exchange_pipeline(cell, request);

    const auto audit = scau::coupling::core::audit_exchange_decision(request, decision);

    EXPECT_DOUBLE_EQ(audit.request_volume, 20.0);
    EXPECT_DOUBLE_EQ(audit.accounted_volume, 20.0);
    EXPECT_DOUBLE_EQ(audit.residual, 0.0);
    EXPECT_TRUE(audit.balanced);
}

TEST(CouplingExchangeAudit, NonnegativeStorageBackoffScenarioIsBalanced) {
    const auto cell = make_cell(0.0, 0.2, 1.0, 50.0);
    const scau::coupling::core::ExchangeRequest request{.q_request = 5.0, .dt_sub = 4.0};
    const auto decision = scau::coupling::core::evaluate_exchange_pipeline(cell, request);

    const auto audit = scau::coupling::core::audit_exchange_decision(request, decision);

    EXPECT_DOUBLE_EQ(audit.request_volume, 20.0);
    EXPECT_DOUBLE_EQ(audit.accounted_volume, 20.0);
    EXPECT_DOUBLE_EQ(audit.residual, 0.0);
    EXPECT_TRUE(audit.balanced);
}

TEST(CouplingExchangeAudit, ManuallyConstructedImbalancedDecisionFlagsResidual) {
    const scau::coupling::core::ExchangeRequest request{.q_request = 4.0, .dt_sub = 4.0};
    const scau::coupling::core::ExchangePipelineDecision decision{
        .exchange = {.q_granted = 1.0, .v_granted = 4.0, .v_unmet = 8.0},
    };

    const auto audit = scau::coupling::core::audit_exchange_decision(request, decision);

    EXPECT_DOUBLE_EQ(audit.request_volume, 16.0);
    EXPECT_DOUBLE_EQ(audit.accounted_volume, 12.0);
    EXPECT_DOUBLE_EQ(audit.residual, 4.0);
    EXPECT_FALSE(audit.balanced);
}

TEST(CouplingExchangeAudit, RejectsNegativeRequestOrDecisionVolumes) {
    const scau::coupling::core::ExchangePipelineDecision good_decision{
        .exchange = {.v_granted = 0.0, .v_unmet = 0.0},
    };

    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::audit_exchange_decision(
            scau::coupling::core::ExchangeRequest{.q_request = -0.1, .dt_sub = 4.0}, good_decision)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::audit_exchange_decision(
            scau::coupling::core::ExchangeRequest{.q_request = 1.0, .dt_sub = 0.0}, good_decision)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::audit_exchange_decision(
            scau::coupling::core::ExchangeRequest{.q_request = 1.0, .dt_sub = 4.0},
            scau::coupling::core::ExchangePipelineDecision{
                .exchange = {.v_granted = -0.1, .v_unmet = 0.0}})),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::audit_exchange_decision(
            scau::coupling::core::ExchangeRequest{.q_request = 1.0, .dt_sub = 4.0},
            scau::coupling::core::ExchangePipelineDecision{
                .exchange = {.v_granted = 0.0, .v_unmet = -0.1}})),
        std::invalid_argument);
}
```

- [ ] **Step 2: Register the test target**

Append to `tests/unit/coupling/CMakeLists.txt`:

```cmake

add_executable(test_coupling_exchange_audit test_coupling_exchange_audit.cpp)
target_link_libraries(test_coupling_exchange_audit
    PRIVATE
        scau::coupling_core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_exchange_audit COMMAND test_coupling_exchange_audit)
```

- [ ] **Step 3: Run focused target and verify compile failure**

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_exchange_audit --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_exchange_audit.exe
```

Expected: Compile failure because `ExchangeConservationAudit` / `audit_exchange_decision` are not declared.

---

## Task 2: Implement Audit Helper

**Files:**
- Modify: `libs/coupling/core/include/coupling/core/state.hpp`
- Modify: `libs/coupling/core/src/state.cpp`

- [ ] **Step 1: Extend the public API**

In `libs/coupling/core/include/coupling/core/state.hpp`, add immediately after the `evaluate_exchange_pipeline(...)` declaration:

```cpp
struct ExchangeConservationAudit {
    double request_volume{0.0};
    double accounted_volume{0.0};
    double residual{0.0};
    bool balanced{true};
};

[[nodiscard]] ExchangeConservationAudit audit_exchange_decision(
    const ExchangeRequest& request,
    const ExchangePipelineDecision& decision);
```

- [ ] **Step 2: Implement the helper**

In `libs/coupling/core/src/state.cpp`, add immediately after `evaluate_exchange_pipeline(...)` and before `roll_deficit(...)`:

```cpp
ExchangeConservationAudit audit_exchange_decision(
    const ExchangeRequest& request,
    const ExchangePipelineDecision& decision) {
    if (request.dt_sub <= 0.0) {
        throw std::invalid_argument("dt_sub must be positive");
    }
    if (request.q_request < 0.0) {
        throw std::invalid_argument("q_request must be non-negative");
    }
    if (decision.exchange.v_granted < 0.0) {
        throw std::invalid_argument("v_granted must be non-negative");
    }
    if (decision.exchange.v_unmet < 0.0) {
        throw std::invalid_argument("v_unmet must be non-negative");
    }

    const double request_volume = request.q_request * request.dt_sub;
    const double accounted_volume = decision.exchange.v_granted + decision.exchange.v_unmet;
    const double residual = request_volume - accounted_volume;

    return ExchangeConservationAudit{
        .request_volume = request_volume,
        .accounted_volume = accounted_volume,
        .residual = residual,
        .balanced = (residual == 0.0),
    };
}
```

- [ ] **Step 3: Run focused audit test**

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_exchange_audit --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_exchange_audit.exe
```

Expected: PASS, 6/6 tests pass in `CouplingExchangeAudit`.

- [ ] **Step 4: Run remaining focused tests to confirm no regression**

Run the 8 existing coupling targets and expect:
- `test_coupling_apply_exchange`: 5/5
- `test_coupling_exchange_pipeline`: 6/6
- `test_coupling_nonnegative_storage_backoff`: 5/5
- `test_coupling_drain_split`: 6/6
- `test_coupling_exchange_decision`: 5/5
- `test_coupling_core_state`: 18/18
- `test_coupling_mass_deficit_account`: 5/5
- `test_coupling_flow_limit`: 4/4

---

## Task 3: Validate And Record Evidence

**Files:**
- Create: `superpowers/specs/2026-05-24-plan-m27-coupling-core-per-call-exchange-conservation-audit-evidence.md`

- [ ] **Step 1: Run full CTest**

```bash
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure
```

Expected: PASS, 31/31 (M26 baseline 30 + M27 adds 1 new test target).

- [ ] **Step 2: Create evidence document**

- [ ] **Step 3: Inspect status and diff**

---

## Boundaries

- Pure function; no state mutation.
- Strict equality only; no tolerance on the correctness path.
- No system-wide `M_ref` accumulation; per-call only.
- No multi-donor logic, FaultController, scheduler, or mapping.
- No connection to external engines.

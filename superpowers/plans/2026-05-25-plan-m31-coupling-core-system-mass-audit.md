# M31 CouplingLib Core System Mass Audit Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans.

**Goal:** Add `compute_system_mass(...)` and `audit_system_mass_against_reference(...)` pure helpers plus `SystemMassAudit` / `SystemMassDelta` value types in `libs/coupling/core`. Strict equality on the correctness path; no tolerance, no state mutation.

**Tech Stack:** C++20, CMake, GoogleTest, existing `scau::coupling_core` and `scau::warnings` targets.

---

## Task 1: Add Red Tests And Register Target

**Files:**
- Create: `tests/unit/coupling/test_coupling_system_mass_audit.cpp`
- Modify: `tests/unit/coupling/CMakeLists.txt`

- [ ] **Step 1: Create test file**

Create `tests/unit/coupling/test_coupling_system_mass_audit.cpp`:

```cpp
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "coupling/core/state.hpp"

namespace {

constexpr double kHWet = 1.0e-4;  // §11.4 default

scau::coupling::core::ExchangeCellState wet_cell(
    double h = 2.0,
    double phi_t = 0.4,
    double area = 50.0,
    double deficit = 0.0) {
    return scau::coupling::core::ExchangeCellState{
        .volume = 0.0,
        .mass_deficit_account = {.volume = deficit},
        .phi_t = phi_t,
        .h = h,
        .area = area,
    };
}

}  // namespace

TEST(CouplingSystemMassAudit, EmptyCellListProducesZeroMass) {
    const std::vector<scau::coupling::core::ExchangeCellState> empty;
    const auto audit = scau::coupling::core::compute_system_mass(empty, kHWet);

    EXPECT_DOUBLE_EQ(audit.surface_mass, 0.0);
    EXPECT_DOUBLE_EQ(audit.deficit_mass, 0.0);
    EXPECT_DOUBLE_EQ(audit.total_mass, 0.0);
    EXPECT_EQ(audit.wet_cell_count, 0U);
}

TEST(CouplingSystemMassAudit, SingleWetCellComputesPhiTHAreaProduct) {
    const std::vector cells{wet_cell(/*h=*/2.0, /*phi_t=*/0.4, /*area=*/50.0, /*deficit=*/0.0)};
    const auto audit = scau::coupling::core::compute_system_mass(cells, kHWet);

    EXPECT_DOUBLE_EQ(audit.surface_mass, 40.0);
    EXPECT_DOUBLE_EQ(audit.deficit_mass, 0.0);
    EXPECT_DOUBLE_EQ(audit.total_mass, 40.0);
    EXPECT_EQ(audit.wet_cell_count, 1U);
}

TEST(CouplingSystemMassAudit, DryCellsAreExcludedFromSurfaceMassButDeficitStillCounted) {
    const std::vector cells{
        wet_cell(/*h=*/2.0,        /*phi_t=*/0.4, /*area=*/50.0, /*deficit=*/1.5),
        wet_cell(/*h=*/kHWet / 2.0, /*phi_t=*/0.4, /*area=*/50.0, /*deficit=*/3.0),
    };
    const auto audit = scau::coupling::core::compute_system_mass(cells, kHWet);

    EXPECT_DOUBLE_EQ(audit.surface_mass, 40.0);     // only wet cell contributes
    EXPECT_DOUBLE_EQ(audit.deficit_mass, 4.5);      // both cells contribute deficit
    EXPECT_DOUBLE_EQ(audit.total_mass, 44.5);
    EXPECT_EQ(audit.wet_cell_count, 1U);
}

TEST(CouplingSystemMassAudit, BoundaryHEqualsHWetIsCountedAsWet) {
    const std::vector cells{wet_cell(/*h=*/kHWet, /*phi_t=*/0.4, /*area=*/50.0)};
    const auto audit = scau::coupling::core::compute_system_mass(cells, kHWet);

    EXPECT_DOUBLE_EQ(audit.surface_mass, 0.4 * kHWet * 50.0);
    EXPECT_EQ(audit.wet_cell_count, 1U);
}

TEST(CouplingSystemMassAudit, DeficitVolumesAccumulateAcrossAllCells) {
    const std::vector cells{
        wet_cell(/*h=*/2.0,        /*phi_t=*/0.4, /*area=*/50.0, /*deficit=*/1.0),
        wet_cell(/*h=*/2.0,        /*phi_t=*/0.4, /*area=*/50.0, /*deficit=*/2.0),
        wet_cell(/*h=*/kHWet / 2.0, /*phi_t=*/0.4, /*area=*/50.0, /*deficit=*/4.0),
    };
    const auto audit = scau::coupling::core::compute_system_mass(cells, kHWet);

    EXPECT_DOUBLE_EQ(audit.deficit_mass, 7.0);
}

TEST(CouplingSystemMassAudit, RejectsInvalidInputs) {
    const std::vector good{wet_cell()};

    EXPECT_THROW(static_cast<void>(scau::coupling::core::compute_system_mass(good, 0.0)),
                 std::invalid_argument);
    EXPECT_THROW(static_cast<void>(scau::coupling::core::compute_system_mass(good, -1e-6)),
                 std::invalid_argument);

    EXPECT_THROW(static_cast<void>(scau::coupling::core::compute_system_mass(
                     std::vector{wet_cell(2.0, -0.1)}, kHWet)),
                 std::invalid_argument);
    EXPECT_THROW(static_cast<void>(scau::coupling::core::compute_system_mass(
                     std::vector{wet_cell(-0.1)}, kHWet)),
                 std::invalid_argument);
    EXPECT_THROW(static_cast<void>(scau::coupling::core::compute_system_mass(
                     std::vector{wet_cell(2.0, 0.4, -1.0)}, kHWet)),
                 std::invalid_argument);
    EXPECT_THROW(static_cast<void>(scau::coupling::core::compute_system_mass(
                     std::vector{wet_cell(2.0, 0.4, 50.0, -0.1)}, kHWet)),
                 std::invalid_argument);
}

TEST(CouplingSystemMassAudit, AuditDetectsZeroResidualWhenStateUnchanged) {
    const std::vector cells{
        wet_cell(/*h=*/2.0, /*phi_t=*/0.4, /*area=*/50.0, /*deficit=*/1.0),
        wet_cell(/*h=*/1.5, /*phi_t=*/0.5, /*area=*/40.0, /*deficit=*/0.5),
    };
    const auto baseline = scau::coupling::core::compute_system_mass(cells, kHWet);
    const auto delta = scau::coupling::core::audit_system_mass_against_reference(
        baseline, cells, kHWet);

    EXPECT_DOUBLE_EQ(delta.residual, 0.0);
    EXPECT_TRUE(delta.conserved);
}

TEST(CouplingSystemMassAudit, AuditDetectsNonZeroResidualWhenMassMissing) {
    std::vector cells{wet_cell(/*h=*/2.0, /*phi_t=*/0.4, /*area=*/50.0)};
    const auto baseline = scau::coupling::core::compute_system_mass(cells, kHWet);

    cells[0].h = 1.0;  // surface_mass drops by 20, no deficit pickup
    const auto delta = scau::coupling::core::audit_system_mass_against_reference(
        baseline, cells, kHWet);

    EXPECT_DOUBLE_EQ(delta.residual, -20.0);
    EXPECT_FALSE(delta.conserved);
}

TEST(CouplingSystemMassAudit, AuditPreservesConservationWhenGrantedMatchedByDeficit) {
    std::vector cells{wet_cell(/*h=*/2.0, /*phi_t=*/0.4, /*area=*/50.0, /*deficit=*/0.0)};
    const auto baseline = scau::coupling::core::compute_system_mass(cells, kHWet);

    const double dh = 0.5;
    const double matching_deficit = cells[0].phi_t * dh * cells[0].area;
    cells[0].h -= dh;
    cells[0].mass_deficit_account.volume += matching_deficit;

    const auto delta = scau::coupling::core::audit_system_mass_against_reference(
        baseline, cells, kHWet);

    EXPECT_DOUBLE_EQ(delta.residual, 0.0);
    EXPECT_TRUE(delta.conserved);
}

TEST(CouplingSystemMassAudit, RejectsNegativeBaseline) {
    const std::vector cells{wet_cell()};
    scau::coupling::core::SystemMassAudit bad_baseline{};
    bad_baseline.total_mass = -1.0;

    EXPECT_THROW(static_cast<void>(scau::coupling::core::audit_system_mass_against_reference(
                     bad_baseline, cells, kHWet)),
                 std::invalid_argument);
}
```

- [ ] **Step 2: Register the test target**

Append to `tests/unit/coupling/CMakeLists.txt`:

```cmake

add_executable(test_coupling_system_mass_audit test_coupling_system_mass_audit.cpp)
target_link_libraries(test_coupling_system_mass_audit
    PRIVATE
        scau::coupling_core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_system_mass_audit COMMAND test_coupling_system_mass_audit)
```

- [ ] **Step 3: Run focused target and verify compile failure**

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_system_mass_audit --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_system_mass_audit.exe
```

Expected: compile failure — `SystemMassAudit`, `SystemMassDelta`, `compute_system_mass`, `audit_system_mass_against_reference` not declared.

---

## Task 2: Implement Helpers

**Files:**
- Modify: `libs/coupling/core/include/coupling/core/state.hpp`
- Modify: `libs/coupling/core/src/state.cpp`

- [ ] **Step 1: Extend the public API**

In `libs/coupling/core/include/coupling/core/state.hpp`, add immediately after the `audit_exchange_decision` declaration:

```cpp
struct SystemMassAudit {
    double surface_mass{0.0};
    double deficit_mass{0.0};
    double total_mass{0.0};
    std::size_t wet_cell_count{0};
};

struct SystemMassDelta {
    SystemMassAudit baseline{};
    SystemMassAudit current{};
    double residual{0.0};
    bool conserved{true};
};

[[nodiscard]] SystemMassAudit compute_system_mass(
    const std::vector<ExchangeCellState>& cells,
    double h_wet);

[[nodiscard]] SystemMassDelta audit_system_mass_against_reference(
    const SystemMassAudit& baseline,
    const std::vector<ExchangeCellState>& current_cells,
    double h_wet);
```

- [ ] **Step 2: Implement the helpers**

In `libs/coupling/core/src/state.cpp`, add immediately after `audit_exchange_decision`:

```cpp
SystemMassAudit compute_system_mass(
    const std::vector<ExchangeCellState>& cells,
    double h_wet) {
    if (h_wet <= 0.0) {
        throw std::invalid_argument("h_wet must be positive");
    }

    SystemMassAudit audit{};
    for (const auto& cell : cells) {
        if (cell.phi_t < 0.0) {
            throw std::invalid_argument("phi_t must be non-negative");
        }
        if (cell.h < 0.0) {
            throw std::invalid_argument("h must be non-negative");
        }
        if (cell.area < 0.0) {
            throw std::invalid_argument("area must be non-negative");
        }
        if (cell.mass_deficit_account.volume < 0.0) {
            throw std::invalid_argument("deficit volume must be non-negative");
        }

        if (cell.h >= h_wet) {
            audit.surface_mass += cell.phi_t * cell.h * cell.area;
            ++audit.wet_cell_count;
        }
        audit.deficit_mass += cell.mass_deficit_account.volume;
    }
    audit.total_mass = audit.surface_mass + audit.deficit_mass;
    return audit;
}

SystemMassDelta audit_system_mass_against_reference(
    const SystemMassAudit& baseline,
    const std::vector<ExchangeCellState>& current_cells,
    double h_wet) {
    if (baseline.total_mass < 0.0) {
        throw std::invalid_argument("baseline total_mass must be non-negative");
    }

    const auto current = compute_system_mass(current_cells, h_wet);
    const double residual = current.total_mass - baseline.total_mass;
    return SystemMassDelta{
        .baseline = baseline,
        .current = current,
        .residual = residual,
        .conserved = (residual == 0.0),
    };
}
```

- [ ] **Step 3: Run focused mass-audit test**

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_system_mass_audit --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_system_mass_audit.exe
```

Expected: PASS, 10/10 tests.

- [ ] **Step 4: Run all 9 existing coupling focused tests to confirm no regression**

Run each target. Expected: M30 baseline counts intact.

- `test_coupling_apply_exchange`: 5/5
- `test_coupling_exchange_audit`: 6/6
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
- Create: `superpowers/specs/2026-05-25-plan-m31-coupling-core-system-mass-audit-evidence.md`

- [ ] **Step 1: Run full CTest**

```bash
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure
```

Expected: PASS, 32/32 (M30 baseline 31 + M31 adds 1 new test target).

- [ ] **Step 2: Create evidence document**

Record concrete numbers (test counts, full CTest duration, normative coverage of §5.1 / §11.4 / §6.3 ledger inclusion).

- [ ] **Step 3: Inspect status and diff**

```bash
git status --short
git diff --stat -- libs/coupling/core/include/coupling/core/state.hpp libs/coupling/core/src/state.cpp tests/unit/coupling/CMakeLists.txt
```

Expected: 3 modified, 4 new (test + design + plan + evidence).

---

## Boundaries

- Pure helpers; no state mutation.
- Strict `==` on residual; no tolerance.
- No `epsilon_mass` performance-path comparison.
- No `CouplingState` integration / baseline persistence.
- No write-off ledger, multi-donor, scheduler, or fault hook.
- No external engine ABI connection.

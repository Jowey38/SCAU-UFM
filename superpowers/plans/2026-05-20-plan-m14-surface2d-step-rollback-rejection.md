# M14 Surface2D Step Rollback/Rejection Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Preserve `Surface2DCore` independence from SWMM, D-Flow FM, `CouplingLib`, and 1D engine ABIs. Keep M14 limited to rejected-step mutation semantics at the CPU reference step boundary.

**Goal:** Ensure `advance_one_step_cpu(...)` reports rollback for raw CFL violations and does not mutate `SurfaceState` when the attempted step is rejected.

**Architecture:** M14 builds on the existing `StepDiagnostics::rollback_required` flag. The CPU step computes diagnostics and residuals as before, then returns before depth/momentum updates when `rollback_required` is true. Tests cover both rejected no-mutation behavior and accepted mutation behavior so the guard does not disable normal advancement.

**Tech Stack:** C++20, CMake, GoogleTest, existing `scau::mesh` and `scau::surface2d` targets.

---

## File Structure

- Create `tests/unit/surface2d/test_step_rollback_rejection.cpp`
  - Adds focused tests for rejected-step no-mutation semantics and accepted-step mutation semantics.
- Modify `tests/unit/surface2d/CMakeLists.txt`
  - Adds `test_step_rollback_rejection` GoogleTest target and CTest registration.
- Modify `libs/surface2d/src/time_integration/step.cpp`
  - Adds the minimal guard that returns diagnostics before state updates when `rollback_required` is true.
- Create `superpowers/specs/2026-05-20-plan-m14-surface2d-step-rollback-rejection-evidence.md`
  - Records focused target, Surface2D regression subset, and full CTest evidence after implementation.

---

## Task 1: Add Rejected-Step No-Mutation Regression Tests

**Files:**
- Create: `tests/unit/surface2d/test_step_rollback_rejection.cpp`
- Modify: `tests/unit/surface2d/CMakeLists.txt`

- [ ] **Step 1: Create the focused test file**

Create `tests/unit/surface2d/test_step_rollback_rejection.cpp` with this content:

```cpp
#include <cstddef>
#include <vector>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/state/state.hpp"
#include "surface2d/time_integration/step.hpp"

namespace {

struct CellSnapshot {
    double h{0.0};
    double hu{0.0};
    double hv{0.0};
    double eta{0.0};
};

std::vector<CellSnapshot> snapshot_cells(const scau::surface2d::SurfaceState& state) {
    std::vector<CellSnapshot> snapshots;
    snapshots.reserve(state.cells.size());
    for (const auto& cell : state.cells) {
        snapshots.push_back(CellSnapshot{
            .h = cell.conserved.h,
            .hu = cell.conserved.hu,
            .hv = cell.conserved.hv,
            .eta = cell.eta,
        });
    }
    return snapshots;
}

void expect_cells_unchanged(
    const scau::surface2d::SurfaceState& state,
    const std::vector<CellSnapshot>& before) {
    ASSERT_EQ(state.cells.size(), before.size());
    for (std::size_t index = 0; index < state.cells.size(); ++index) {
        EXPECT_DOUBLE_EQ(state.cells[index].conserved.h, before[index].h);
        EXPECT_DOUBLE_EQ(state.cells[index].conserved.hu, before[index].hu);
        EXPECT_DOUBLE_EQ(state.cells[index].conserved.hv, before[index].hv);
        EXPECT_DOUBLE_EQ(state.cells[index].eta, before[index].eta);
    }
}

}  // namespace

TEST(SurfaceStepRollbackRejection, RejectedStepReportsRollbackAndDoesNotMutateState) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    state.cells[0].conserved.hu = 2.0;
    state.cells[0].conserved.hv = 0.5;
    const auto before = snapshot_cells(state);
    const auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);

    const scau::surface2d::StepConfig config{.dt = 0.5, .cfl_safety = 0.45, .c_rollback = 0.01};
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields);

    EXPECT_TRUE(diagnostics.rollback_required);
    EXPECT_GT(diagnostics.max_cell_cfl, config.c_rollback);
    ASSERT_EQ(diagnostics.cells.size(), mesh.cells.size());
    ASSERT_EQ(diagnostics.edges.size(), mesh.edges.size());
    expect_cells_unchanged(state, before);
}

TEST(SurfaceStepRollbackRejection, AcceptedStepKeepsExistingStateMutationPath) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    state.cells[0].conserved.hu = 2.0;
    state.cells[0].conserved.hv = 0.5;
    const auto before = snapshot_cells(state);
    const auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 100.0};
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields);

    EXPECT_FALSE(diagnostics.rollback_required);
    ASSERT_EQ(diagnostics.cells.size(), mesh.cells.size());

    bool any_cell_changed = false;
    for (std::size_t index = 0; index < state.cells.size(); ++index) {
        any_cell_changed = any_cell_changed || state.cells[index].conserved.h != before[index].h;
        any_cell_changed = any_cell_changed || state.cells[index].conserved.hu != before[index].hu;
        any_cell_changed = any_cell_changed || state.cells[index].conserved.hv != before[index].hv;
    }
    EXPECT_TRUE(any_cell_changed);
}
```

- [ ] **Step 2: Register the test target**

Append this block to `tests/unit/surface2d/CMakeLists.txt` after the existing Surface2D test targets:

```cmake
add_executable(test_step_rollback_rejection test_step_rollback_rejection.cpp)
target_link_libraries(test_step_rollback_rejection
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_step_rollback_rejection COMMAND test_step_rollback_rejection)
```

- [ ] **Step 3: Run the focused test and verify it fails**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_step_rollback_rejection --config Debug && build/windows-msvc/tests/unit/surface2d/Debug/test_step_rollback_rejection.exe
```

Expected before implementation: `RejectedStepReportsRollbackAndDoesNotMutateState` fails because the current CPU step applies depth/momentum updates even when `rollback_required` is true.

---

## Task 2: Add the Minimal Rejected-Step Update Guard

**Files:**
- Modify: `libs/surface2d/src/time_integration/step.cpp`

- [ ] **Step 1: Add the guard before state updates**

In `advance_one_step_cpu(const mesh::Mesh&, SurfaceState&, const StepConfig&, const DpmFields&, const BoundaryConditions&)`, replace the tail:

```cpp
    apply_depth_update(mesh, state, config, diagnostics.cells);
    apply_momentum_update(mesh, state, config, diagnostics.cells);
    return diagnostics;
```

with:

```cpp
    if (diagnostics.rollback_required) {
        return diagnostics;
    }
    apply_depth_update(mesh, state, config, diagnostics.cells);
    apply_momentum_update(mesh, state, config, diagnostics.cells);
    return diagnostics;
```

- [ ] **Step 2: Run the focused test and verify it passes**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_step_rollback_rejection --config Debug && build/windows-msvc/tests/unit/surface2d/Debug/test_step_rollback_rejection.exe
```

Expected: both tests pass.

---

## Task 3: Record Validation Evidence

**Files:**
- Create: `superpowers/specs/2026-05-20-plan-m14-surface2d-step-rollback-rejection-evidence.md`

- [ ] **Step 1: Run Surface2D regression subset**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug -R "surface_state|surface_step|dpm_fields|hydrostatic_reconstruction|hllc_flux|phi_t_source|surface2d_golden_minimal|cfl_rollback|dpm_edge_source_conservation|conservative_update|wetting_drying|boundary_flux|boundary_conditions|momentum_transport|pressure_momentum|hllc_wave_momentum|hllc_wave_mass|step_rollback_rejection" --output-on-failure
```

Expected: all listed Surface2D tests pass.

- [ ] **Step 2: Run full CTest**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure
```

Expected: full test suite passes.

- [ ] **Step 3: Create evidence document**

Create `superpowers/specs/2026-05-20-plan-m14-surface2d-step-rollback-rejection-evidence.md` with this content after replacing pass counts with observed results:

```markdown
# M14 Surface2D Step Rollback/Rejection Evidence

**Date:** 2026-05-20
**Design:** `superpowers/specs/2026-05-20-m14-surface2d-step-rollback-rejection-design.md`
**Plan:** `superpowers/plans/2026-05-20-plan-m14-surface2d-step-rollback-rejection.md`

## Scope

M14 adds focused CPU reference `Surface2DCore` regression coverage proving that rejected steps report rollback and do not commit depth or momentum updates to `SurfaceState`.

## Local Validation

- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_step_rollback_rejection --config Debug && build/windows-msvc/tests/unit/surface2d/Debug/test_step_rollback_rejection.exe`: PASS, 2/2 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug -R "surface_state|surface_step|dpm_fields|hydrostatic_reconstruction|hllc_flux|phi_t_source|surface2d_golden_minimal|cfl_rollback|dpm_edge_source_conservation|conservative_update|wetting_drying|boundary_flux|boundary_conditions|momentum_transport|pressure_momentum|hllc_wave_momentum|hllc_wave_mass|step_rollback_rejection" --output-on-failure`: PASS, replace with observed pass count.
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure`: PASS, replace with observed pass count.

## Coverage

- `test_step_rollback_rejection`: rejected CPU step reports `rollback_required == true`, keeps raw `max_cell_cfl > C_rollback`, emits attempted-step cell/edge diagnostics, and preserves every cell's `h`, `hu`, `hv`, and `eta`.
- `test_step_rollback_rejection`: accepted CPU step with permissive `C_rollback` keeps the existing state mutation path active.
- Existing CFL rollback tests continue to prove raw `max_cell_cfl` is not scaled by `CFL_safety` and is compared directly against `C_rollback`.

## Boundaries

- M14 does not add `CouplingLib`, global `snapshot / rollback / replay` APIs, `mass_deficit_account`, adaptive timestep selection, CUDA behavior, SWMM, D-Flow FM, 1D engine interfaces, or release-level GoldenSuite claims.
```

---

## Boundaries

- Do not change `max_cell_cfl` semantics.
- Do not scale `max_cell_cfl` by `CFL_safety`.
- Do not add adaptive timestep selection.
- Do not add `SurfaceSnapshot`, replay APIs, or `CouplingLib` semantics.
- Do not modify SWMM, D-Flow FM, or any 1D engine boundary.

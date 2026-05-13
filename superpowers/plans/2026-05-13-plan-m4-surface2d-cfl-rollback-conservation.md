# M4 Surface2D CFL Rollback Conservation Implementation Plan

> **For agentic workers:** Execute this plan task-by-task. Preserve `Surface2DCore` independence from SWMM, D-Flow FM, `CouplingLib`, and 1D engine ABIs. Keep `max_cell_cfl` as the raw physical CFL diagnostic and compare rollback only against `C_rollback`.

**Goal:** Add the first formal CFL/rollback and minimal conservative update path for the `Surface2DCore` CPU reference step.

**Architecture:** M4 extends the M3 minimal slice inside `libs/surface2d` only. The CPU step computes edge flux diagnostics, accumulates cell residuals from internal edges, advances conserved cell state with a finite-volume update, records raw physical CFL, and marks rollback when raw CFL exceeds `C_rollback`; no coupling, 1D engine, or GoldenSuite release-gate behavior is introduced.

**Tech Stack:** C++20, CMake, GoogleTest, existing `scau::core`, `scau::mesh`, and `scau::surface2d` targets.

---

## File Structure

- Modify `libs/surface2d/include/surface2d/time_integration/step.hpp`
  - Add cell residual/update diagnostics needed by tests and evidence.
  - Keep the existing no-DPM overload and DPM-aware overload signatures stable.
- Modify `libs/surface2d/src/time_integration/step.cpp`
  - Compute raw physical CFL.
  - Accumulate minimal conservative mass residuals across internal edges.
  - Apply one CPU reference update for `h`; leave momentum update out of M4 unless required by existing flux support.
  - Set `rollback_required` from `max_cell_cfl > config.c_rollback` only.
- Modify `tests/unit/surface2d/test_step.cpp`
  - Extend step tests for raw CFL, rollback threshold, and no-DPM compatibility.
- Create `tests/unit/surface2d/test_cfl_rollback.cpp`
  - Focused tests for raw CFL and rollback semantics.
- Create `tests/unit/surface2d/test_conservative_update.cpp`
  - Focused tests for internal-edge residual symmetry and depth update.
- Modify `tests/unit/surface2d/CMakeLists.txt`
  - Register the two new test executables.
- Create `superpowers/specs/2026-05-13-plan-m4-surface2d-cfl-rollback-conservation-evidence.md`
  - Record build/test evidence after implementation.

---

## Task 1: Add CFL Rollback Tests

**Files:**
- Create: `tests/unit/surface2d/test_cfl_rollback.cpp`
- Modify: `tests/unit/surface2d/CMakeLists.txt`

- [ ] **Step 1: Create failing CFL rollback tests**

Create `tests/unit/surface2d/test_cfl_rollback.cpp`:

```cpp
#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/state/state.hpp"
#include "surface2d/time_integration/step.hpp"

TEST(SurfaceCflRollback, ReportsRawPhysicalCflWithoutSafetyScaling) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    state.cells[0].conserved.hu = 2.0;
    const auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);

    const scau::surface2d::StepConfig config{.dt = 0.5, .cfl_safety = 0.25, .c_rollback = 10.0};
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields);

    EXPECT_DOUBLE_EQ(diagnostics.max_cell_cfl, 1.0);
    EXPECT_FALSE(diagnostics.rollback_required);
}

TEST(SurfaceCflRollback, ComparesRollbackAgainstCRollbackOnly) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    state.cells[0].conserved.hu = 2.0;
    const auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);

    const scau::surface2d::StepConfig config{.dt = 0.5, .cfl_safety = 0.25, .c_rollback = 0.75};
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields);

    EXPECT_DOUBLE_EQ(diagnostics.max_cell_cfl, 1.0);
    EXPECT_TRUE(diagnostics.rollback_required);
}
```

- [ ] **Step 2: Register the CFL rollback test**

Append to `tests/unit/surface2d/CMakeLists.txt`:

```cmake
add_executable(test_cfl_rollback test_cfl_rollback.cpp)
target_link_libraries(test_cfl_rollback
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_cfl_rollback COMMAND test_cfl_rollback)
```

- [ ] **Step 3: Run the new test and verify it fails**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_cfl_rollback
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R test_cfl_rollback
```

Expected: FAIL because `max_cell_cfl` is still reported as `0.0`.

---

## Task 2: Implement Raw CFL And Rollback

**Files:**
- Modify: `libs/surface2d/src/time_integration/step.cpp`
- Modify: `tests/unit/surface2d/test_step.cpp`

- [ ] **Step 1: Add raw CFL helpers**

In `libs/surface2d/src/time_integration/step.cpp`, add helper functions inside the anonymous namespace:

```cpp
core::Real cell_speed(const CellState& cell) {
    return std::hypot(cell.u(), cell.v());
}

core::Real raw_cell_cfl(const mesh::Mesh& mesh, const SurfaceState& state, const StepConfig& config) {
    core::Real max_cfl = 0.0;
    for (std::size_t cell_index = 0; cell_index < mesh.cells.size(); ++cell_index) {
        const core::Real area = mesh::cell_area(mesh, mesh.cells[cell_index]);
        if (area > 0.0) {
            max_cfl = std::max(max_cfl, config.dt * cell_speed(state.cells[cell_index]) / area);
        }
    }
    return max_cfl;
}
```

Also add:

```cpp
#include <algorithm>
#include <cmath>
```

- [ ] **Step 2: Use raw CFL in base diagnostics**

Change `base_diagnostics` to accept state and config:

```cpp
StepDiagnostics base_diagnostics(const mesh::Mesh& mesh, const SurfaceState& state, const StepConfig& config) {
    const core::Real max_cell_cfl = raw_cell_cfl(mesh, state, config);
    return StepDiagnostics{
        .cell_count = mesh.cells.size(),
        .edge_count = mesh.edges.size(),
        .max_cell_cfl = max_cell_cfl,
        .rollback_required = max_cell_cfl > config.c_rollback,
    };
}
```

Update both overloads to call `base_diagnostics(mesh, state, config)`.

- [ ] **Step 3: Preserve no-DPM overload compatibility test**

Update `tests/unit/surface2d/test_step.cpp` no-DPM test to keep zero velocity so it still expects:

```cpp
EXPECT_EQ(diagnostics.max_cell_cfl, 0.0);
EXPECT_FALSE(diagnostics.rollback_required);
EXPECT_TRUE(diagnostics.edges.empty());
```

- [ ] **Step 4: Run CFL tests**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_cfl_rollback test_surface_step
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "test_(cfl_rollback|surface_step)$"
```

Expected: PASS.

---

## Task 3: Add Conservative Update Tests

**Files:**
- Create: `tests/unit/surface2d/test_conservative_update.cpp`
- Modify: `tests/unit/surface2d/CMakeLists.txt`

- [ ] **Step 1: Create failing conservative update tests**

Create `tests/unit/surface2d/test_conservative_update.cpp`:

```cpp
#include <cstddef>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/state/state.hpp"
#include "surface2d/time_integration/step.hpp"

namespace {

std::size_t cell_index_by_id(const scau::mesh::Mesh& mesh, const std::string& cell_id) {
    for (std::size_t index = 0; index < mesh.cells.size(); ++index) {
        if (mesh.cells[index].id == cell_id) {
            return index;
        }
    }
    throw std::invalid_argument("mesh must contain referenced cell");
}

std::size_t first_internal_edge_index(const scau::mesh::Mesh& mesh) {
    for (std::size_t index = 0; index < mesh.edges.size(); ++index) {
        if (mesh.edges[index].left_cell.has_value() && mesh.edges[index].right_cell.has_value()) {
            return index;
        }
    }
    throw std::invalid_argument("mesh must contain an internal edge");
}

}  // namespace

TEST(SurfaceConservativeUpdate, InternalEdgeMassResidualIsAntisymmetric) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto edge_index = first_internal_edge_index(mesh);
    const auto left_index = cell_index_by_id(mesh, *mesh.edges[edge_index].left_cell);
    const auto right_index = cell_index_by_id(mesh, *mesh.edges[edge_index].right_cell);

    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    state.cells[left_index].conserved.hu = 1.0;
    state.cells[right_index].conserved.hu = 1.0;
    const auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 10.0};
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields);

    EXPECT_DOUBLE_EQ(diagnostics.cells[left_index].mass_residual, -diagnostics.cells[right_index].mass_residual);
}

TEST(SurfaceConservativeUpdate, AdvancesDepthFromInternalMassFlux) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto edge_index = first_internal_edge_index(mesh);
    const auto left_index = cell_index_by_id(mesh, *mesh.edges[edge_index].left_cell);
    const auto right_index = cell_index_by_id(mesh, *mesh.edges[edge_index].right_cell);

    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    state.cells[left_index].conserved.hu = 1.0;
    state.cells[right_index].conserved.hu = 1.0;
    const auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 10.0};
    scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields);

    EXPECT_NE(state.cells[left_index].conserved.h, 1.0);
    EXPECT_NE(state.cells[right_index].conserved.h, 1.0);
}
```

- [ ] **Step 2: Register the conservative update test**

Append to `tests/unit/surface2d/CMakeLists.txt`:

```cmake
add_executable(test_conservative_update test_conservative_update.cpp)
target_link_libraries(test_conservative_update
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_conservative_update COMMAND test_conservative_update)
```

- [ ] **Step 3: Run the new test and verify it fails**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_conservative_update
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R test_conservative_update
```

Expected: compile FAIL because `StepDiagnostics::cells` does not exist yet, or test FAIL because state is not advanced yet.

---

## Task 4: Implement Cell Residuals And Depth Update

**Files:**
- Modify: `libs/surface2d/include/surface2d/time_integration/step.hpp`
- Modify: `libs/surface2d/src/time_integration/step.cpp`

- [ ] **Step 1: Add cell diagnostics API**

In `libs/surface2d/include/surface2d/time_integration/step.hpp`, add:

```cpp
struct CellStepDiagnostics {
    core::Real mass_residual{0.0};
};
```

Add to `StepDiagnostics`:

```cpp
std::vector<CellStepDiagnostics> cells;
```

- [ ] **Step 2: Initialize cell diagnostics**

In `base_diagnostics`, include:

```cpp
.cells = std::vector<CellStepDiagnostics>(mesh.cells.size()),
```

- [ ] **Step 3: Accumulate internal-edge residuals**

In the DPM-aware overload loop, after computing each edge diagnostic, apply internal-edge mass flux only:

```cpp
const auto& edge = mesh.edges[edge_index];
const auto edge_diagnostics = edge_step_diagnostics(edge, state, dpm_fields, cell_indices, edge_index);
diagnostics.edges.push_back(edge_diagnostics);

if (edge.left_cell.has_value() && edge.right_cell.has_value()) {
    const std::size_t left_index = cell_indices.at(*edge.left_cell);
    const std::size_t right_index = cell_indices.at(*edge.right_cell);
    const core::Real integrated_flux = edge_diagnostics.mass_flux * edge.length;
    diagnostics.cells[left_index].mass_residual -= integrated_flux;
    diagnostics.cells[right_index].mass_residual += integrated_flux;
}
```

- [ ] **Step 4: Apply minimal finite-volume depth update**

After edge residual accumulation, update depth:

```cpp
for (std::size_t cell_index = 0; cell_index < mesh.cells.size(); ++cell_index) {
    const core::Real area = mesh::cell_area(mesh, mesh.cells[cell_index]);
    if (area > 0.0) {
        state.cells[cell_index].conserved.h += config.dt * diagnostics.cells[cell_index].mass_residual / area;
    }
}
```

- [ ] **Step 5: Run conservative update tests**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_conservative_update test_surface2d_golden_minimal
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "test_(conservative_update|surface2d_golden_minimal)$"
```

Expected: PASS, with M3 hydrostatic/blocked cases still preserving zero flux/update behavior.

---

## Task 5: Preserve M3 Boundaries And Full Validation

**Files:**
- Modify: `tests/unit/surface2d/test_step.cpp`
- Modify: `tests/unit/surface2d/test_surface2d_golden_minimal.cpp` if needed only to keep M3 expectations explicit.

- [ ] **Step 1: Add regression assertion for no-DPM overload cells**

In `tests/unit/surface2d/test_step.cpp`, ensure the no-DPM overload still reports no edge diagnostics and does not advance state:

```cpp
EXPECT_TRUE(diagnostics.edges.empty());
EXPECT_EQ(state.cells[0].conserved.h, 1.25);
EXPECT_EQ(state.cells[0].conserved.hu, 0.5);
EXPECT_EQ(state.cells[0].conserved.hv, -0.25);
```

- [ ] **Step 2: Run Surface2D subset**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "test_(surface_state|surface_step|dpm_fields|hydrostatic_reconstruction|hllc_flux|phi_t_source|surface2d_golden_minimal|cfl_rollback|conservative_update)$"
```

Expected: PASS, 9/9 Surface2D tests passed.

- [ ] **Step 3: Run full CTest**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure
```

Expected: PASS, all tests passed.

---

## Task 6: Record M4 Evidence

**Files:**
- Create: `superpowers/specs/2026-05-13-plan-m4-surface2d-cfl-rollback-conservation-evidence.md`

- [ ] **Step 1: Write evidence document**

Create `superpowers/specs/2026-05-13-plan-m4-surface2d-cfl-rollback-conservation-evidence.md`:

```markdown
# M4 Surface2D CFL Rollback Conservation Evidence

**Date:** 2026-05-13
**Plan:** `superpowers/plans/2026-05-13-plan-m4-surface2d-cfl-rollback-conservation.md`
**M3 Basis:** `superpowers/specs/2026-05-12-plan-m3-surface2d-numerical-minimal-slice-evidence.md`

## Scope

M4 adds raw physical CFL diagnostics, `C_rollback` rollback comparison, internal-edge conservative mass residuals, and a minimal CPU reference depth update for `Surface2DCore`.

## Local Validation

- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc`: PASS
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "test_(surface_state|surface_step|dpm_fields|hydrostatic_reconstruction|hllc_flux|phi_t_source|surface2d_golden_minimal|cfl_rollback|conservative_update)$"`: PASS
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure`: PASS

## Coverage

- `test_cfl_rollback`: verifies `max_cell_cfl` remains raw physical CFL and rollback compares against `C_rollback` only.
- `test_conservative_update`: verifies internal-edge mass residual antisymmetry and finite-volume depth update.
- Existing M3 Surface2D tests remain passing for DPM fields, hydrostatic reconstruction, HLLC degeneracies, `phi_t` source pairing, CPU step diagnostics, and G1/G2/G3 minimal coverage.

## Boundaries

- `Surface2DCore` remains independent of SWMM, D-Flow FM, `CouplingLib`, and 1D engine ABIs.
- `max_cell_cfl` remains the raw physical CFL diagnostic; it is not scaled by `CFL_safety`.
- Rollback uses `C_rollback` separately from `CFL_safety`.
- This evidence does not claim GoldenSuite release readiness.
```

- [ ] **Step 2: Inspect git diff**

Run:

```bash
git status --short
git diff --stat
```

Expected: only M4 files changed.

- [ ] **Step 3: Commit M4 plan only before implementation if requested**

Suggested commit message:

```text
docs(m4): plan cfl rollback conservation slice
```

Do not commit implementation changes until after tasks pass validation.

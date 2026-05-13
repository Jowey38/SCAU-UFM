# M5 Surface2D Wetting Boundary Implementation Plan

> **For agentic workers:** Execute this plan task-by-task. Preserve `Surface2DCore` independence from SWMM, D-Flow FM, `CouplingLib`, and 1D engine ABIs. Keep M5 limited to wetting/drying guards, boundary-edge flux behavior, and non-negative depth protection in the CPU reference path.

**Goal:** Add the first explicit wet/dry and boundary-edge behavior for `Surface2DCore` conservative CPU updates.

**Architecture:** M5 extends the M4 CPU reference step inside `libs/surface2d` only. The solver classifies very shallow cells with a configurable `h_min`, blocks dry-side flux contribution in the HLLC path, optionally applies boundary fluxes when a boundary state is available, and clamps finite-volume depth updates to non-negative depth. No coupling, 1D engine, external ABI, or GoldenSuite release-gate behavior is introduced.

**Tech Stack:** C++20, CMake, GoogleTest, existing `scau::core`, `scau::mesh`, and `scau::surface2d` targets.

---

## File Structure

- Modify `libs/surface2d/include/surface2d/time_integration/step.hpp`
  - Add `h_min` to `StepConfig` with a conservative default.
  - Keep existing overload signatures stable.
- Modify `libs/surface2d/src/time_integration/step.cpp`
  - Validate `h_min`.
  - Clamp depth updates to `>= 0.0`.
  - Keep internal-edge residual semantics from M4 unchanged.
- Modify `libs/surface2d/src/riemann/hllc.cpp`
  - Guard flux calculations against dry reconstructed depths.
- Modify `tests/unit/surface2d/test_step.cpp`
  - Add invalid `h_min` validation coverage.
- Create `tests/unit/surface2d/test_wetting_drying.cpp`
  - Focused tests for dry-cell flux blocking and non-negative depth update.
- Create `tests/unit/surface2d/test_boundary_flux.cpp`
  - Focused tests documenting current boundary-edge behavior.
- Modify `tests/unit/surface2d/CMakeLists.txt`
  - Register the two new test executables.
- Create `superpowers/specs/2026-05-13-plan-m5-surface2d-wetting-boundary-evidence.md`
  - Record build/test evidence after implementation.

---

## Task 1: Add Wetting/Drying Tests

**Files:**
- Create: `tests/unit/surface2d/test_wetting_drying.cpp`
- Modify: `tests/unit/surface2d/CMakeLists.txt`

- [ ] **Step 1: Create wetting/drying tests**

Create `tests/unit/surface2d/test_wetting_drying.cpp`:

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

void isolate_edge(scau::surface2d::DpmFields& dpm_fields, std::size_t edge_index) {
    for (auto& edge_fields : dpm_fields.edges) {
        edge_fields.phi_e_n = 0.0;
        edge_fields.omega_edge = 0.0;
    }
    dpm_fields.edges[edge_index].phi_e_n = 1.0;
    dpm_fields.edges[edge_index].omega_edge = 1.0;
}

}  // namespace

TEST(SurfaceWettingDrying, DryInternalCellDoesNotContributeMassFlux) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto edge_index = first_internal_edge_index(mesh);
    const auto left_index = cell_index_by_id(mesh, *mesh.edges[edge_index].left_cell);
    const auto right_index = cell_index_by_id(mesh, *mesh.edges[edge_index].right_cell);

    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    state.cells[left_index].conserved.h = 1.0e-8;
    state.cells[left_index].conserved.hu = 1.0;
    state.cells[right_index].conserved.hu = 1.0;
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    isolate_edge(dpm_fields, edge_index);

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 10.0, .h_min = 1.0e-6};
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields);

    EXPECT_EQ(diagnostics.edges[edge_index].mass_flux, 0.0);
    EXPECT_EQ(diagnostics.cells[left_index].mass_residual, 0.0);
    EXPECT_EQ(diagnostics.cells[right_index].mass_residual, 0.0);
}

TEST(SurfaceWettingDrying, DepthUpdateDoesNotMakeDepthNegative) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto edge_index = first_internal_edge_index(mesh);
    const auto left_index = cell_index_by_id(mesh, *mesh.edges[edge_index].left_cell);
    const auto right_index = cell_index_by_id(mesh, *mesh.edges[edge_index].right_cell);

    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    state.cells[left_index].conserved.h = 1.0e-4;
    state.cells[left_index].conserved.hu = 10.0;
    state.cells[right_index].conserved.hu = 10.0;
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    isolate_edge(dpm_fields, edge_index);

    const scau::surface2d::StepConfig config{.dt = 1.0, .cfl_safety = 0.45, .c_rollback = 100000.0, .h_min = 1.0e-8};
    static_cast<void>(scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields));

    EXPECT_GE(state.cells[left_index].conserved.h, 0.0);
    EXPECT_GE(state.cells[right_index].conserved.h, 0.0);
}
```

- [ ] **Step 2: Register wetting/drying test**

Append to `tests/unit/surface2d/CMakeLists.txt`:

```cmake
add_executable(test_wetting_drying test_wetting_drying.cpp)
target_link_libraries(test_wetting_drying
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_wetting_drying COMMAND test_wetting_drying)
```

- [ ] **Step 3: Run the new test and verify it fails**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_wetting_drying
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R test_wetting_drying
```

Expected: compile FAIL because `StepConfig::h_min` does not exist yet, or test FAIL because dry-cell and non-negative-depth guards are not implemented yet.

---

## Task 2: Implement Wet/Dry Configuration And Depth Clamp

**Files:**
- Modify: `libs/surface2d/include/surface2d/time_integration/step.hpp`
- Modify: `libs/surface2d/src/time_integration/step.cpp`
- Modify: `tests/unit/surface2d/test_step.cpp`

- [ ] **Step 1: Add `h_min` to step config**

In `libs/surface2d/include/surface2d/time_integration/step.hpp`, update `StepConfig`:

```cpp
struct StepConfig {
    core::Real dt{0.0};
    core::Real cfl_safety{0.45};
    core::Real c_rollback{1.0};
    core::Real h_min{1.0e-8};
};
```

- [ ] **Step 2: Validate `h_min`**

In `validate_step_inputs`, add:

```cpp
if (config.h_min < 0.0) {
    throw std::invalid_argument("h_min must be non-negative");
}
```

- [ ] **Step 3: Clamp depth update**

In `apply_depth_update`, replace the assignment with:

```cpp
state.cells[cell_index].conserved.h = std::max(
    0.0,
    state.cells[cell_index].conserved.h + config.dt * cells[cell_index].mass_residual / area);
```

- [ ] **Step 4: Add invalid `h_min` regression test**

In `tests/unit/surface2d/test_step.cpp`, add to `RejectsInvalidStepInputs`:

```cpp
EXPECT_THROW(
    static_cast<void>(scau::surface2d::advance_one_step_cpu(
        mesh,
        state,
        scau::surface2d::StepConfig{.dt = 0.5, .cfl_safety = 0.45, .c_rollback = 1.0, .h_min = -1.0})),
    std::invalid_argument);
```

Use a fresh valid state before this assertion if the existing test mutates `state` earlier.

---

## Task 3: Implement Dry Flux Guard

**Files:**
- Modify: `libs/surface2d/include/surface2d/riemann/hllc.hpp`
- Modify: `libs/surface2d/src/riemann/hllc.cpp`
- Modify: `libs/surface2d/src/time_integration/step.cpp`

- [ ] **Step 1: Add `h_min` parameter to HLLC flux**

Update `hllc_normal_flux` declaration and definition to accept `core::Real h_min` with default `1.0e-8` in the header:

```cpp
[[nodiscard]] EdgeFlux hllc_normal_flux(
    const CellState& left,
    const CellState& right,
    const EdgeDpmFields& edge_fields,
    Normal2 normal,
    core::Real h_min = 1.0e-8);
```

- [ ] **Step 2: Block dry reconstructed states**

In `hllc_normal_flux`, after reconstruction:

```cpp
if (pair.left.conserved.h <= h_min || pair.right.conserved.h <= h_min) {
    return EdgeFlux{};
}
```

- [ ] **Step 3: Pass config threshold from step**

In `edge_step_diagnostics`, add a `core::Real h_min` parameter and pass it into `hllc_normal_flux`.

In the DPM-aware step loop, call:

```cpp
const auto edge_diagnostics = edge_step_diagnostics(edge, state, dpm_fields, cell_indices, edge_index, config.h_min);
```

- [ ] **Step 4: Run wetting/drying tests**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_wetting_drying test_hllc_flux test_surface_step
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "test_(wetting_drying|hllc_flux|surface_step)$"
```

Expected: PASS.

---

## Task 4: Document Boundary-Edge Behavior

**Files:**
- Create: `tests/unit/surface2d/test_boundary_flux.cpp`
- Modify: `tests/unit/surface2d/CMakeLists.txt`

- [ ] **Step 1: Create boundary behavior tests**

Create `tests/unit/surface2d/test_boundary_flux.cpp`:

```cpp
#include <cstddef>
#include <stdexcept>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/state/state.hpp"
#include "surface2d/time_integration/step.hpp"

namespace {

std::size_t first_boundary_edge_index(const scau::mesh::Mesh& mesh) {
    for (std::size_t index = 0; index < mesh.edges.size(); ++index) {
        const bool has_left = mesh.edges[index].left_cell.has_value();
        const bool has_right = mesh.edges[index].right_cell.has_value();
        if (has_left != has_right) {
            return index;
        }
    }
    throw std::invalid_argument("mesh must contain a boundary edge");
}

std::size_t edge_cell_index(const scau::mesh::Mesh& mesh, std::size_t edge_index) {
    const auto& edge = mesh.edges[edge_index];
    const auto& cell_id = edge.left_cell.has_value() ? *edge.left_cell : *edge.right_cell;
    for (std::size_t index = 0; index < mesh.cells.size(); ++index) {
        if (mesh.cells[index].id == cell_id) {
            return index;
        }
    }
    throw std::invalid_argument("mesh must contain boundary edge cell");
}

}  // namespace

TEST(SurfaceBoundaryFlux, BoundaryEdgesReportFluxDiagnosticsWithoutCellResidualUpdate) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto edge_index = first_boundary_edge_index(mesh);
    const auto cell_index = edge_cell_index(mesh, edge_index);

    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    state.cells[cell_index].conserved.hu = 1.0;
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 10.0};
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields);

    ASSERT_EQ(diagnostics.edges.size(), mesh.edges.size());
    EXPECT_EQ(diagnostics.cells[cell_index].mass_residual, 0.0);
}
```

- [ ] **Step 2: Register boundary behavior test**

Append to `tests/unit/surface2d/CMakeLists.txt`:

```cmake
add_executable(test_boundary_flux test_boundary_flux.cpp)
target_link_libraries(test_boundary_flux
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_boundary_flux COMMAND test_boundary_flux)
```

- [ ] **Step 3: Run boundary behavior tests**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_boundary_flux
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R test_boundary_flux
```

Expected: PASS. M5 documents that boundary-edge diagnostics can be reported, but boundary-edge residual updates remain out of scope until explicit boundary state semantics are added.

---

## Task 5: Preserve Surface2D Validation

**Files:**
- Modify: `tests/unit/surface2d/CMakeLists.txt`
- Modify tests only if needed to preserve existing semantics.

- [ ] **Step 1: Run Surface2D subset**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "test_(surface_state|surface_step|dpm_fields|hydrostatic_reconstruction|hllc_flux|phi_t_source|surface2d_golden_minimal|cfl_rollback|conservative_update|wetting_drying|boundary_flux)$"
```

Expected: PASS, 11/11 Surface2D tests passed.

- [ ] **Step 2: Run full CTest**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure
```

Expected: PASS, all tests passed.

---

## Task 6: Record M5 Evidence

**Files:**
- Create: `superpowers/specs/2026-05-13-plan-m5-surface2d-wetting-boundary-evidence.md`

- [ ] **Step 1: Write evidence document**

Create `superpowers/specs/2026-05-13-plan-m5-surface2d-wetting-boundary-evidence.md`:

```markdown
# M5 Surface2D Wetting Boundary Evidence

**Date:** 2026-05-13
**Plan:** `superpowers/plans/2026-05-13-plan-m5-surface2d-wetting-boundary.md`
**M4 Basis:** `superpowers/specs/2026-05-13-plan-m4-surface2d-cfl-rollback-conservation-evidence.md`

## Scope

M5 adds configurable wet/dry thresholding, dry-side HLLC flux blocking, non-negative finite-volume depth updates, and explicit boundary-edge behavior coverage for `Surface2DCore`.

## Local Validation

- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc`: PASS
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "test_(surface_state|surface_step|dpm_fields|hydrostatic_reconstruction|hllc_flux|phi_t_source|surface2d_golden_minimal|cfl_rollback|conservative_update|wetting_drying|boundary_flux)$"`: PASS
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure`: PASS

## Coverage

- `test_wetting_drying`: verifies dry-cell flux blocking and non-negative depth update.
- `test_boundary_flux`: documents current boundary-edge diagnostic behavior and confirms boundary residual updates remain out of scope.
- Existing M4/M3 Surface2D tests remain passing.

## Boundaries

- `Surface2DCore` remains independent of SWMM, D-Flow FM, `CouplingLib`, and 1D engine ABIs.
- Boundary-edge residual update semantics remain out of scope until explicit boundary state semantics are designed.
- This evidence does not claim GoldenSuite release readiness.
```

- [ ] **Step 2: Inspect git diff**

Run:

```bash
git status --short
git diff --stat
```

Expected: only M5 files changed.

- [ ] **Step 3: Commit M5 implementation after validation**

Suggested commit message:

```text
feat(surface2d): add wetting boundary guards
```

# M6 Surface2D Boundary Conditions Implementation Plan

> **For agentic workers:** Execute this plan task-by-task. Preserve `Surface2DCore` independence from SWMM, D-Flow FM, `CouplingLib`, and 1D engine ABIs. Keep M6 limited to explicit Wall/Open boundary semantics on boundary edges in the CPU reference path; do not introduce prescribed (h, u_n) boundaries, momentum flux completion, or GoldenSuite release-gate behavior.

**Goal:** Add explicit Wall and Open boundary-edge semantics to the `Surface2DCore` CPU reference step so boundary edges can lawfully contribute to mass residuals.

**Architecture:** M6 extends the M5 CPU reference step inside `libs/surface2d` only. A new `surface2d::BoundaryConditions` container (shaped like `DpmFields`) carries a per-edge `BoundaryKind` enum (`Wall` or `Open`). The DPM-aware step gains a fourth overload that accepts `BoundaryConditions`. Wall edges keep the current M5 behavior (zero mass flux, no residual contribution). Open edges copy the inside cell state into an outside ghost state, run the existing HLLC flux, and accumulate the integrated mass flux into the inside cell's residual. The default kind is `Wall`, so the previous overloads remain semantically equivalent to M5.

**Tech Stack:** C++20, CMake, GoogleTest, existing `scau::core`, `scau::mesh`, and `scau::surface2d` targets.

---

## File Structure

- Create `libs/surface2d/include/surface2d/boundary/conditions.hpp`
  - Declare `BoundaryKind` enum and `BoundaryConditions` container with `for_mesh` factory and edge-count validation.
- Create `libs/surface2d/src/boundary/conditions.cpp`
  - Implement `for_mesh` and `validate_boundary_conditions_match_mesh`.
- Modify `libs/surface2d/CMakeLists.txt`
  - Add the new boundary translation unit to `scau_surface2d`.
- Modify `libs/surface2d/include/surface2d/time_integration/step.hpp`
  - Add a new four-argument `advance_one_step_cpu` overload that accepts `const BoundaryConditions&`.
- Modify `libs/surface2d/src/time_integration/step.cpp`
  - Implement boundary-edge handling: Wall edges keep zero mass flux; Open edges form a ghost outside state mirroring the inside cell state, run HLLC, and accumulate the integrated flux into the inside cell residual.
- Create `tests/unit/surface2d/test_boundary_conditions.cpp`
  - Cover Wall behavior (no residual contribution), Open behavior (residual reflects outflow), and `BoundaryConditions` validation.
- Modify `tests/unit/surface2d/test_boundary_flux.cpp`
  - Retain the M5 default-Wall documentation test and add a second assertion that confirms the new default Wall overload also yields zero residual on the boundary cell.
- Modify `tests/unit/surface2d/CMakeLists.txt`
  - Register `test_boundary_conditions`.
- Create `superpowers/specs/2026-05-14-plan-m6-surface2d-boundary-conditions-evidence.md`
  - Record build/test evidence after implementation.

---

## Task 1: Add BoundaryConditions Container

**Files:**
- Create: `libs/surface2d/include/surface2d/boundary/conditions.hpp`
- Create: `libs/surface2d/src/boundary/conditions.cpp`
- Modify: `libs/surface2d/CMakeLists.txt`

- [ ] **Step 1: Declare `BoundaryKind` and `BoundaryConditions`**

Create `libs/surface2d/include/surface2d/boundary/conditions.hpp`:

```cpp
#pragma once

#include <cstddef>
#include <vector>

#include "mesh/mesh.hpp"

namespace scau::surface2d {

enum class BoundaryKind {
    Wall,
    Open,
};

struct BoundaryConditions {
    std::vector<BoundaryKind> edges;

    [[nodiscard]] static BoundaryConditions for_mesh(const mesh::Mesh& mesh);
};

void validate_boundary_conditions_match_mesh(const BoundaryConditions& boundary, const mesh::Mesh& mesh);

}  // namespace scau::surface2d
```

- [ ] **Step 2: Implement `for_mesh` and validation**

Create `libs/surface2d/src/boundary/conditions.cpp`:

```cpp
#include "surface2d/boundary/conditions.hpp"

#include <stdexcept>

namespace scau::surface2d {

BoundaryConditions BoundaryConditions::for_mesh(const mesh::Mesh& mesh) {
    return BoundaryConditions{
        .edges = std::vector<BoundaryKind>(mesh.edges.size(), BoundaryKind::Wall),
    };
}

void validate_boundary_conditions_match_mesh(const BoundaryConditions& boundary, const mesh::Mesh& mesh) {
    if (boundary.edges.size() != mesh.edges.size()) {
        throw std::invalid_argument("boundary edge count must match mesh edge count");
    }
}

}  // namespace scau::surface2d
```

- [ ] **Step 3: Register new translation unit**

In `libs/surface2d/CMakeLists.txt`, append the new source to the `scau_surface2d` source list next to `src/dpm/fields.cpp`:

```cmake
src/boundary/conditions.cpp
```

Keep the existing target/include layout unchanged.

- [ ] **Step 4: Compile-only smoke check**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target scau_surface2d
```

Expected: PASS.

---

## Task 2: Add Boundary-Aware Step Tests

**Files:**
- Create: `tests/unit/surface2d/test_boundary_conditions.cpp`
- Modify: `tests/unit/surface2d/CMakeLists.txt`

- [ ] **Step 1: Create boundary-aware tests**

Create `tests/unit/surface2d/test_boundary_conditions.cpp`:

```cpp
#include <cstddef>
#include <stdexcept>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/boundary/conditions.hpp"
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

void isolate_edge(scau::surface2d::DpmFields& dpm_fields, std::size_t edge_index) {
    for (auto& edge_fields : dpm_fields.edges) {
        edge_fields.phi_e_n = 0.0;
        edge_fields.omega_edge = 0.0;
    }
    dpm_fields.edges[edge_index].phi_e_n = 1.0;
    dpm_fields.edges[edge_index].omega_edge = 1.0;
}

}  // namespace

TEST(SurfaceBoundaryConditions, WallBoundaryProducesNoResidual) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto edge_index = first_boundary_edge_index(mesh);
    const auto cell_index = edge_cell_index(mesh, edge_index);

    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    state.cells[cell_index].conserved.hu = 1.0;
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    isolate_edge(dpm_fields, edge_index);
    auto boundary = scau::surface2d::BoundaryConditions::for_mesh(mesh);
    boundary.edges[edge_index] = scau::surface2d::BoundaryKind::Wall;

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 10.0};
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields, boundary);

    EXPECT_EQ(diagnostics.edges[edge_index].mass_flux, 0.0);
    EXPECT_EQ(diagnostics.cells[cell_index].mass_residual, 0.0);
}

TEST(SurfaceBoundaryConditions, OpenBoundaryAccumulatesResidualOnInsideCell) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto edge_index = first_boundary_edge_index(mesh);
    const auto cell_index = edge_cell_index(mesh, edge_index);

    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    state.cells[cell_index].conserved.hu = 1.0;
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    isolate_edge(dpm_fields, edge_index);
    auto boundary = scau::surface2d::BoundaryConditions::for_mesh(mesh);
    boundary.edges[edge_index] = scau::surface2d::BoundaryKind::Open;

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 10.0};
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields, boundary);

    EXPECT_NE(diagnostics.edges[edge_index].mass_flux, 0.0);
    EXPECT_NE(diagnostics.cells[cell_index].mass_residual, 0.0);
}

TEST(SurfaceBoundaryConditions, RejectsMismatchedBoundaryConditions) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    auto boundary = scau::surface2d::BoundaryConditions::for_mesh(mesh);
    boundary.edges.pop_back();

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 10.0};
    EXPECT_THROW(
        static_cast<void>(scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields, boundary)),
        std::invalid_argument);
}
```

- [ ] **Step 2: Register boundary-aware test**

Append to `tests/unit/surface2d/CMakeLists.txt`:

```cmake
add_executable(test_boundary_conditions test_boundary_conditions.cpp)
target_link_libraries(test_boundary_conditions
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_boundary_conditions COMMAND test_boundary_conditions)
```

- [ ] **Step 3: Run the new test and verify it fails**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_boundary_conditions
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R test_boundary_conditions
```

Expected: compile FAIL because the four-argument boundary overload of `advance_one_step_cpu` does not exist yet.

---

## Task 3: Implement Boundary-Aware Step Overload

**Files:**
- Modify: `libs/surface2d/include/surface2d/time_integration/step.hpp`
- Modify: `libs/surface2d/src/time_integration/step.cpp`

- [ ] **Step 1: Declare new overload**

In `libs/surface2d/include/surface2d/time_integration/step.hpp`, add the include and the new overload below the existing DPM-aware overload:

```cpp
#include "surface2d/boundary/conditions.hpp"
```

```cpp
[[nodiscard]] StepDiagnostics advance_one_step_cpu(
    const mesh::Mesh& mesh,
    SurfaceState& state,
    const StepConfig& config,
    const DpmFields& dpm_fields,
    const BoundaryConditions& boundary);
```

- [ ] **Step 2: Implement boundary-aware step**

In `libs/surface2d/src/time_integration/step.cpp`:

Replace the body of the existing three-argument DPM overload so it forwards to the new four-argument overload with a default Wall `BoundaryConditions`:

```cpp
StepDiagnostics advance_one_step_cpu(
    const mesh::Mesh& mesh,
    SurfaceState& state,
    const StepConfig& config,
    const DpmFields& dpm_fields) {
    return advance_one_step_cpu(mesh, state, config, dpm_fields, BoundaryConditions::for_mesh(mesh));
}
```

Add a new helper for boundary-edge handling above the new overload:

```cpp
CellState open_boundary_outside_state(const CellState& inside) {
    return CellState{
        .conserved = {.h = inside.conserved.h, .hu = inside.conserved.hu, .hv = inside.conserved.hv},
        .eta = inside.eta,
    };
}
```

Implement the new overload alongside the existing DPM-aware overload:

```cpp
StepDiagnostics advance_one_step_cpu(
    const mesh::Mesh& mesh,
    SurfaceState& state,
    const StepConfig& config,
    const DpmFields& dpm_fields,
    const BoundaryConditions& boundary) {
    validate_step_inputs(mesh, state, config);
    validate_dpm_fields_match_mesh(dpm_fields, mesh);
    validate_boundary_conditions_match_mesh(boundary, mesh);

    auto diagnostics = base_diagnostics(mesh, state, config);
    diagnostics.edges.reserve(mesh.edges.size());
    const auto cell_indices = cell_indices_by_id(mesh);
    for (std::size_t edge_index = 0; edge_index < mesh.edges.size(); ++edge_index) {
        const auto& edge = mesh.edges[edge_index];
        const bool is_internal = edge.left_cell.has_value() && edge.right_cell.has_value();

        if (is_internal) {
            const auto edge_diagnostics = edge_step_diagnostics(edge, state, dpm_fields, cell_indices, edge_index, config.h_min);
            diagnostics.edges.push_back(edge_diagnostics);
            const std::size_t left_index = cell_indices.at(*edge.left_cell);
            const std::size_t right_index = cell_indices.at(*edge.right_cell);
            const core::Real integrated_flux = edge_diagnostics.mass_flux * edge.length;
            diagnostics.cells[left_index].mass_residual -= integrated_flux;
            diagnostics.cells[right_index].mass_residual += integrated_flux;
            continue;
        }

        if (boundary.edges[edge_index] == BoundaryKind::Wall) {
            diagnostics.edges.push_back(EdgeStepDiagnostics{});
            continue;
        }

        const std::size_t inside_index = edge_cell_index(cell_indices, edge.left_cell, edge.right_cell);
        const auto& inside = state.cells[inside_index];
        const auto outside = open_boundary_outside_state(inside);
        const auto flux = hllc_normal_flux(
            inside,
            outside,
            dpm_fields.edges[edge_index],
            Normal2{.x = edge.normal.x, .y = edge.normal.y},
            config.h_min);

        EdgeStepDiagnostics edge_diagnostics{
            .mass_flux = flux.mass,
            .momentum_flux_n = flux.momentum_n,
        };
        diagnostics.edges.push_back(edge_diagnostics);

        const core::Real integrated_flux = edge_diagnostics.mass_flux * edge.length;
        if (edge.left_cell.has_value()) {
            diagnostics.cells[inside_index].mass_residual -= integrated_flux;
        } else {
            diagnostics.cells[inside_index].mass_residual += integrated_flux;
        }
    }
    apply_depth_update(mesh, state, config, diagnostics.cells);
    return diagnostics;
}
```

Keep the existing no-DPM and DPM-aware overloads unchanged besides the forwarding body above.

- [ ] **Step 3: Run boundary-aware tests**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_boundary_conditions
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R test_boundary_conditions
```

Expected: PASS.

---

## Task 4: Preserve M5 Boundary Documentation Test

**Files:**
- Modify: `tests/unit/surface2d/test_boundary_flux.cpp`

- [ ] **Step 1: Append default-Wall regression**

In `tests/unit/surface2d/test_boundary_flux.cpp`, add a second test below the existing one that exercises the new four-argument overload with a default Wall `BoundaryConditions`:

```cpp
#include "surface2d/boundary/conditions.hpp"
```

```cpp
TEST(SurfaceBoundaryFlux, DefaultWallBoundaryConditionsMatchM5DocumentedBehavior) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto edge_index = first_boundary_edge_index(mesh);
    const auto cell_index = edge_cell_index(mesh, edge_index);

    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    state.cells[cell_index].conserved.hu = 1.0;
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    isolate_edge(dpm_fields, edge_index);
    const auto boundary = scau::surface2d::BoundaryConditions::for_mesh(mesh);

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 10.0};
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields, boundary);

    ASSERT_EQ(diagnostics.edges.size(), mesh.edges.size());
    EXPECT_EQ(diagnostics.edges[edge_index].mass_flux, 0.0);
    EXPECT_EQ(diagnostics.cells[cell_index].mass_residual, 0.0);
}
```

The existing M5 test (`BoundaryEdgesReportFluxDiagnosticsWithoutCellResidualUpdate`) must remain unchanged so the implicit-Wall documentation contract is preserved.

- [ ] **Step 2: Run boundary documentation tests**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_boundary_flux
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R test_boundary_flux
```

Expected: PASS, two tests passed.

---

## Task 5: Preserve Surface2D Validation

**Files:**
- Modify tests only if needed to preserve existing semantics.

- [ ] **Step 1: Run Surface2D subset**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "test_(surface_state|surface_step|dpm_fields|hydrostatic_reconstruction|hllc_flux|phi_t_source|surface2d_golden_minimal|cfl_rollback|conservative_update|wetting_drying|boundary_flux|boundary_conditions)$"
```

Expected: PASS, 12/12 Surface2D tests passed.

- [ ] **Step 2: Run full CTest**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure
```

Expected: PASS, all tests passed.

---

## Task 6: Record M6 Evidence

**Files:**
- Create: `superpowers/specs/2026-05-14-plan-m6-surface2d-boundary-conditions-evidence.md`

- [ ] **Step 1: Write evidence document**

Create `superpowers/specs/2026-05-14-plan-m6-surface2d-boundary-conditions-evidence.md`:

```markdown
# M6 Surface2D Boundary Conditions Evidence

**Date:** 2026-05-14
**Plan:** `superpowers/plans/2026-05-14-plan-m6-surface2d-boundary-conditions.md`
**M5 Basis:** `superpowers/specs/2026-05-13-plan-m5-surface2d-wetting-boundary-evidence.md`

## Scope

M6 adds explicit Wall and Open boundary-edge semantics for `Surface2DCore` through a new `BoundaryConditions` container and a four-argument `advance_one_step_cpu` overload. Default kind is Wall; the existing three-argument DPM overload forwards to the new overload with a default Wall container.

## Local Validation

- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc`: PASS
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "test_(surface_state|surface_step|dpm_fields|hydrostatic_reconstruction|hllc_flux|phi_t_source|surface2d_golden_minimal|cfl_rollback|conservative_update|wetting_drying|boundary_flux|boundary_conditions)$"`: PASS
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure`: PASS

## Coverage

- `test_boundary_conditions`: verifies Wall produces no residual, Open accumulates inside-cell residual, and mismatched boundary sizes are rejected.
- `test_boundary_flux`: retains the M5 default-Wall documentation contract and adds a default-Wall regression against the new overload.
- Existing M3/M4/M5 Surface2D tests remain passing.

## Boundaries

- `Surface2DCore` remains independent of SWMM, D-Flow FM, `CouplingLib`, and 1D engine ABIs.
- Prescribed (h, u_n) boundaries and momentum-flux completion remain out of scope.
- This evidence does not claim GoldenSuite release readiness.
```

- [ ] **Step 2: Inspect git diff**

Run:

```bash
git status --short
git diff --stat
```

Expected: only M6 files changed.

- [ ] **Step 3: Commit M6 implementation after validation**

Suggested commit message:

```text
feat(surface2d): add explicit wall and open boundaries
```

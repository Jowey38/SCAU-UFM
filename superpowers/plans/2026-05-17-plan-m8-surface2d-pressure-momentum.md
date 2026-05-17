# M8 Surface2D Pressure-Normal Momentum Implementation Plan

> **For agentic workers:** Execute this plan task-by-task. Preserve `Surface2DCore` independence from SWMM, D-Flow FM, `CouplingLib`, and 1D engine ABIs. Keep M8 limited to pressure-normal momentum contribution in the CPU reference path. Do NOT change the M7 upwind mass-carried momentum transport, and do NOT introduce prescribed boundary states or full HLLC wave-speed solving in this slice.

**Goal:** Add pressure-normal momentum contribution to `Surface2DCore` so edge diagnostics and per-cell momentum residuals include the hydrostatic pressure term alongside the existing M7 transport term.

**Architecture:** M8 extends the M7 CPU reference step inside `libs/surface2d` only. The existing `EdgeStepDiagnostics::momentum_flux_n` field becomes meaningful: for each edge, compute a scalar pressure-normal contribution from the reconstructed hydrostatic pair using the local depth and gravity, then project it onto the edge normal and accumulate it into `MomentumResidual{x, y}`. The M7 upwind mass-carried transport remains intact and continues to be applied in parallel. Wall edges continue to contribute nothing. Open edges continue to use the boundary ghost-state path introduced in M6, but now also contribute pressure-normal momentum when a valid flux state exists.

**Tech Stack:** C++20, CMake, GoogleTest, existing `scau::core`, `scau::mesh`, and `scau::surface2d` targets.

---

## File Structure

- Modify `libs/surface2d/include/surface2d/riemann/hllc.hpp`
  - Keep the existing HLLC mass flux signature stable.
  - Add or clarify a pressure-normal helper declaration if needed for the new edge contribution.
- Modify `libs/surface2d/src/riemann/hllc.cpp`
  - Compute a scalar pressure-normal momentum contribution from the reconstructed hydrostatic pair.
  - Leave the existing mass flux path unchanged.
- Modify `libs/surface2d/src/time_integration/step.cpp`
  - Accumulate pressure-normal momentum residuals for internal and Open edges.
  - Keep the M7 upwind transport accumulation intact.
- Create `tests/unit/surface2d/test_pressure_momentum.cpp`
  - Cover internal-edge pressure residual antisymmetry, dry-side blocking, Wall zero contribution, Open boundary contribution, and regression with M7 transport.
- Modify `tests/unit/surface2d/CMakeLists.txt`
  - Register the new test executable.
- Create `superpowers/specs/2026-05-17-plan-m8-surface2d-pressure-momentum-evidence.md`
  - Record build/test evidence after implementation.

---

## Task 1: Add Pressure-Momentum Tests

**Files:**
- Create: `tests/unit/surface2d/test_pressure_momentum.cpp`
- Modify: `tests/unit/surface2d/CMakeLists.txt`

- [ ] **Step 1: Create pressure-momentum tests**

Create `tests/unit/surface2d/test_pressure_momentum.cpp`:

```cpp
#include <cstddef>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/boundary/conditions.hpp"
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

TEST(SurfacePressureMomentum, InternalEdgePressureResidualIsAntisymmetric) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto edge_index = first_internal_edge_index(mesh);
    const auto left_index = cell_index_by_id(mesh, *mesh.edges[edge_index].left_cell);
    const auto right_index = cell_index_by_id(mesh, *mesh.edges[edge_index].right_cell);

    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    isolate_edge(dpm_fields, edge_index);

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 10.0};
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields);

    EXPECT_DOUBLE_EQ(
        diagnostics.cells[left_index].momentum_residual.x,
        -diagnostics.cells[right_index].momentum_residual.x);
    EXPECT_DOUBLE_EQ(
        diagnostics.cells[left_index].momentum_residual.y,
        -diagnostics.cells[right_index].momentum_residual.y);
}

TEST(SurfacePressureMomentum, DrySideEdgeContributesNoPressureResidual) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto edge_index = first_internal_edge_index(mesh);
    const auto left_index = cell_index_by_id(mesh, *mesh.edges[edge_index].left_cell);
    const auto right_index = cell_index_by_id(mesh, *mesh.edges[edge_index].right_cell);

    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    state.cells[left_index].conserved.h = 1.0e-8;
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    isolate_edge(dpm_fields, edge_index);

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 10.0, .h_min = 1.0e-6};
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields);

    EXPECT_EQ(diagnostics.cells[left_index].momentum_residual.x, 0.0);
    EXPECT_EQ(diagnostics.cells[left_index].momentum_residual.y, 0.0);
    EXPECT_EQ(diagnostics.cells[right_index].momentum_residual.x, 0.0);
    EXPECT_EQ(diagnostics.cells[right_index].momentum_residual.y, 0.0);
}

TEST(SurfacePressureMomentum, WallBoundaryContributesNoPressureResidual) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto edge_index = first_boundary_edge_index(mesh);
    const auto cell_index = edge_cell_index(mesh, edge_index);

    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    isolate_edge(dpm_fields, edge_index);
    auto boundary = scau::surface2d::BoundaryConditions::for_mesh(mesh);
    boundary.edges[edge_index] = scau::surface2d::BoundaryKind::Wall;

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 10.0};
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields, boundary);

    EXPECT_EQ(diagnostics.cells[cell_index].momentum_residual.x, 0.0);
    EXPECT_EQ(diagnostics.cells[cell_index].momentum_residual.y, 0.0);
}

TEST(SurfacePressureMomentum, OpenBoundaryContributesPressureResidual) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto edge_index = first_boundary_edge_index(mesh);
    const auto cell_index = edge_cell_index(mesh, edge_index);

    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    state.cells[cell_index].conserved.hu = 1.0;
    state.cells[cell_index].conserved.hv = 1.0;
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    isolate_edge(dpm_fields, edge_index);
    auto boundary = scau::surface2d::BoundaryConditions::for_mesh(mesh);
    boundary.edges[edge_index] = scau::surface2d::BoundaryKind::Open;

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 10.0};
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields, boundary);

    EXPECT_NE(diagnostics.cells[cell_index].momentum_residual.x, 0.0);
    EXPECT_NE(diagnostics.cells[cell_index].momentum_residual.y, 0.0);
}

TEST(SurfacePressureMomentum, MomentumTransportRegressionStillPasses) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto edge_index = first_internal_edge_index(mesh);
    const auto left_index = cell_index_by_id(mesh, *mesh.edges[edge_index].left_cell);
    const auto right_index = cell_index_by_id(mesh, *mesh.edges[edge_index].right_cell);

    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    state.cells[left_index].conserved.hu = 1.0;
    state.cells[left_index].conserved.hv = 0.5;
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    isolate_edge(dpm_fields, edge_index);

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 10.0};
    static_cast<void>(scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields));

    EXPECT_NE(state.cells[left_index].conserved.hu, 1.0);
    EXPECT_NE(state.cells[right_index].conserved.hu, 0.0);
}
```

- [ ] **Step 2: Register pressure-momentum test**

Append to `tests/unit/surface2d/CMakeLists.txt`:

```cmake
add_executable(test_pressure_momentum test_pressure_momentum.cpp)
target_link_libraries(test_pressure_momentum
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_pressure_momentum COMMAND test_pressure_momentum)
```

- [ ] **Step 3: Run the new test and verify it fails**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_pressure_momentum
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R test_pressure_momentum
```

Expected: compile FAIL because pressure-normal momentum accumulation does not exist yet.

---

## Task 2: Add Pressure-Momentum Diagnostics Hook

**Files:**
- Modify: `libs/surface2d/include/surface2d/time_integration/step.hpp`
- Modify: `libs/surface2d/src/riemann/hllc.cpp` if a helper declaration is needed.

- [ ] **Step 1: Reuse existing `momentum_flux_n` field**

Keep `EdgeStepDiagnostics::momentum_flux_n` as the pressure-normal contribution carrier. Do not add another edge-side momentum field. The tests and step integration should treat this field as the scalar normal pressure momentum component for M8.

- [ ] **Step 2: Compile-only smoke check**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target scau_surface2d
```

Expected: PASS.

---

## Task 3: Implement Pressure-Normal Momentum Accumulation

**Files:**
- Modify: `libs/surface2d/src/riemann/hllc.cpp`
- Modify: `libs/surface2d/src/time_integration/step.cpp`

- [ ] **Step 1: Add pressure-normal helper**

In `libs/surface2d/src/riemann/hllc.cpp`, add a small helper that computes a scalar pressure-normal contribution from the reconstructed hydrostatic pair. Use the same reconstructed depths that the mass flux already uses; keep the mass flux formula unchanged.

Suggested shape:

```cpp
core::Real pressure_normal_momentum(const CellState& left, const CellState& right, core::Real gravity = 9.81) {
    const auto pair = reconstruct_hydrostatic_pair(left, right);
    return 0.25 * gravity * (pair.left.conserved.h * pair.left.conserved.h + pair.right.conserved.h * pair.right.conserved.h);
}
```

This slice may choose a slightly different symmetric average if required by the implementation, but it must be a scalar normal contribution, not an x/y vector.

- [ ] **Step 2: Populate `momentum_flux_n` for internal edges**

In `edge_step_diagnostics(...)`, replace the existing zero assignment with the new pressure helper result:

```cpp
EdgeStepDiagnostics diagnostics{
    .mass_flux = flux.mass,
    .momentum_flux_n = pressure_normal_momentum(left, right),
};
```

- [ ] **Step 3: Populate `momentum_flux_n` for Open boundaries**

In the Open boundary branch of `advance_one_step_cpu`, compute the same scalar pressure-normal contribution using `inside` and the Open ghost state, and store it in `edge_diagnostics.momentum_flux_n`.

- [ ] **Step 4: Accumulate pressure-normal momentum into cells**

For internal edges, after mass residual accumulation and M7 upwind momentum transport, add pressure-normal momentum residual contribution projected onto the edge normal:

```cpp
const core::Real pressure_n = edge_diagnostics.momentum_flux_n * edge.length;
diagnostics.cells[left_index].momentum_residual.x -= pressure_n * edge.normal.x;
diagnostics.cells[left_index].momentum_residual.y -= pressure_n * edge.normal.y;
diagnostics.cells[right_index].momentum_residual.x += pressure_n * edge.normal.x;
diagnostics.cells[right_index].momentum_residual.y += pressure_n * edge.normal.y;
```

For Open boundaries, apply the same projection to the inside cell with the same signed convention used for mass residuals. Wall boundaries continue to contribute nothing.

- [ ] **Step 5: Preserve M7 transport and dry clamp**

Do not modify the M7 upwind transport helpers or `apply_momentum_update` ordering. The M8 pressure contribution is additive and must not replace the M7 advective momentum term.

- [ ] **Step 6: Run pressure-momentum tests**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_pressure_momentum
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R test_pressure_momentum
```

Expected: PASS.

---

## Task 4: Preserve Surface2D Validation

**Files:**
- Modify tests only if needed to preserve existing semantics.

- [ ] **Step 1: Run Surface2D subset**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "test_(surface_state|surface_step|dpm_fields|hydrostatic_reconstruction|hllc_flux|phi_t_source|surface2d_golden_minimal|cfl_rollback|conservative_update|wetting_drying|boundary_flux|boundary_conditions|momentum_transport|pressure_momentum)$"
```

Expected: PASS, 14/14 Surface2D tests passed.

- [ ] **Step 2: Run full CTest**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure
```

Expected: PASS, all tests passed.

---

## Task 5: Record M8 Evidence

**Files:**
- Create: `superpowers/specs/2026-05-17-plan-m8-surface2d-pressure-momentum-evidence.md`

- [ ] **Step 1: Write evidence document**

Create `superpowers/specs/2026-05-17-plan-m8-surface2d-pressure-momentum-evidence.md`:

```markdown
# M8 Surface2D Pressure-Normal Momentum Evidence

**Date:** 2026-05-17
**Plan:** `superpowers/plans/2026-05-17-plan-m8-surface2d-pressure-momentum.md`
**M7 Basis:** `superpowers/specs/2026-05-16-plan-m7-surface2d-momentum-transport-evidence.md`

## Scope

M8 adds a scalar pressure-normal momentum contribution to `Surface2DCore` edge diagnostics and cell momentum residuals. It builds on the M7 upwind momentum transport without changing the HLLC mass flux path or introducing the full HLLC wave-speed solver.

## Local Validation

- `cmake --build build/windows-msvc`: PASS
- Surface2D subset 14/14 PASS (`surface_state`, `surface_step`, `dpm_fields`, `hydrostatic_reconstruction`, `hllc_flux`, `phi_t_source`, `surface2d_golden_minimal`, `cfl_rollback`, `conservative_update`, `wetting_drying`, `boundary_flux`, `boundary_conditions`, `momentum_transport`, `pressure_momentum`)
- Full CTest: PASS

## Coverage

- `test_pressure_momentum`: internal-edge antisymmetry, dry-side blocking, Wall zero contribution, Open boundary contribution, and regression with M7 transport.
- Existing M3/M4/M5/M6/M7 Surface2D tests remain passing.

## Boundaries

- `Surface2DCore` remains independent of SWMM, D-Flow FM, `CouplingLib`, and 1D engine ABIs.
- Full HLLC normal momentum flux and wave-speed solving remain out of scope.
- This evidence does not claim GoldenSuite release readiness.
```

- [ ] **Step 2: Inspect git diff**

Run:

```bash
git status --short
git diff --stat
```

Expected: only M8 files changed.

- [ ] **Step 3: Commit M8 implementation after validation**

Suggested commit message:

```text
feat(surface2d): add pressure-normal momentum contribution
```

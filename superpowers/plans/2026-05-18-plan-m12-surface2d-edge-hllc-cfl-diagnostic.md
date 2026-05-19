# M12 Surface2D Edge HLLC CFL Diagnostic Implementation Plan

> **For agentic workers:** Execute this plan task-by-task. Preserve `Surface2DCore` independence from SWMM, D-Flow FM, `CouplingLib`, and 1D engine ABIs. Keep M12 limited to the CPU reference Surface2D CFL diagnostic and rollback flag computation. Do NOT introduce adaptive timestep selection, CUDA backends, coupling ledger behavior, GoldenSuite release claims, or 1D engine interface changes in this slice.

**Goal:** Replace the current cell-speed-over-area CFL approximation with an edge-based raw HLLC CFL diagnostic that reflects wave speeds, edge length, and cell area while preserving existing `C_rollback` comparison semantics.

**Architecture:** M12 reuses existing HLLC wave-speed estimation per active edge. Each adjacent cell receives `dt * edge.length * max(abs(S_l), abs(S_r)) / cell_area`, accumulated over its incident non-wall edges, and `StepDiagnostics::max_cell_cfl` reports the maximum cell value. `rollback_required` remains `max_cell_cfl > config.c_rollback`; `cfl_safety` remains validation-only for now and is not applied to the diagnostic.

**Tech Stack:** C++20, CMake, GoogleTest, existing `scau::core`, `scau::mesh`, and `scau::surface2d` targets.

---

## File Structure

- Modify `tests/unit/surface2d/test_cfl_rollback.cpp`
  - Update existing CFL expectations from the old cell-speed approximation to edge-based wave-speed CFL.
  - Add red tests proving static water has nonzero gravity-wave CFL and edge length contributes to the diagnostic.
- Modify `libs/surface2d/src/time_integration/step.cpp`
  - Replace `raw_cell_cfl(...)` with edge-based HLLC wave-speed CFL accumulation.
  - Keep `rollback_required = max_cell_cfl > config.c_rollback` unchanged.
  - Keep Wall boundaries out of CFL contribution; include internal and Open boundaries because they can carry HLLC flux.
- Create `superpowers/specs/2026-05-18-plan-m12-surface2d-edge-hllc-cfl-diagnostic-evidence.md`
  - Record focused CFL validation, Surface2D subset, and full CTest evidence after implementation.

---

## Task 1: Add Edge-Based CFL Red Tests

**Files:**
- Modify: `tests/unit/surface2d/test_cfl_rollback.cpp`

- [ ] **Step 1: Add required includes**

Add:

```cpp
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <unordered_map>
```

Also add:

```cpp
#include "surface2d/boundary/conditions.hpp"
#include "surface2d/riemann/hllc.hpp"
```

- [ ] **Step 2: Add local expected CFL helpers**

Add inside an anonymous namespace before the tests:

```cpp
namespace {

std::unordered_map<std::string, std::size_t> cell_indices_by_id(const scau::mesh::Mesh& mesh) {
    std::unordered_map<std::string, std::size_t> indices;
    for (std::size_t index = 0; index < mesh.cells.size(); ++index) {
        indices.emplace(mesh.cells[index].id, index);
    }
    return indices;
}

std::size_t edge_cell_index(
    const std::unordered_map<std::string, std::size_t>& cell_indices,
    const std::optional<std::string>& primary_cell,
    const std::optional<std::string>& fallback_cell) {
    const auto& cell_id = primary_cell.has_value() ? *primary_cell : *fallback_cell;
    return cell_indices.at(cell_id);
}

std::vector<double> cell_areas(const scau::mesh::Mesh& mesh) {
    const auto nodes = scau::mesh::node_lookup(mesh.nodes);
    std::vector<double> areas(mesh.cells.size(), 0.0);
    for (std::size_t cell_index = 0; cell_index < mesh.cells.size(); ++cell_index) {
        areas[cell_index] = scau::mesh::cell_area(mesh.cells[cell_index], nodes);
    }
    return areas;
}

double expected_edge_hllc_cfl(
    const scau::mesh::Mesh& mesh,
    const scau::surface2d::SurfaceState& state,
    const scau::surface2d::StepConfig& config,
    const scau::surface2d::BoundaryConditions& boundary) {
    const auto cell_indices = cell_indices_by_id(mesh);
    const auto areas = cell_areas(mesh);
    std::vector<double> cell_cfl(mesh.cells.size(), 0.0);

    for (std::size_t edge_index = 0; edge_index < mesh.edges.size(); ++edge_index) {
        const auto& edge = mesh.edges[edge_index];
        if (boundary.edges[edge_index] == scau::surface2d::BoundaryKind::Wall) {
            continue;
        }

        const std::size_t left_index = edge_cell_index(cell_indices, edge.left_cell, edge.right_cell);
        const std::size_t right_index = edge_cell_index(cell_indices, edge.right_cell, edge.left_cell);
        const auto speeds = scau::surface2d::estimate_hllc_wave_speeds(
            state.cells[left_index],
            state.cells[right_index],
            scau::surface2d::Normal2{.x = edge.normal.x, .y = edge.normal.y});
        const double spectral_radius = std::max(std::abs(speeds.s_l), std::abs(speeds.s_r));

        if (edge.left_cell.has_value() && areas[left_index] > 0.0) {
            cell_cfl[left_index] += config.dt * edge.length * spectral_radius / areas[left_index];
        }
        if (edge.right_cell.has_value() && areas[right_index] > 0.0) {
            cell_cfl[right_index] += config.dt * edge.length * spectral_radius / areas[right_index];
        }
    }

    return *std::max_element(cell_cfl.begin(), cell_cfl.end());
}

}  // namespace
```

- [ ] **Step 3: Replace old exact CFL expectations**

Change both existing tests to compute:

```cpp
const auto boundary = scau::surface2d::BoundaryConditions::for_mesh(mesh);
const double expected_cfl = expected_edge_hllc_cfl(mesh, state, config, boundary);
```

Then assert:

```cpp
EXPECT_DOUBLE_EQ(diagnostics.max_cell_cfl, expected_cfl);
```

Keep the existing rollback false/true assertions.

- [ ] **Step 4: Add static-water wave CFL test**

Append:

```cpp
TEST(SurfaceCflRollback, StaticWaterReportsGravityWaveCfl) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    const auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    const auto boundary = scau::surface2d::BoundaryConditions::for_mesh(mesh);

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 10.0};
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields, boundary);

    EXPECT_GT(diagnostics.max_cell_cfl, 0.0);
    EXPECT_DOUBLE_EQ(diagnostics.max_cell_cfl, expected_edge_hllc_cfl(mesh, state, config, boundary));
    EXPECT_FALSE(diagnostics.rollback_required);
}
```

Expected before implementation: FAILS because the old diagnostic reports zero for static water.

- [ ] **Step 5: Add edge-length contribution test**

Append:

```cpp
TEST(SurfaceCflRollback, EdgeLengthContributesToRawCfl) {
    auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto longer_edge_mesh = mesh;
    longer_edge_mesh.edges[0].length *= 2.0;

    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    auto longer_edge_state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(longer_edge_mesh, 1.0, 1.0);
    const auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    const auto longer_edge_dpm_fields = scau::surface2d::DpmFields::for_mesh(longer_edge_mesh);
    const auto boundary = scau::surface2d::BoundaryConditions::for_mesh(mesh);
    const auto longer_edge_boundary = scau::surface2d::BoundaryConditions::for_mesh(longer_edge_mesh);

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 10.0};
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields, boundary);
    const auto longer_edge_diagnostics = scau::surface2d::advance_one_step_cpu(
        longer_edge_mesh,
        longer_edge_state,
        config,
        longer_edge_dpm_fields,
        longer_edge_boundary);

    EXPECT_GT(longer_edge_diagnostics.max_cell_cfl, diagnostics.max_cell_cfl);
    EXPECT_DOUBLE_EQ(
        longer_edge_diagnostics.max_cell_cfl,
        expected_edge_hllc_cfl(longer_edge_mesh, longer_edge_state, config, longer_edge_boundary));
}
```

Expected before implementation: FAILS because the old diagnostic ignores edge lengths.

- [ ] **Step 6: Run red CFL target**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_cfl_rollback --config Debug && build/windows-msvc/tests/unit/surface2d/Debug/test_cfl_rollback.exe
```

Expected before implementation: at least `StaticWaterReportsGravityWaveCfl` and `EdgeLengthContributesToRawCfl` FAIL.

---

## Task 2: Implement Edge-Based Raw CFL Diagnostic

**Files:**
- Modify: `libs/surface2d/src/time_integration/step.cpp`

- [ ] **Step 1: Add required includes**

Add:

```cpp
#include <optional>
#include <vector>
```

if not already available through direct includes.

- [ ] **Step 2: Add cell-area helper**

Add near `cell_indices_by_id(...)`:

```cpp
std::vector<core::Real> cell_areas(const mesh::Mesh& mesh) {
    const auto nodes = mesh::node_lookup(mesh.nodes);
    std::vector<core::Real> areas(mesh.cells.size(), 0.0);
    for (std::size_t cell_index = 0; cell_index < mesh.cells.size(); ++cell_index) {
        areas[cell_index] = mesh::cell_area(mesh.cells[cell_index], nodes);
    }
    return areas;
}
```

- [ ] **Step 3: Replace `raw_cell_cfl(...)` implementation**

Use:

```cpp
core::Real raw_cell_cfl(
    const mesh::Mesh& mesh,
    const SurfaceState& state,
    const StepConfig& config,
    const BoundaryConditions& boundary) {
    const auto cell_indices = cell_indices_by_id(mesh);
    const auto areas = cell_areas(mesh);
    std::vector<core::Real> cell_cfl(mesh.cells.size(), 0.0);

    for (std::size_t edge_index = 0; edge_index < mesh.edges.size(); ++edge_index) {
        const auto& edge = mesh.edges[edge_index];
        if (boundary.edges[edge_index] == BoundaryKind::Wall) {
            continue;
        }

        const std::size_t left_index = edge_cell_index(cell_indices, edge.left_cell, edge.right_cell);
        const std::size_t right_index = edge_cell_index(cell_indices, edge.right_cell, edge.left_cell);
        const auto speeds = estimate_hllc_wave_speeds(
            state.cells[left_index],
            state.cells[right_index],
            Normal2{.x = edge.normal.x, .y = edge.normal.y});
        const core::Real spectral_radius = std::max(std::abs(speeds.s_l), std::abs(speeds.s_r));

        if (edge.left_cell.has_value() && areas[left_index] > 0.0) {
            cell_cfl[left_index] += config.dt * edge.length * spectral_radius / areas[left_index];
        }
        if (edge.right_cell.has_value() && areas[right_index] > 0.0) {
            cell_cfl[right_index] += config.dt * edge.length * spectral_radius / areas[right_index];
        }
    }

    return *std::max_element(cell_cfl.begin(), cell_cfl.end());
}
```

- [ ] **Step 4: Thread boundary into base diagnostics**

Change `base_diagnostics(...)` to accept `const BoundaryConditions& boundary` and call:

```cpp
const core::Real max_cell_cfl = raw_cell_cfl(mesh, state, config, boundary);
```

Update callers:

```cpp
return base_diagnostics(mesh, state, config, BoundaryConditions::for_mesh(mesh));
```

and:

```cpp
auto diagnostics = base_diagnostics(mesh, state, config, boundary);
```

- [ ] **Step 5: Run CFL target**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_cfl_rollback --config Debug && build/windows-msvc/tests/unit/surface2d/Debug/test_cfl_rollback.exe
```

Expected after implementation: all `test_cfl_rollback` cases PASS.

---

## Task 3: Run M12 Regression Targets

**Files:**
- No source changes expected.

- [ ] **Step 1: Build and run focused targets**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_cfl_rollback test_surface_step test_hllc_flux test_hllc_wave_mass test_hllc_wave_momentum --config Debug && PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug -R "^(test_cfl_rollback|test_surface_step|test_hllc_flux|test_hllc_wave_mass|test_hllc_wave_momentum)$" --output-on-failure
```

Expected: all focused targets PASS.

- [ ] **Step 2: Build full Debug tree**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --config Debug
```

Expected: build PASS.

- [ ] **Step 3: Run Surface2D subset and full CTest**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug -R "surface_state|surface_step|dpm_fields|hydrostatic_reconstruction|hllc_flux|phi_t_source|surface2d_golden_minimal|cfl_rollback|conservative_update|wetting_drying|boundary_flux|boundary_conditions|momentum_transport|pressure_momentum|hllc_wave_momentum|hllc_wave_mass" --output-on-failure && PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure
```

Expected: Surface2D subset and full CTest PASS.

---

## Task 4: Record M12 Evidence And Commit

**Files:**
- Create: `superpowers/specs/2026-05-18-plan-m12-surface2d-edge-hllc-cfl-diagnostic-evidence.md`

- [ ] **Step 1: Create evidence file**

Create `superpowers/specs/2026-05-18-plan-m12-surface2d-edge-hllc-cfl-diagnostic-evidence.md`:

```markdown
# M12 Surface2D Edge HLLC CFL Diagnostic Evidence

**Date:** 2026-05-18
**Plan:** `superpowers/plans/2026-05-18-plan-m12-surface2d-edge-hllc-cfl-diagnostic.md`
**M11 Basis:** `superpowers/specs/2026-05-18-plan-m11-surface2d-hllc-vector-momentum-projection-evidence.md`

## Scope

M12 replaces the old cell-speed-over-area CFL approximation with an edge-based raw HLLC CFL diagnostic in the CPU reference `Surface2DCore` time-integration path. It preserves `rollback_required = max_cell_cfl > C_rollback`, keeps `cfl_safety` out of the raw diagnostic, and does not introduce adaptive timestep selection.

## Local Validation

- Focused M12 targets: PASS, update with observed count.
- Full Debug build: PASS.
- Surface2D CTest subset: PASS, update with observed count.
- Full CTest: PASS, update with observed count.

## Coverage

- `test_cfl_rollback`: raw CFL remains unscaled by `cfl_safety`, rollback compares only against `C_rollback`, static water reports gravity-wave CFL, and edge length contributes to CFL.
- Existing HLLC flux, wave-speed, surface-step, boundary, wetting/drying, conservative update, momentum, pressure, and golden-minimal tests remain passing.

## Boundaries

- `Surface2DCore` remains independent of SWMM, D-Flow FM, `CouplingLib`, and 1D engine ABIs.
- M12 remains limited to CPU reference Surface2D CFL diagnostics and rollback flag computation.
- M12 does not introduce adaptive timestep selection, CUDA backends, coupling ledger behavior, GoldenSuite release claims, or 1D engine interface changes.
```

- [ ] **Step 2: Review changed files**

Run:

```bash
git status --short && git diff -- tests/unit/surface2d/test_cfl_rollback.cpp libs/surface2d/src/time_integration/step.cpp superpowers/plans/2026-05-18-plan-m12-surface2d-edge-hllc-cfl-diagnostic.md superpowers/specs/2026-05-18-plan-m12-surface2d-edge-hllc-cfl-diagnostic-evidence.md
```

Expected:
- Only M12 test, step, plan, and evidence files changed.
- No `Surface2DCore` dependency on SWMM, D-Flow FM, `CouplingLib`, or 1D engine ABIs.

- [ ] **Step 3: Commit implementation**

Run:

```bash
git add tests/unit/surface2d/test_cfl_rollback.cpp libs/surface2d/src/time_integration/step.cpp superpowers/plans/2026-05-18-plan-m12-surface2d-edge-hllc-cfl-diagnostic.md superpowers/specs/2026-05-18-plan-m12-surface2d-edge-hllc-cfl-diagnostic-evidence.md
git commit -m "feat(surface2d): compute CFL from HLLC edge waves"
```

Expected: commit succeeds after all validation is recorded.

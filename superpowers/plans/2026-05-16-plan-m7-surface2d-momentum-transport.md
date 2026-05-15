# M7 Surface2D Momentum Transport Implementation Plan

> **For agentic workers:** Execute this plan task-by-task. Preserve `Surface2DCore` independence from SWMM, D-Flow FM, `CouplingLib`, and 1D engine ABIs. Keep M7 limited to upwind momentum transport carried by the existing HLLC mass flux. Do NOT modify the HLLC `momentum_n` formula in this slice; the pressure term `0.5 * g * h^2` and the full HLLC momentum flux are explicitly deferred to a later slice.

**Goal:** Add per-cell momentum residual accumulation and `hu`/`hv` finite-volume updates to `Surface2DCore` using upwind transport driven by the existing HLLC mass flux.

**Architecture:** M7 extends the M6 CPU reference step inside `libs/surface2d` only. A new nested `MomentumResidual{x, y}` is added to `CellStepDiagnostics`. Each internal or Open boundary edge computes an upwind cell-state pick driven by the sign of the existing HLLC mass flux, and accumulates `mass_flux * u_upwind * edge.length` and `mass_flux * v_upwind * edge.length` into the appropriate cells. A new `apply_momentum_update` runs after `apply_depth_update`; when the updated cell depth `h <= h_min`, both `hu` and `hv` are zeroed to prevent infinite velocity. Wall boundaries continue to contribute nothing. The HLLC normal momentum flux remains zero in this slice.

**Tech Stack:** C++20, CMake, GoogleTest, existing `scau::core`, `scau::mesh`, and `scau::surface2d` targets.

---

## File Structure

- Modify `libs/surface2d/include/surface2d/time_integration/step.hpp`
  - Add `MomentumResidual{x, y}` nested in `CellStepDiagnostics`.
- Modify `libs/surface2d/src/time_integration/step.cpp`
  - Add `upwind_state` helper that picks left/right based on `mass_flux` sign (zero -> left for determinism).
  - Add `accumulate_momentum_residual` helper used by internal and Open boundary branches.
  - Add `apply_momentum_update` after `apply_depth_update`, zeroing `hu`/`hv` when `h <= config.h_min`.
- Create `tests/unit/surface2d/test_momentum_transport.cpp`
  - Tests for internal-edge upwind antisymmetry, momentum update advancing `hu`/`hv`, dry-cell momentum zeroing, and Open boundary momentum contribution.
- Modify `tests/unit/surface2d/CMakeLists.txt`
  - Register the new test executable.
- Create `superpowers/specs/2026-05-16-plan-m7-surface2d-momentum-transport-evidence.md`
  - Record build/test evidence after implementation.

---

## Task 1: Add Momentum Transport Tests

**Files:**
- Create: `tests/unit/surface2d/test_momentum_transport.cpp`
- Modify: `tests/unit/surface2d/CMakeLists.txt`

- [ ] **Step 1: Create momentum transport tests**

Create `tests/unit/surface2d/test_momentum_transport.cpp`:

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

TEST(SurfaceMomentumTransport, InternalEdgeMomentumResidualIsAntisymmetric) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto edge_index = first_internal_edge_index(mesh);
    const auto left_index = cell_index_by_id(mesh, *mesh.edges[edge_index].left_cell);
    const auto right_index = cell_index_by_id(mesh, *mesh.edges[edge_index].right_cell);

    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    state.cells[left_index].conserved.hu = 1.0;
    state.cells[left_index].conserved.hv = 0.5;
    state.cells[right_index].conserved.hu = 1.0;
    state.cells[right_index].conserved.hv = 0.5;
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

TEST(SurfaceMomentumTransport, AdvancesHuHvFromInternalMassFlux) {
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

TEST(SurfaceMomentumTransport, DryCellMomentumIsZeroedAfterDepthClamp) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto edge_index = first_internal_edge_index(mesh);
    const auto left_index = cell_index_by_id(mesh, *mesh.edges[edge_index].left_cell);
    const auto right_index = cell_index_by_id(mesh, *mesh.edges[edge_index].right_cell);

    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    state.cells[left_index].conserved.h = 1.0e-4;
    state.cells[left_index].conserved.hu = 10.0;
    state.cells[left_index].conserved.hv = 10.0;
    state.cells[right_index].conserved.hu = 10.0;
    state.cells[right_index].conserved.hv = 10.0;
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    isolate_edge(dpm_fields, edge_index);

    const scau::surface2d::StepConfig config{.dt = 1.0, .cfl_safety = 0.45, .c_rollback = 100000.0, .h_min = 1.0e-8};
    static_cast<void>(scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields));

    if (state.cells[left_index].conserved.h <= config.h_min) {
        EXPECT_EQ(state.cells[left_index].conserved.hu, 0.0);
        EXPECT_EQ(state.cells[left_index].conserved.hv, 0.0);
    }
    if (state.cells[right_index].conserved.h <= config.h_min) {
        EXPECT_EQ(state.cells[right_index].conserved.hu, 0.0);
        EXPECT_EQ(state.cells[right_index].conserved.hv, 0.0);
    }
}

TEST(SurfaceMomentumTransport, OpenBoundaryContributesMomentumResidual) {
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

TEST(SurfaceMomentumTransport, WallBoundaryContributesNoMomentumResidual) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto edge_index = first_boundary_edge_index(mesh);
    const auto cell_index = edge_cell_index(mesh, edge_index);

    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    state.cells[cell_index].conserved.hu = 1.0;
    state.cells[cell_index].conserved.hv = 1.0;
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    isolate_edge(dpm_fields, edge_index);
    auto boundary = scau::surface2d::BoundaryConditions::for_mesh(mesh);
    boundary.edges[edge_index] = scau::surface2d::BoundaryKind::Wall;

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 10.0};
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields, boundary);

    EXPECT_EQ(diagnostics.cells[cell_index].momentum_residual.x, 0.0);
    EXPECT_EQ(diagnostics.cells[cell_index].momentum_residual.y, 0.0);
}
```

- [ ] **Step 2: Register momentum transport test**

Append to `tests/unit/surface2d/CMakeLists.txt`:

```cmake
add_executable(test_momentum_transport test_momentum_transport.cpp)
target_link_libraries(test_momentum_transport
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_momentum_transport COMMAND test_momentum_transport)
```

- [ ] **Step 3: Run the new test and verify it fails**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_momentum_transport
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R test_momentum_transport
```

Expected: compile FAIL because `CellStepDiagnostics::momentum_residual` does not exist yet.

---

## Task 2: Add MomentumResidual To Diagnostics

**Files:**
- Modify: `libs/surface2d/include/surface2d/time_integration/step.hpp`

- [ ] **Step 1: Nest MomentumResidual in CellStepDiagnostics**

In `libs/surface2d/include/surface2d/time_integration/step.hpp`, replace the existing `CellStepDiagnostics` with:

```cpp
struct MomentumResidual {
    core::Real x{0.0};
    core::Real y{0.0};
};

struct CellStepDiagnostics {
    core::Real mass_residual{0.0};
    MomentumResidual momentum_residual{};
};
```

Keep `StepDiagnostics`, `EdgeStepDiagnostics`, `StepConfig`, and the existing overload declarations unchanged.

- [ ] **Step 2: Compile-only smoke check**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target scau_surface2d
```

Expected: PASS (no consumers reference `momentum_residual` yet).

---

## Task 3: Implement Upwind Momentum Transport And Apply Step

**Files:**
- Modify: `libs/surface2d/src/time_integration/step.cpp`

- [ ] **Step 1: Add upwind helper**

In the anonymous namespace in `libs/surface2d/src/time_integration/step.cpp`, above `open_boundary_outside_state`, add:

```cpp
const CellState& upwind_state(core::Real mass_flux, const CellState& left, const CellState& right) {
    return mass_flux >= 0.0 ? left : right;
}
```

Decision: `mass_flux == 0.0` deterministically picks `left`. The momentum contribution will be zero anyway because `mass_flux` multiplies the upwind velocity.

- [ ] **Step 2: Add accumulate helper**

Below `upwind_state` in the anonymous namespace, add:

```cpp
void accumulate_momentum_residual(
    CellStepDiagnostics& sink,
    core::Real signed_integrated_flux,
    const CellState& upwind) {
    sink.momentum_residual.x += signed_integrated_flux * upwind.u();
    sink.momentum_residual.y += signed_integrated_flux * upwind.v();
}
```

- [ ] **Step 3: Wire internal-edge momentum accumulation**

In the four-argument `advance_one_step_cpu`, inside the `is_internal` branch, after the existing `mass_residual` accumulation, add:

```cpp
const auto& upwind = upwind_state(edge_diagnostics.mass_flux, state.cells[left_index], state.cells[right_index]);
accumulate_momentum_residual(diagnostics.cells[left_index], -integrated_flux, upwind);
accumulate_momentum_residual(diagnostics.cells[right_index], integrated_flux, upwind);
continue;
```

The existing `continue;` at the end of the `is_internal` branch must be replaced by the block above so it remains the only `continue`.

- [ ] **Step 4: Wire Open boundary momentum accumulation**

In the same overload, after the Open boundary `integrated_flux` line and before `apply_depth_update`, replace the existing inside-cell residual accumulation with:

```cpp
const core::Real signed_integrated_flux = edge.left_cell.has_value() ? -integrated_flux : integrated_flux;
diagnostics.cells[inside_index].mass_residual += signed_integrated_flux;

const auto outside_for_upwind = open_boundary_outside_state(inside);
const auto& upwind_open = upwind_state(edge_diagnostics.mass_flux, inside, outside_for_upwind);
accumulate_momentum_residual(diagnostics.cells[inside_index], signed_integrated_flux, upwind_open);
```

Wall boundaries continue to push an empty `EdgeStepDiagnostics{}` and skip residual accumulation, so they contribute nothing.

- [ ] **Step 5: Add apply_momentum_update**

Below `apply_depth_update` in the anonymous namespace, add:

```cpp
void apply_momentum_update(
    const mesh::Mesh& mesh,
    SurfaceState& state,
    const StepConfig& config,
    const std::vector<CellStepDiagnostics>& cells) {
    const auto nodes = mesh::node_lookup(mesh.nodes);
    for (std::size_t cell_index = 0; cell_index < mesh.cells.size(); ++cell_index) {
        const core::Real area = mesh::cell_area(mesh.cells[cell_index], nodes);
        if (area <= 0.0) {
            continue;
        }
        if (state.cells[cell_index].conserved.h <= config.h_min) {
            state.cells[cell_index].conserved.hu = 0.0;
            state.cells[cell_index].conserved.hv = 0.0;
            continue;
        }
        state.cells[cell_index].conserved.hu += config.dt * cells[cell_index].momentum_residual.x / area;
        state.cells[cell_index].conserved.hv += config.dt * cells[cell_index].momentum_residual.y / area;
    }
}
```

- [ ] **Step 6: Call apply_momentum_update after apply_depth_update**

In the four-argument `advance_one_step_cpu`, after the existing `apply_depth_update(mesh, state, config, diagnostics.cells);` call and before `return diagnostics;`, add:

```cpp
apply_momentum_update(mesh, state, config, diagnostics.cells);
```

- [ ] **Step 7: Run momentum transport tests**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_momentum_transport
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R test_momentum_transport
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
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "test_(surface_state|surface_step|dpm_fields|hydrostatic_reconstruction|hllc_flux|phi_t_source|surface2d_golden_minimal|cfl_rollback|conservative_update|wetting_drying|boundary_flux|boundary_conditions|momentum_transport)$"
```

Expected: PASS, 13/13 Surface2D tests passed.

- [ ] **Step 2: Run full CTest**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure
```

Expected: PASS, all tests passed.

---

## Task 5: Record M7 Evidence

**Files:**
- Create: `superpowers/specs/2026-05-16-plan-m7-surface2d-momentum-transport-evidence.md`

- [ ] **Step 1: Write evidence document**

Create `superpowers/specs/2026-05-16-plan-m7-surface2d-momentum-transport-evidence.md`:

```markdown
# M7 Surface2D Momentum Transport Evidence

**Date:** 2026-05-16
**Plan:** `superpowers/plans/2026-05-16-plan-m7-surface2d-momentum-transport.md`
**M6 Basis:** `superpowers/specs/2026-05-14-plan-m6-surface2d-boundary-conditions-evidence.md`

## Scope

M7 adds per-cell momentum residual accumulation and finite-volume `hu`/`hv` updates for `Surface2DCore`. Momentum transport uses the existing HLLC mass flux as the carrier (upwind cell state selected by mass flux sign). The HLLC `momentum_n` formula remains zero in this slice; the pressure term and the full HLLC momentum flux are deferred.

## Local Validation

- `cmake --build build/windows-msvc`: PASS
- Surface2D subset 13/13 PASS (`surface_state`, `surface_step`, `dpm_fields`, `hydrostatic_reconstruction`, `hllc_flux`, `phi_t_source`, `surface2d_golden_minimal`, `cfl_rollback`, `conservative_update`, `wetting_drying`, `boundary_flux`, `boundary_conditions`, `momentum_transport`)
- Full CTest: PASS

## Coverage

- `test_momentum_transport`: internal-edge upwind antisymmetry, momentum update advances `hu`/`hv`, dry-cell `hu`/`hv` zeroing after depth clamp, Open boundary momentum contribution, Wall boundary zero contribution.
- Existing M3/M4/M5/M6 Surface2D tests remain passing.

## Boundaries

- `Surface2DCore` remains independent of SWMM, D-Flow FM, `CouplingLib`, and 1D engine ABIs.
- HLLC momentum flux pressure term (`0.5 * g * h^2`) and full HLLC momentum_n remain out of scope.
- This evidence does not claim GoldenSuite release readiness.
```

- [ ] **Step 2: Inspect git diff**

Run:

```bash
git status --short
git diff --stat
```

Expected: only M7 files changed.

- [ ] **Step 3: Commit M7 implementation after validation**

Suggested commit message:

```text
feat(surface2d): add upwind momentum transport
```

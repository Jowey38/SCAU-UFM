# M10 Surface2D HLLC Mass Flux Implementation Plan

> **For agentic workers:** Execute this plan task-by-task. Preserve `Surface2DCore` independence from SWMM, D-Flow FM, `CouplingLib`, and 1D engine ABIs. Keep M10 limited to the CPU reference HLLC edge flux path. Do NOT introduce CUDA backends, prescribed external boundary states, coupling ledger behavior, GoldenSuite release claims, or changes to 1D engine interfaces in this slice.

**Goal:** Replace the centered normal mass-flux diagnostic with an HLLC wave-speed branch-selected mass flux that uses the same reconstructed states and signal-speed branch as the M9 normal momentum flux.

**Architecture:** M10 stays inside `libs/surface2d` and keeps the existing `hllc_normal_flux(...)` API stable. The implementation reuses M9 hydrostatic reconstruction and `WaveSpeeds` estimation, computes physical and star-region mass fluxes per HLLC branch, and applies `phi_e_n` scaling exactly once in the returned `EdgeFlux`. Existing dry-edge blocking, DPM hard/soft blocking, Wall/Open boundary behavior, CFL diagnostics, and momentum residual projection semantics remain unchanged.

**Tech Stack:** C++20, CMake, GoogleTest, existing `scau::core`, `scau::mesh`, and `scau::surface2d` targets.

---

## File Structure

- Modify `libs/surface2d/src/riemann/hllc.cpp`
  - Add local helpers for physical normal mass flux and HLLC star-region normal mass flux.
  - Replace the centered `.mass` formula in `hllc_normal_flux(...)` with branch-selected HLLC mass flux using the same `WaveSpeeds` as `momentum_n`.
  - Keep `EdgeFlux` shape and `hllc_normal_flux(...)` signature unchanged.
- Modify `tests/unit/surface2d/test_hllc_flux.cpp`
  - Add direct flux-level tests proving asymmetric left/right states no longer use a centered mass average.
  - Preserve hydrostatic zero mass flux and DPM blocking expectations.
- Create `tests/unit/surface2d/test_hllc_wave_mass.cpp`
  - Cover step-level mass-flux behavior through `advance_one_step_cpu(...)` for internal edges, dry edges, DPM-blocked edges, Wall boundaries, and Open boundaries.
- Modify `tests/unit/surface2d/CMakeLists.txt`
  - Register `test_hllc_wave_mass`.
- Create `superpowers/specs/2026-05-18-plan-m10-surface2d-hllc-mass-flux-evidence.md`
  - Record target build/test results, Surface2D subset evidence, and full CTest evidence after implementation.

---

## Task 1: Add Flux-Level HLLC Mass Tests

**Files:**
- Modify: `tests/unit/surface2d/test_hllc_flux.cpp`

- [ ] **Step 1: Add a one-sided right-going mass-flux test**

Append this test after the existing velocity-direction flux test:

```cpp
TEST(HllcFlux, MassFluxUsesLeftStateWhenAllWavesMoveRight) {
    const scau::surface2d::CellState left{.conserved = {.h = 1.0, .hu = 4.0, .hv = 0.0}, .eta = 1.0};
    const scau::surface2d::CellState right{.conserved = {.h = 1.0, .hu = 0.0, .hv = 0.0}, .eta = 1.0};
    const scau::surface2d::EdgeDpmFields edge_fields{.phi_e_n = 1.0, .omega_edge = 1.0};
    const auto normal = scau::surface2d::Normal2{.x = 1.0, .y = 0.0};

    const auto flux = scau::surface2d::hllc_normal_flux(left, right, edge_fields, normal);

    EXPECT_DOUBLE_EQ(flux.mass, 4.0);
}
```

This should fail before implementation because the centered M9 formula returns `2.0`.

- [ ] **Step 2: Add a `phi_e_n` scaling test for HLLC mass**

Append this test after the one-sided test:

```cpp
TEST(HllcFlux, MassFluxAppliesPhiEdgeScalingAfterHllcBranchSelection) {
    const scau::surface2d::CellState left{.conserved = {.h = 1.0, .hu = 4.0, .hv = 0.0}, .eta = 1.0};
    const scau::surface2d::CellState right{.conserved = {.h = 1.0, .hu = 0.0, .hv = 0.0}, .eta = 1.0};
    const scau::surface2d::EdgeDpmFields edge_fields{.phi_e_n = 0.25, .omega_edge = 1.0};
    const auto normal = scau::surface2d::Normal2{.x = 1.0, .y = 0.0};

    const auto flux = scau::surface2d::hllc_normal_flux(left, right, edge_fields, normal);

    EXPECT_DOUBLE_EQ(flux.mass, 1.0);
}
```

This should fail before implementation because the centered M9 formula returns `0.5`.

- [ ] **Step 3: Run the flux target and confirm red tests**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_hllc_flux --config Debug && build/windows-msvc/tests/unit/surface2d/Debug/test_hllc_flux.exe
```

Expected before implementation:
- `MassFluxUsesLeftStateWhenAllWavesMoveRight` FAILS with actual `2.0`.
- `MassFluxAppliesPhiEdgeScalingAfterHllcBranchSelection` FAILS with actual `0.5`.

---

## Task 2: Add Step-Level HLLC Mass Tests

**Files:**
- Create: `tests/unit/surface2d/test_hllc_wave_mass.cpp`
- Modify: `tests/unit/surface2d/CMakeLists.txt`

- [ ] **Step 1: Create step-level mass tests**

Create `tests/unit/surface2d/test_hllc_wave_mass.cpp`:

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
    return cell_index_by_id(mesh, cell_id);
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

TEST(SurfaceHllcWaveMass, InternalMassFluxIncreasesWithLeftNormalVelocity) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto edge_index = first_internal_edge_index(mesh);
    const auto left_index = cell_index_by_id(mesh, *mesh.edges[edge_index].left_cell);

    auto still_state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    auto moving_state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    moving_state.cells[left_index].conserved.hu = 4.0;
    moving_state.cells[left_index].conserved.hv = 4.0;

    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    isolate_edge(dpm_fields, edge_index);

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 10.0};
    const auto still_diagnostics = scau::surface2d::advance_one_step_cpu(mesh, still_state, config, dpm_fields);
    const auto moving_diagnostics = scau::surface2d::advance_one_step_cpu(mesh, moving_state, config, dpm_fields);

    EXPECT_GT(moving_diagnostics.edges[edge_index].mass_flux, still_diagnostics.edges[edge_index].mass_flux);
}

TEST(SurfaceHllcWaveMass, DryInternalEdgeStillBlocksMassFlux) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto edge_index = first_internal_edge_index(mesh);
    const auto left_index = cell_index_by_id(mesh, *mesh.edges[edge_index].left_cell);
    const auto right_index = cell_index_by_id(mesh, *mesh.edges[edge_index].right_cell);

    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    state.cells[left_index].conserved.h = 1.0e-8;
    state.cells[right_index].conserved.hu = 4.0;
    state.cells[right_index].conserved.hv = 4.0;
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    isolate_edge(dpm_fields, edge_index);

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 10.0, .h_min = 1.0e-6};
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields);

    EXPECT_EQ(diagnostics.edges[edge_index].mass_flux, 0.0);
    EXPECT_EQ(diagnostics.edges[edge_index].momentum_flux_n, 0.0);
}

TEST(SurfaceHllcWaveMass, BlockedDpmEdgeStillZerosMassFlux) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto edge_index = first_internal_edge_index(mesh);
    const auto left_index = cell_index_by_id(mesh, *mesh.edges[edge_index].left_cell);

    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    state.cells[left_index].conserved.hu = 4.0;
    state.cells[left_index].conserved.hv = 4.0;
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    dpm_fields.edges[edge_index].omega_edge = 0.0;
    dpm_fields.edges[edge_index].phi_e_n = scau::surface2d::PhiEdgeMin * 0.5;

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 10.0};
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields);

    EXPECT_EQ(diagnostics.edges[edge_index].mass_flux, 0.0);
    EXPECT_EQ(diagnostics.edges[edge_index].momentum_flux_n, 0.0);
}

TEST(SurfaceHllcWaveMass, WallBoundaryStillContributesNoMassFlux) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto edge_index = first_boundary_edge_index(mesh);
    const auto cell_index = edge_cell_index(mesh, edge_index);

    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    state.cells[cell_index].conserved.hu = 4.0;
    state.cells[cell_index].conserved.hv = 4.0;
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    isolate_edge(dpm_fields, edge_index);
    auto boundary = scau::surface2d::BoundaryConditions::for_mesh(mesh);
    boundary.edges[edge_index] = scau::surface2d::BoundaryKind::Wall;

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 10.0};
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields, boundary);

    EXPECT_EQ(diagnostics.edges[edge_index].mass_flux, 0.0);
    EXPECT_EQ(diagnostics.edges[edge_index].momentum_flux_n, 0.0);
}

TEST(SurfaceHllcWaveMass, OpenBoundaryUsesWaveMassFlux) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto edge_index = first_boundary_edge_index(mesh);
    const auto cell_index = edge_cell_index(mesh, edge_index);

    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    state.cells[cell_index].conserved.hu = 4.0;
    state.cells[cell_index].conserved.hv = 4.0;
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    isolate_edge(dpm_fields, edge_index);
    auto boundary = scau::surface2d::BoundaryConditions::for_mesh(mesh);
    boundary.edges[edge_index] = scau::surface2d::BoundaryKind::Open;

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 10.0};
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields, boundary);

    EXPECT_NE(diagnostics.edges[edge_index].mass_flux, 0.0);
}
```

- [ ] **Step 2: Register the test target**

Add this block to `tests/unit/surface2d/CMakeLists.txt` near the other Surface2D unit test executables:

```cmake
add_executable(test_hllc_wave_mass test_hllc_wave_mass.cpp)
target_link_libraries(test_hllc_wave_mass
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_hllc_wave_mass COMMAND test_hllc_wave_mass)
```

- [ ] **Step 3: Build and run the new target**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_hllc_wave_mass --config Debug && build/windows-msvc/tests/unit/surface2d/Debug/test_hllc_wave_mass.exe
```

Expected before implementation:
- The new target builds once registered.
- The internal mass-flux sensitivity test may pass under M9 because centered mass still responds to velocity, but the flux-level tests from Task 1 remain the required red evidence for branch correctness.

---

## Task 3: Implement Branch-Selected HLLC Mass Flux

**Files:**
- Modify: `libs/surface2d/src/riemann/hllc.cpp`

- [ ] **Step 1: Add mass-flux helpers**

Add these helpers in the anonymous namespace after `wave_celerity(...)`:

```cpp
core::Real physical_normal_mass_flux(const CellState& state, Normal2 normal) {
    return state.conserved.h * normal_velocity(state, normal);
}

core::Real hllc_star_normal_mass_flux(
    const CellState& state,
    Normal2 normal,
    core::Real s_k,
    core::Real s_star) {
    const core::Real un = normal_velocity(state, normal);
    const core::Real denominator = s_k - s_star;
    if (denominator == 0.0) {
        return physical_normal_mass_flux(state, normal);
    }
    const core::Real h_star = state.conserved.h * (s_k - un) / denominator;
    return physical_normal_mass_flux(state, normal) + s_k * (h_star - state.conserved.h);
}
```

- [ ] **Step 2: Select mass flux in the existing HLLC branch**

Replace the current branch body in `hllc_normal_flux(...)`:

```cpp
core::Real momentum_n = 0.0;
if (0.0 <= speeds.s_l) {
    momentum_n = physical_normal_momentum_flux(pair.left, normal, 9.81);
} else if (speeds.s_l <= 0.0 && 0.0 <= speeds.s_star) {
    momentum_n = hllc_star_normal_momentum_flux(pair.left, normal, speeds.s_l, speeds.s_star, 9.81);
} else if (speeds.s_star <= 0.0 && 0.0 <= speeds.s_r) {
    momentum_n = hllc_star_normal_momentum_flux(pair.right, normal, speeds.s_r, speeds.s_star, 9.81);
} else {
    momentum_n = physical_normal_momentum_flux(pair.right, normal, 9.81);
}

return EdgeFlux{
    .mass = 0.5 * edge_fields.phi_e_n * (pair.left.conserved.h * left_un + pair.right.conserved.h * right_un),
    .momentum_n = edge_fields.phi_e_n * momentum_n,
};
```

with:

```cpp
core::Real mass = 0.0;
core::Real momentum_n = 0.0;
if (0.0 <= speeds.s_l) {
    mass = physical_normal_mass_flux(pair.left, normal);
    momentum_n = physical_normal_momentum_flux(pair.left, normal, 9.81);
} else if (speeds.s_l <= 0.0 && 0.0 <= speeds.s_star) {
    mass = hllc_star_normal_mass_flux(pair.left, normal, speeds.s_l, speeds.s_star);
    momentum_n = hllc_star_normal_momentum_flux(pair.left, normal, speeds.s_l, speeds.s_star, 9.81);
} else if (speeds.s_star <= 0.0 && 0.0 <= speeds.s_r) {
    mass = hllc_star_normal_mass_flux(pair.right, normal, speeds.s_r, speeds.s_star);
    momentum_n = hllc_star_normal_momentum_flux(pair.right, normal, speeds.s_r, speeds.s_star, 9.81);
} else {
    mass = physical_normal_mass_flux(pair.right, normal);
    momentum_n = physical_normal_momentum_flux(pair.right, normal, 9.81);
}

return EdgeFlux{
    .mass = edge_fields.phi_e_n * mass,
    .momentum_n = edge_fields.phi_e_n * momentum_n,
};
```

Remove the now-unused `left_un` and `right_un` locals if the compiler reports them unused.

- [ ] **Step 3: Run flux tests and confirm green**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_hllc_flux --config Debug && build/windows-msvc/tests/unit/surface2d/Debug/test_hllc_flux.exe
```

Expected after implementation:
- All `test_hllc_flux` cases PASS.
- Hydrostatic mass remains `0.0`.
- One-sided right-going mass returns `4.0` before `phi_e_n` scaling.

---

## Task 4: Run M9/M10 Regression Targets

**Files:**
- No source changes expected.

- [ ] **Step 1: Build target tests**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_hllc_flux test_hllc_wave_momentum test_hllc_wave_mass --config Debug
```

Expected: build PASS.

- [ ] **Step 2: Run target executables**

Run:

```bash
build/windows-msvc/tests/unit/surface2d/Debug/test_hllc_flux.exe && build/windows-msvc/tests/unit/surface2d/Debug/test_hllc_wave_momentum.exe && build/windows-msvc/tests/unit/surface2d/Debug/test_hllc_wave_mass.exe
```

Expected: all target tests PASS.

- [ ] **Step 3: Run M7/M8/M9/M10 regression subset**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_hllc_flux test_momentum_transport test_pressure_momentum test_hllc_wave_momentum test_hllc_wave_mass --config Debug && build/windows-msvc/tests/unit/surface2d/Debug/test_hllc_flux.exe && build/windows-msvc/tests/unit/surface2d/Debug/test_momentum_transport.exe && build/windows-msvc/tests/unit/surface2d/Debug/test_pressure_momentum.exe && build/windows-msvc/tests/unit/surface2d/Debug/test_hllc_wave_momentum.exe && build/windows-msvc/tests/unit/surface2d/Debug/test_hllc_wave_mass.exe
```

Expected: all regression subset tests PASS.

---

## Task 5: Run Surface2D and Full Validation

**Files:**
- No source changes expected unless failures identify M10 regressions.

- [ ] **Step 1: Build the full Debug tree**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --config Debug
```

Expected: build PASS.

- [ ] **Step 2: Run the Surface2D subset through CTest**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug -R "surface_state|surface_step|dpm_fields|hydrostatic_reconstruction|hllc_flux|phi_t_source|surface2d_golden_minimal|cfl_rollback|conservative_update|wetting_drying|boundary_flux|boundary_conditions|momentum_transport|pressure_momentum|hllc_wave_momentum|hllc_wave_mass" --output-on-failure
```

Expected: Surface2D subset PASS, now including `test_hllc_wave_mass`.

- [ ] **Step 3: Run full CTest**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure
```

Expected: all tests PASS.

---

## Task 6: Record M10 Evidence

**Files:**
- Create: `superpowers/specs/2026-05-18-plan-m10-surface2d-hllc-mass-flux-evidence.md`

- [ ] **Step 1: Create evidence file**

Create `superpowers/specs/2026-05-18-plan-m10-surface2d-hllc-mass-flux-evidence.md`:

```markdown
# M10 Surface2D HLLC Mass Flux Evidence

**Date:** 2026-05-18
**Plan:** `superpowers/plans/2026-05-18-plan-m10-surface2d-hllc-mass-flux.md`
**M9 Basis:** `superpowers/specs/2026-05-18-plan-m9-surface2d-hllc-wave-speed-momentum-evidence.md`

## Scope

M10 replaces the centered normal mass-flux diagnostic with an HLLC wave-speed branch-selected mass flux in the CPU reference `Surface2DCore` edge flux path. It preserves M9 HLLC normal momentum flux behavior, wet/dry blocking, DPM edge blocking, Wall/Open boundary behavior, CFL diagnostics, and momentum residual projection semantics.

## Local Validation

- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_hllc_flux test_hllc_wave_momentum test_hllc_wave_mass --config Debug`: PASS
- M10 target tests: PASS, `test_hllc_flux`, `test_hllc_wave_momentum`, and `test_hllc_wave_mass` passed
- M7/M8/M9/M10 regression subset: PASS, `test_hllc_flux`, `test_momentum_transport`, `test_pressure_momentum`, `test_hllc_wave_momentum`, and `test_hllc_wave_mass` passed
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --config Debug`: PASS
- Surface2D subset 16/16 PASS (`surface_state`, `surface_step`, `dpm_fields`, `hydrostatic_reconstruction`, `hllc_flux`, `phi_t_source`, `surface2d_golden_minimal`, `cfl_rollback`, `conservative_update`, `wetting_drying`, `boundary_flux`, `boundary_conditions`, `momentum_transport`, `pressure_momentum`, `hllc_wave_momentum`, `hllc_wave_mass`)
- Full CTest: PASS, update with observed total count

## Coverage

- `test_hllc_flux`: HLLC branch-selected mass flux for one-sided right-going flow and `phi_e_n` scaling after branch selection.
- `test_hllc_wave_mass`: internal-edge mass sensitivity, dry internal-edge blocking, DPM edge blocking, Wall zero contribution, and Open boundary wave mass contribution.
- Existing M3/M4/M5/M6/M7/M8/M9 Surface2D tests remain passing.

## Boundaries

- `Surface2DCore` remains independent of SWMM, D-Flow FM, `CouplingLib`, and 1D engine ABIs.
- M10 remains limited to the CPU reference HLLC edge flux path.
- M10 does not introduce CUDA backends, prescribed external boundary states, coupling ledger behavior, GoldenSuite release claims, or 1D engine interface changes.
```

Update the full CTest count with the actual observed result before committing.

- [ ] **Step 2: Review changed files**

Run:

```bash
git status --short && git diff -- libs/surface2d/src/riemann/hllc.cpp tests/unit/surface2d/test_hllc_flux.cpp tests/unit/surface2d/test_hllc_wave_mass.cpp tests/unit/surface2d/CMakeLists.txt superpowers/specs/2026-05-18-plan-m10-surface2d-hllc-mass-flux-evidence.md
```

Expected:
- Only M10 source, test, CMake, and evidence files changed.
- No `Surface2DCore` dependency on SWMM, D-Flow FM, `CouplingLib`, or 1D engine ABIs.

- [ ] **Step 3: Commit implementation**

Run:

```bash
git add libs/surface2d/src/riemann/hllc.cpp tests/unit/surface2d/test_hllc_flux.cpp tests/unit/surface2d/test_hllc_wave_mass.cpp tests/unit/surface2d/CMakeLists.txt superpowers/specs/2026-05-18-plan-m10-surface2d-hllc-mass-flux-evidence.md
git commit -m "feat(surface2d): align HLLC mass flux with wave speeds"
```

Expected: commit succeeds after all validation is recorded.

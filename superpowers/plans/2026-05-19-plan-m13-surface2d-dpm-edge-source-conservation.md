# M13 Surface2D DPM Edge/Source Conservation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Preserve `Surface2DCore` independence from SWMM, D-Flow FM, `CouplingLib`, and 1D engine ABIs. Keep M13 limited to focused CPU reference edge/source conservation regressions.

**Goal:** Add minimal, focused regression coverage proving that DPM edge transport diagnostics and `phi_t` source-pairing diagnostics form a coherent one-step internal-edge conservation picture.

**Architecture:** M13 treats one isolated internal edge in `build_mixed_minimal_mesh()` as the smallest conservation unit. The new tests close every DPM edge except the selected internal edge, then verify active-edge antisymmetry, exact `pressure_pairing + s_phi_t` cancellation, and zero transport/residual behavior for `omega_edge` and `phi_e_n` blocking. Existing `advance_one_step_cpu(...)`, `DpmFields`, `StepDiagnostics`, and HLLC edge flux code are expected to satisfy the slice without new production numerical machinery.

**Tech Stack:** C++20, CMake, GoogleTest, existing `scau::mesh` and `scau::surface2d` targets.

---

## File Structure

- Create `tests/unit/surface2d/test_dpm_edge_source_conservation.cpp`
  - Defines focused M13 regression tests for isolated internal-edge active transport, `omega_edge` hard blocking, and `phi_e_n` soft blocking.
  - Uses local helpers already patterned after existing Surface2D tests: `first_internal_edge_index(...)`, `cell_index_by_id(...)`, and `isolate_edge(...)`.
- Modify `tests/unit/surface2d/CMakeLists.txt`
  - Adds the dedicated `test_dpm_edge_source_conservation` GoogleTest target and CTest registration.
- Create `superpowers/specs/2026-05-19-plan-m13-surface2d-dpm-edge-source-conservation-evidence.md`
  - Records focused M13 target results, Surface2D regression subset, full Debug build, and full CTest evidence.
- No production source changes are expected for M13.
  - If the new tests fail, fix only the specific existing Surface2D edge residual or HLLC DPM blocking path required by the failing assertion.
  - Do not add new DPM field names, rollback/snapshot/replay behavior, global mass audit infrastructure, CUDA code, adaptive timestep selection, `CouplingLib`, SWMM, D-Flow FM, or 1D engine interfaces.

---

## Task 1: Add Focused DPM Edge/Source Conservation Tests

**Files:**
- Create: `tests/unit/surface2d/test_dpm_edge_source_conservation.cpp`
- Modify: `tests/unit/surface2d/CMakeLists.txt`

- [ ] **Step 1: Create the focused test file**

Create `tests/unit/surface2d/test_dpm_edge_source_conservation.cpp` with this content:

```cpp
#include <cstddef>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/riemann/hllc.hpp"
#include "surface2d/state/state.hpp"
#include "surface2d/time_integration/step.hpp"

namespace {

std::size_t first_internal_edge_index(const scau::mesh::Mesh& mesh) {
    for (std::size_t index = 0; index < mesh.edges.size(); ++index) {
        if (mesh.edges[index].left_cell.has_value() && mesh.edges[index].right_cell.has_value()) {
            return index;
        }
    }
    throw std::invalid_argument("mesh must contain an internal edge");
}

std::size_t cell_index_by_id(const scau::mesh::Mesh& mesh, const std::string& cell_id) {
    for (std::size_t index = 0; index < mesh.cells.size(); ++index) {
        if (mesh.cells[index].id == cell_id) {
            return index;
        }
    }
    throw std::invalid_argument("mesh must contain referenced cell");
}

void isolate_edge(scau::surface2d::DpmFields& dpm_fields, std::size_t edge_index) {
    for (auto& edge_fields : dpm_fields.edges) {
        edge_fields.phi_e_n = 0.0;
        edge_fields.omega_edge = 0.0;
    }
    dpm_fields.edges[edge_index].phi_e_n = 1.0;
    dpm_fields.edges[edge_index].omega_edge = 1.0;
}

void set_edge_aligned_state(
    const scau::mesh::Mesh& mesh,
    std::size_t edge_index,
    scau::surface2d::SurfaceState& state,
    std::size_t left_index,
    std::size_t right_index) {
    const auto& normal = mesh.edges[edge_index].normal;
    const scau::surface2d::Normal2 tangent{.x = -normal.y, .y = normal.x};

    const double left_h = 1.20;
    const double right_h = 0.95;
    const double left_un = 0.70;
    const double right_un = -0.10;
    const double left_ut = 0.20;
    const double right_ut = -0.05;

    state.cells[left_index].conserved.h = left_h;
    state.cells[left_index].conserved.hu = left_h * (left_un * normal.x + left_ut * tangent.x);
    state.cells[left_index].conserved.hv = left_h * (left_un * normal.y + left_ut * tangent.y);
    state.cells[left_index].eta = 1.20;

    state.cells[right_index].conserved.h = right_h;
    state.cells[right_index].conserved.hu = right_h * (right_un * normal.x + right_ut * tangent.x);
    state.cells[right_index].conserved.hv = right_h * (right_un * normal.y + right_ut * tangent.y);
    state.cells[right_index].eta = 1.20;
}

void expect_zero_cell_residual(
    const scau::surface2d::StepDiagnostics& diagnostics,
    std::size_t cell_index) {
    EXPECT_EQ(diagnostics.cells[cell_index].mass_residual, 0.0);
    EXPECT_EQ(diagnostics.cells[cell_index].momentum_residual.x, 0.0);
    EXPECT_EQ(diagnostics.cells[cell_index].momentum_residual.y, 0.0);
}

}  // namespace

TEST(SurfaceDpmEdgeSourceConservation, ActiveInternalEdgeClosesMassMomentumAndPhiTSourcePairing) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto edge_index = first_internal_edge_index(mesh);
    const auto left_index = cell_index_by_id(mesh, *mesh.edges[edge_index].left_cell);
    const auto right_index = cell_index_by_id(mesh, *mesh.edges[edge_index].right_cell);

    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    set_edge_aligned_state(mesh, edge_index, state, left_index, right_index);

    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    isolate_edge(dpm_fields, edge_index);
    dpm_fields.cells[left_index].phi_t = 1.0;
    dpm_fields.cells[right_index].phi_t = 1.35;

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 20.0};
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields);

    ASSERT_EQ(diagnostics.edges.size(), mesh.edges.size());
    ASSERT_EQ(diagnostics.cells.size(), mesh.cells.size());

    EXPECT_NE(diagnostics.edges[edge_index].mass_flux, 0.0);
    EXPECT_NE(diagnostics.edges[edge_index].momentum_flux_n, 0.0);
    EXPECT_NE(diagnostics.edges[edge_index].pressure_pairing, 0.0);
    EXPECT_DOUBLE_EQ(
        diagnostics.edges[edge_index].pressure_pairing + diagnostics.edges[edge_index].s_phi_t,
        0.0);
    EXPECT_DOUBLE_EQ(diagnostics.edges[edge_index].residual, 0.0);

    EXPECT_NE(diagnostics.cells[left_index].mass_residual, 0.0);
    EXPECT_DOUBLE_EQ(
        diagnostics.cells[left_index].mass_residual,
        -diagnostics.cells[right_index].mass_residual);

    const double left_momentum_norm =
        diagnostics.cells[left_index].momentum_residual.x * diagnostics.cells[left_index].momentum_residual.x +
        diagnostics.cells[left_index].momentum_residual.y * diagnostics.cells[left_index].momentum_residual.y;
    EXPECT_NE(left_momentum_norm, 0.0);
    EXPECT_DOUBLE_EQ(
        diagnostics.cells[left_index].momentum_residual.x,
        -diagnostics.cells[right_index].momentum_residual.x);
    EXPECT_DOUBLE_EQ(
        diagnostics.cells[left_index].momentum_residual.y,
        -diagnostics.cells[right_index].momentum_residual.y);
}

TEST(SurfaceDpmEdgeSourceConservation, OmegaEdgeHardBlockZerosTransportResiduals) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto edge_index = first_internal_edge_index(mesh);
    const auto left_index = cell_index_by_id(mesh, *mesh.edges[edge_index].left_cell);
    const auto right_index = cell_index_by_id(mesh, *mesh.edges[edge_index].right_cell);

    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    set_edge_aligned_state(mesh, edge_index, state, left_index, right_index);

    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    isolate_edge(dpm_fields, edge_index);
    dpm_fields.edges[edge_index].omega_edge = 0.0;

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 20.0};
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields);

    ASSERT_EQ(diagnostics.edges.size(), mesh.edges.size());
    EXPECT_EQ(diagnostics.edges[edge_index].mass_flux, 0.0);
    EXPECT_EQ(diagnostics.edges[edge_index].momentum_flux_n, 0.0);
    expect_zero_cell_residual(diagnostics, left_index);
    expect_zero_cell_residual(diagnostics, right_index);
}

TEST(SurfaceDpmEdgeSourceConservation, PhiENSoftBlockZerosTransportResiduals) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto edge_index = first_internal_edge_index(mesh);
    const auto left_index = cell_index_by_id(mesh, *mesh.edges[edge_index].left_cell);
    const auto right_index = cell_index_by_id(mesh, *mesh.edges[edge_index].right_cell);

    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    set_edge_aligned_state(mesh, edge_index, state, left_index, right_index);

    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    isolate_edge(dpm_fields, edge_index);
    dpm_fields.edges[edge_index].phi_e_n = scau::surface2d::PhiEdgeMin * 0.5;

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 20.0};
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields);

    ASSERT_EQ(diagnostics.edges.size(), mesh.edges.size());
    EXPECT_EQ(diagnostics.edges[edge_index].mass_flux, 0.0);
    EXPECT_EQ(diagnostics.edges[edge_index].momentum_flux_n, 0.0);
    expect_zero_cell_residual(diagnostics, left_index);
    expect_zero_cell_residual(diagnostics, right_index);
}
```

- [ ] **Step 2: Wire the dedicated test target into CMake**

In `tests/unit/surface2d/CMakeLists.txt`, add this block after the `test_cfl_rollback` registration:

```cmake
add_executable(test_dpm_edge_source_conservation test_dpm_edge_source_conservation.cpp)
target_link_libraries(test_dpm_edge_source_conservation
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_dpm_edge_source_conservation COMMAND test_dpm_edge_source_conservation)
```

- [ ] **Step 3: Build and run the focused M13 target**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_dpm_edge_source_conservation --config Debug && build/windows-msvc/tests/unit/surface2d/Debug/test_dpm_edge_source_conservation.exe
```

Expected: build succeeds and the new test binary reports `3 tests` passed.

---

## Task 2: Run M13 Surface2D Regression Validation

**Files:**
- No source changes expected.

- [ ] **Step 1: Build full Debug tree**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --config Debug
```

Expected: Debug build succeeds.

- [ ] **Step 2: Run focused Surface2D regression subset**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug -R "surface_state|surface_step|dpm_fields|hydrostatic_reconstruction|hllc_flux|phi_t_source|surface2d_golden_minimal|cfl_rollback|dpm_edge_source_conservation|conservative_update|wetting_drying|boundary_flux|boundary_conditions|momentum_transport|pressure_momentum|hllc_wave_momentum|hllc_wave_mass" --output-on-failure
```

Expected: all Surface2D-related CTest entries pass, including the new `test_dpm_edge_source_conservation` target.

- [ ] **Step 3: Run full CTest**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure
```

Expected: full CTest passes.

---

## Task 3: Record M13 Evidence

**Files:**
- Create: `superpowers/specs/2026-05-19-plan-m13-surface2d-dpm-edge-source-conservation-evidence.md`

- [ ] **Step 1: Create the evidence file after validation passes**

Create `superpowers/specs/2026-05-19-plan-m13-surface2d-dpm-edge-source-conservation-evidence.md` with this content after the Task 1 and Task 2 commands have passed:

```markdown
# M13 Surface2D DPM Edge/Source Conservation Evidence

**Date:** 2026-05-19
**Design:** `superpowers/specs/2026-05-19-m13-surface2d-dpm-edge-source-conservation-design.md`
**Plan:** `superpowers/plans/2026-05-19-plan-m13-surface2d-dpm-edge-source-conservation.md`
**M12 Basis:** `superpowers/specs/2026-05-18-plan-m12-surface2d-edge-hllc-cfl-diagnostic-evidence.md`

## Scope

M13 adds focused CPU reference `Surface2DCore` regression coverage proving that isolated internal-edge HLLC transport diagnostics, DPM edge blocking fields, and `phi_t` source-pairing diagnostics participate coherently in one-step edge/source conservation checks.

## Local Validation

- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_dpm_edge_source_conservation --config Debug && build/windows-msvc/tests/unit/surface2d/Debug/test_dpm_edge_source_conservation.exe`: PASS, 3/3 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --config Debug`: PASS.
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug -R "surface_state|surface_step|dpm_fields|hydrostatic_reconstruction|hllc_flux|phi_t_source|surface2d_golden_minimal|cfl_rollback|dpm_edge_source_conservation|conservative_update|wetting_drying|boundary_flux|boundary_conditions|momentum_transport|pressure_momentum|hllc_wave_momentum|hllc_wave_mass" --output-on-failure`: PASS, 17/17 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure`: PASS, 21/21 tests passed.

## Coverage

- `test_dpm_edge_source_conservation`: active isolated internal edge has equal-and-opposite left/right mass residuals, equal-and-opposite left/right Cartesian momentum residual vectors, nonzero transport, and exact `pressure_pairing + s_phi_t == 0` closure.
- `test_dpm_edge_source_conservation`: `omega_edge == 0.0` hard block zeros edge mass and momentum transport diagnostics and leaves adjacent cell mass/momentum residuals at zero for the isolated edge.
- `test_dpm_edge_source_conservation`: `phi_e_n < PhiEdgeMin` soft block zeros edge mass and momentum transport diagnostics and leaves adjacent cell mass/momentum residuals at zero for the isolated edge.
- Existing `test_phi_t_source`, `test_pressure_momentum`, `test_surface_step`, `test_surface2d_golden_minimal`, DPM, HLLC, boundary, wetting/drying, conservative update, and CFL rollback regressions remain passing.

## Boundaries

- `Surface2DCore` remains independent of SWMM, D-Flow FM, `CouplingLib`, and 1D engine ABIs.
- M13 does not add new DPM field names or aliases; `phi_t`, `phi_e_n`, and `omega_edge` keep distinct semantics.
- M13 does not change M12 `max_cell_cfl` semantics.
- M13 does not add rollback, snapshot, replay, `mass_deficit_account`, global mass audit infrastructure, CUDA backends, adaptive timestep selection, coupling ledger behavior, GoldenSuite release claims, or 1D engine interfaces.
```

---

## Task 4: Review Diff And Commit M13

**Files:**
- `tests/unit/surface2d/test_dpm_edge_source_conservation.cpp`
- `tests/unit/surface2d/CMakeLists.txt`
- `superpowers/plans/2026-05-19-plan-m13-surface2d-dpm-edge-source-conservation.md`
- `superpowers/specs/2026-05-19-plan-m13-surface2d-dpm-edge-source-conservation-evidence.md`

- [ ] **Step 1: Review working tree status and M13 diff**

Run:

```bash
git status --short && git diff -- tests/unit/surface2d/test_dpm_edge_source_conservation.cpp tests/unit/surface2d/CMakeLists.txt superpowers/plans/2026-05-19-plan-m13-surface2d-dpm-edge-source-conservation.md superpowers/specs/2026-05-19-plan-m13-surface2d-dpm-edge-source-conservation-evidence.md
```

Expected:
- Only M13 test, CMake, plan, and evidence files are changed.
- No production source file is changed unless a new test exposed a real M13 gap.
- No dependency on SWMM, D-Flow FM, `CouplingLib`, CUDA, rollback/snapshot/replay, or 1D engine interfaces appears in the diff.

- [ ] **Step 2: Commit M13 implementation**

Run:

```bash
git add tests/unit/surface2d/test_dpm_edge_source_conservation.cpp tests/unit/surface2d/CMakeLists.txt superpowers/plans/2026-05-19-plan-m13-surface2d-dpm-edge-source-conservation.md superpowers/specs/2026-05-19-plan-m13-surface2d-dpm-edge-source-conservation-evidence.md
git commit -m "test(surface2d): cover dpm edge source conservation"
```

Expected: commit succeeds after all validation is recorded.

# M9 Surface2D HLLC Wave-Speed Momentum Implementation Plan

> **For agentic workers:** Execute this plan task-by-task. Preserve `Surface2DCore` independence from SWMM, D-Flow FM, `CouplingLib`, and 1D engine ABIs. Keep M9 limited to the CPU reference HLLC edge flux path. Do NOT introduce CUDA backends, prescribed external boundary states, coupling ledger behavior, GoldenSuite release claims, or changes to 1D engine interfaces in this slice.

**Goal:** Replace the M8 pressure-only normal momentum diagnostic with a minimal HLLC wave-speed-based normal momentum flux while preserving existing mass flux, wet/dry, boundary, and momentum residual behavior.

**Architecture:** M9 stays inside `libs/surface2d` and extends the existing `hllc_normal_flux(...)` API without changing callers. The implementation computes hydrostatic reconstructed left/right states, estimates shallow-water signal speeds `s_l`, `s_r`, and `s_star` in the edge-normal direction, then uses those speeds to return a physically directional HLLC-style normal momentum flux. The finite-volume step continues to consume `EdgeFlux::mass` and `EdgeFlux::momentum_n` exactly as M7/M8 established, so `step.cpp` should only need regression-preserving adjustments if tests expose a sign or dry-edge mismatch.

**Tech Stack:** C++20, CMake, GoogleTest, existing `scau::core`, `scau::mesh`, and `scau::surface2d` targets.

---

## File Structure

- Modify `libs/surface2d/include/surface2d/riemann/hllc.hpp`
  - Add a small `WaveSpeeds` value type and helper declaration if the tests need direct wave-speed coverage.
  - Keep `hllc_normal_flux(...)` signature stable for `step.cpp` and existing tests.
- Modify `libs/surface2d/src/riemann/hllc.cpp`
  - Add local shallow-water wave-speed estimation from reconstructed states.
  - Compute normal momentum flux from the HLLC branch selected by `s_l`, `s_star`, and `s_r`.
  - Keep DPM hard/soft blocking and dry reconstructed-depth blocking unchanged.
- Modify `tests/unit/surface2d/test_hllc_flux.cpp`
  - Update the static hydrostatic expectation from zero momentum to non-zero pressure momentum, matching M8 behavior.
  - Add direct wave-speed and directional momentum-flux tests.
- Create `tests/unit/surface2d/test_hllc_wave_momentum.cpp`
  - Cover M9 step-level behavior through `advance_one_step_cpu(...)`: directional flux changes with velocity, dry edges block, Wall edges still contribute zero, Open edges still use the Open ghost path.
- Modify `tests/unit/surface2d/CMakeLists.txt`
  - Register `test_hllc_wave_momentum`.
- Create `superpowers/specs/2026-05-17-plan-m9-surface2d-hllc-wave-speed-momentum-evidence.md`
  - Record build, Surface2D subset, and full CTest evidence after implementation.

---

## Task 1: Add HLLC Flux-Level Wave-Speed Tests

**Files:**
- Modify: `tests/unit/surface2d/test_hllc_flux.cpp`

- [ ] **Step 1: Update the static hydrostatic momentum expectation**

Replace the old M3 expectation that static hydrostatic momentum is zero with the M8/M9 pressure baseline:

```cpp
TEST(HllcFlux, ZeroVelocityHydrostaticStateHasZeroMassFluxAndPressureMomentum) {
    const scau::surface2d::CellState left{.conserved = {.h = 1.0, .hu = 0.0, .hv = 0.0}, .eta = 1.0};
    const scau::surface2d::CellState right{.conserved = {.h = 1.0, .hu = 0.0, .hv = 0.0}, .eta = 1.0};
    const scau::surface2d::EdgeDpmFields edge_fields{.phi_e_n = 1.0, .omega_edge = 1.0};

    const auto flux = scau::surface2d::hllc_normal_flux(
        left,
        right,
        edge_fields,
        scau::surface2d::Normal2{.x = 1.0, .y = 0.0});

    EXPECT_EQ(flux.mass, 0.0);
    EXPECT_DOUBLE_EQ(flux.momentum_n, 4.905);
}
```

- [ ] **Step 2: Add a direct wave-speed estimate test**

Append this test to `tests/unit/surface2d/test_hllc_flux.cpp` after the hydrostatic test:

```cpp
TEST(HllcFlux, WaveSpeedsBracketLeftAndRightNormalVelocities) {
    const scau::surface2d::CellState left{.conserved = {.h = 4.0, .hu = 8.0, .hv = 0.0}, .eta = 4.0};
    const scau::surface2d::CellState right{.conserved = {.h = 1.0, .hu = -1.0, .hv = 0.0}, .eta = 1.0};

    const auto speeds = scau::surface2d::estimate_hllc_wave_speeds(
        left,
        right,
        scau::surface2d::Normal2{.x = 1.0, .y = 0.0});

    EXPECT_LT(speeds.s_l, scau::surface2d::normal_velocity(left, scau::surface2d::Normal2{.x = 1.0, .y = 0.0}));
    EXPECT_GT(speeds.s_r, scau::surface2d::normal_velocity(right, scau::surface2d::Normal2{.x = 1.0, .y = 0.0}));
    EXPECT_LE(speeds.s_l, speeds.s_star);
    EXPECT_LE(speeds.s_star, speeds.s_r);
}
```

- [ ] **Step 3: Add a directional normal momentum flux test**

Append this test to `tests/unit/surface2d/test_hllc_flux.cpp`:

```cpp
TEST(HllcFlux, NormalMomentumFluxDependsOnVelocityDirection) {
    const scau::surface2d::CellState still_left{.conserved = {.h = 1.0, .hu = 0.0, .hv = 0.0}, .eta = 1.0};
    const scau::surface2d::CellState still_right{.conserved = {.h = 1.0, .hu = 0.0, .hv = 0.0}, .eta = 1.0};
    const scau::surface2d::CellState moving_left{.conserved = {.h = 1.0, .hu = 2.0, .hv = 0.0}, .eta = 1.0};
    const scau::surface2d::CellState moving_right{.conserved = {.h = 1.0, .hu = 2.0, .hv = 0.0}, .eta = 1.0};
    const scau::surface2d::EdgeDpmFields edge_fields{.phi_e_n = 1.0, .omega_edge = 1.0};
    const auto normal = scau::surface2d::Normal2{.x = 1.0, .y = 0.0};

    const auto still_flux = scau::surface2d::hllc_normal_flux(still_left, still_right, edge_fields, normal);
    const auto moving_flux = scau::surface2d::hllc_normal_flux(moving_left, moving_right, edge_fields, normal);

    EXPECT_GT(moving_flux.momentum_n, still_flux.momentum_n);
    EXPECT_GT(moving_flux.mass, still_flux.mass);
}
```

- [ ] **Step 4: Run the target test and confirm expected failure**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_hllc_flux && build/windows-msvc/tests/unit/surface2d/Debug/test_hllc_flux.exe
```

Expected before implementation:
- Compile FAIL because `estimate_hllc_wave_speeds` and `WaveSpeeds` do not exist.
- If the helper declaration is temporarily stubbed, the directional momentum test should FAIL because `momentum_n` is still pressure-only for equal-depth moving states.

---

## Task 2: Add Step-Level Wave Momentum Tests

**Files:**
- Create: `tests/unit/surface2d/test_hllc_wave_momentum.cpp`
- Modify: `tests/unit/surface2d/CMakeLists.txt`

- [ ] **Step 1: Create step-level tests**

Create `tests/unit/surface2d/test_hllc_wave_momentum.cpp`:

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

core::Real residual_norm_squared(const scau::surface2d::MomentumResidual& residual) {
    return residual.x * residual.x + residual.y * residual.y;
}

}  // namespace

TEST(SurfaceHllcWaveMomentum, InternalMomentumFluxIncreasesWithNormalVelocity) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto edge_index = first_internal_edge_index(mesh);
    const auto left_index = cell_index_by_id(mesh, *mesh.edges[edge_index].left_cell);

    auto still_state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    auto moving_state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    moving_state.cells[left_index].conserved.hu = 2.0;
    moving_state.cells[left_index].conserved.hv = 2.0;

    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    isolate_edge(dpm_fields, edge_index);

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 10.0};
    const auto still_diagnostics = scau::surface2d::advance_one_step_cpu(mesh, still_state, config, dpm_fields);
    const auto moving_diagnostics = scau::surface2d::advance_one_step_cpu(mesh, moving_state, config, dpm_fields);

    EXPECT_GT(
        residual_norm_squared(moving_diagnostics.cells[left_index].momentum_residual),
        residual_norm_squared(still_diagnostics.cells[left_index].momentum_residual));
}

TEST(SurfaceHllcWaveMomentum, DryInternalEdgeStillBlocksMomentumFlux) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto edge_index = first_internal_edge_index(mesh);
    const auto left_index = cell_index_by_id(mesh, *mesh.edges[edge_index].left_cell);
    const auto right_index = cell_index_by_id(mesh, *mesh.edges[edge_index].right_cell);

    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    state.cells[left_index].conserved.h = 1.0e-8;
    state.cells[right_index].conserved.hu = 2.0;
    state.cells[right_index].conserved.hv = 2.0;
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    isolate_edge(dpm_fields, edge_index);

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 10.0, .h_min = 1.0e-6};
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields);

    EXPECT_EQ(diagnostics.edges[edge_index].mass_flux, 0.0);
    EXPECT_EQ(diagnostics.edges[edge_index].momentum_flux_n, 0.0);
    EXPECT_EQ(residual_norm_squared(diagnostics.cells[left_index].momentum_residual), 0.0);
    EXPECT_EQ(residual_norm_squared(diagnostics.cells[right_index].momentum_residual), 0.0);
}

TEST(SurfaceHllcWaveMomentum, WallBoundaryStillContributesNoMomentumFlux) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto edge_index = first_boundary_edge_index(mesh);
    const auto cell_index = edge_cell_index(mesh, edge_index);

    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    state.cells[cell_index].conserved.hu = 2.0;
    state.cells[cell_index].conserved.hv = 2.0;
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    isolate_edge(dpm_fields, edge_index);
    auto boundary = scau::surface2d::BoundaryConditions::for_mesh(mesh);
    boundary.edges[edge_index] = scau::surface2d::BoundaryKind::Wall;

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 10.0};
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields, boundary);

    EXPECT_EQ(diagnostics.edges[edge_index].mass_flux, 0.0);
    EXPECT_EQ(diagnostics.edges[edge_index].momentum_flux_n, 0.0);
    EXPECT_EQ(residual_norm_squared(diagnostics.cells[cell_index].momentum_residual), 0.0);
}

TEST(SurfaceHllcWaveMomentum, OpenBoundaryUsesWaveMomentumFlux) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto edge_index = first_boundary_edge_index(mesh);
    const auto cell_index = edge_cell_index(mesh, edge_index);

    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    state.cells[cell_index].conserved.hu = 2.0;
    state.cells[cell_index].conserved.hv = 2.0;
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    isolate_edge(dpm_fields, edge_index);
    auto boundary = scau::surface2d::BoundaryConditions::for_mesh(mesh);
    boundary.edges[edge_index] = scau::surface2d::BoundaryKind::Open;

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 10.0};
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields, boundary);

    EXPECT_NE(diagnostics.edges[edge_index].momentum_flux_n, 0.0);
    EXPECT_NE(residual_norm_squared(diagnostics.cells[cell_index].momentum_residual), 0.0);
}
```

- [ ] **Step 2: Register the step-level test**

Append to `tests/unit/surface2d/CMakeLists.txt`:

```cmake
add_executable(test_hllc_wave_momentum test_hllc_wave_momentum.cpp)
target_link_libraries(test_hllc_wave_momentum
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_hllc_wave_momentum COMMAND test_hllc_wave_momentum)
```

- [ ] **Step 3: Run the target test and confirm expected failure**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_hllc_wave_momentum && build/windows-msvc/tests/unit/surface2d/Debug/test_hllc_wave_momentum.exe
```

Expected before implementation:
- Build PASS after registration.
- At least `InternalMomentumFluxIncreasesWithNormalVelocity` FAILS because M8 pressure-only `momentum_flux_n` does not increase for equal-depth moving states.

---

## Task 3: Implement Minimal HLLC Wave Speeds

**Files:**
- Modify: `libs/surface2d/include/surface2d/riemann/hllc.hpp`
- Modify: `libs/surface2d/src/riemann/hllc.cpp`

- [ ] **Step 1: Declare wave-speed diagnostics**

Add this value type and declaration to `libs/surface2d/include/surface2d/riemann/hllc.hpp` after `struct EdgeFlux`:

```cpp
struct WaveSpeeds {
    core::Real s_l{0.0};
    core::Real s_star{0.0};
    core::Real s_r{0.0};
};

[[nodiscard]] WaveSpeeds estimate_hllc_wave_speeds(
    const CellState& left,
    const CellState& right,
    Normal2 normal,
    core::Real gravity = 9.81);
```

- [ ] **Step 2: Implement wave-speed estimation**

Add these helpers to the anonymous namespace in `libs/surface2d/src/riemann/hllc.cpp`:

```cpp
core::Real wave_celerity(const CellState& state, core::Real gravity) {
    return std::sqrt(gravity * state.conserved.h);
}

core::Real physical_normal_momentum_flux(const CellState& state, Normal2 normal, core::Real gravity) {
    const core::Real un = normal_velocity(state, normal);
    return state.conserved.h * un * un + 0.5 * gravity * state.conserved.h * state.conserved.h;
}
```

Add `#include <algorithm>` and `#include <cmath>` at the top of `hllc.cpp`.

Then implement the public helper after `normal_velocity(...)`:

```cpp
WaveSpeeds estimate_hllc_wave_speeds(
    const CellState& left,
    const CellState& right,
    Normal2 normal,
    core::Real gravity) {
    const core::Real left_un = normal_velocity(left, normal);
    const core::Real right_un = normal_velocity(right, normal);
    const core::Real left_c = wave_celerity(left, gravity);
    const core::Real right_c = wave_celerity(right, gravity);
    const core::Real s_l = std::min(left_un - left_c, right_un - right_c);
    const core::Real s_r = std::max(left_un + left_c, right_un + right_c);

    const core::Real numerator =
        right.conserved.h * right_un * (s_r - right_un) -
        left.conserved.h * left_un * (s_l - left_un) +
        physical_normal_momentum_flux(left, normal, gravity) -
        physical_normal_momentum_flux(right, normal, gravity);
    const core::Real denominator =
        right.conserved.h * (s_r - right_un) - left.conserved.h * (s_l - left_un);
    const core::Real s_star = denominator != 0.0 ? numerator / denominator : 0.0;

    return WaveSpeeds{.s_l = s_l, .s_star = s_star, .s_r = s_r};
}
```

- [ ] **Step 3: Run flux test compile check**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_hllc_flux
```

Expected: build PASS, but directional flux test still FAILS until `hllc_normal_flux(...)` consumes the wave speeds.

---

## Task 4: Use HLLC Branching for Normal Momentum Flux

**Files:**
- Modify: `libs/surface2d/src/riemann/hllc.cpp`

- [ ] **Step 1: Add HLLC star momentum helper**

Add this helper to the anonymous namespace in `hllc.cpp`:

```cpp
core::Real hllc_star_normal_momentum_flux(
    const CellState& state,
    Normal2 normal,
    core::Real s_k,
    core::Real s_star,
    core::Real gravity) {
    const core::Real un = normal_velocity(state, normal);
    const core::Real denominator = s_k - s_star;
    if (denominator == 0.0) {
        return physical_normal_momentum_flux(state, normal, gravity);
    }
    const core::Real h_star = state.conserved.h * (s_k - un) / denominator;
    const core::Real momentum_star = h_star * s_star;
    return physical_normal_momentum_flux(state, normal, gravity) + s_k * (momentum_star - state.conserved.h * un);
}
```

- [ ] **Step 2: Replace pressure-only momentum assignment**

In `hllc_normal_flux(...)`, keep the existing DPM and dry guards, keep the M8 mass formula, and replace the `momentum_n` calculation with:

```cpp
    const auto speeds = estimate_hllc_wave_speeds(pair.left, pair.right, normal);
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

- [ ] **Step 3: Remove the M8 pressure-only helper if unused**

Remove `pressure_normal_momentum(...)` from `hllc.cpp` if it has no remaining callers.

- [ ] **Step 4: Run flux-level tests**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_hllc_flux && build/windows-msvc/tests/unit/surface2d/Debug/test_hllc_flux.exe
```

Expected: PASS.

---

## Task 5: Preserve Step-Level Momentum Behavior

**Files:**
- Modify: `libs/surface2d/src/time_integration/step.cpp` only if target tests expose a sign or accumulation issue.
- Modify: `tests/unit/surface2d/test_pressure_momentum.cpp` only if an assertion is pressure-only-specific rather than behavior-specific.

- [ ] **Step 1: Run wave-momentum step tests**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_hllc_wave_momentum && build/windows-msvc/tests/unit/surface2d/Debug/test_hllc_wave_momentum.exe
```

Expected: PASS after Task 4. If this fails, inspect only the sign convention between `momentum_flux_n`, edge normals, and `accumulate_pressure_momentum_residual(...)`.

- [ ] **Step 2: Run M7/M8 regression tests**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_momentum_transport test_pressure_momentum && build/windows-msvc/tests/unit/surface2d/Debug/test_momentum_transport.exe && build/windows-msvc/tests/unit/surface2d/Debug/test_pressure_momentum.exe
```

Expected: PASS. The M8 tests should continue to assert non-zero `momentum_flux_n`, dry-side blocking, Wall zero contribution, Open contribution, and M7 transport regression without assuming pressure-only magnitudes.

- [ ] **Step 3: Keep `step.cpp` unchanged if regressions pass**

Do not refactor `step.cpp` if the tests pass. M9 is a Riemann flux improvement, not a time-integration restructuring.

---

## Task 6: Register and Run Full M9 Validation

**Files:**
- Modify: `tests/unit/surface2d/CMakeLists.txt`

- [ ] **Step 1: Confirm the new target is registered**

Ensure `tests/unit/surface2d/CMakeLists.txt` contains:

```cmake
add_executable(test_hllc_wave_momentum test_hllc_wave_momentum.cpp)
target_link_libraries(test_hllc_wave_momentum
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_hllc_wave_momentum COMMAND test_hllc_wave_momentum)
```

- [ ] **Step 2: Run the Windows build**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc
```

Expected: PASS.

- [ ] **Step 3: Run the Surface2D subset**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug -R "surface_state|surface_step|dpm_fields|hydrostatic_reconstruction|hllc_flux|phi_t_source|surface2d_golden_minimal|cfl_rollback|conservative_update|wetting_drying|boundary_flux|boundary_conditions|momentum_transport|pressure_momentum|hllc_wave_momentum" --output-on-failure
```

Expected: 15/15 Surface2D tests PASS.

- [ ] **Step 4: Run full CTest**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure
```

Expected: 19/19 tests PASS.

---

## Task 7: Record M9 Evidence and Commit

**Files:**
- Create: `superpowers/specs/2026-05-17-plan-m9-surface2d-hllc-wave-speed-momentum-evidence.md`

- [ ] **Step 1: Create evidence document**

Create `superpowers/specs/2026-05-17-plan-m9-surface2d-hllc-wave-speed-momentum-evidence.md`:

```markdown
# M9 Surface2D HLLC Wave-Speed Momentum Evidence

**Date:** 2026-05-17
**Plan:** `superpowers/plans/2026-05-17-plan-m9-surface2d-hllc-wave-speed-momentum.md`
**M8 Basis:** `superpowers/specs/2026-05-17-plan-m8-surface2d-pressure-momentum-evidence.md`

## Scope

M9 replaces the M8 pressure-only normal momentum diagnostic with a minimal HLLC wave-speed-based normal momentum flux in `Surface2DCore`. It keeps the existing HLLC mass flux signature, M7 upwind momentum transport, M8 pressure residual projection path, dry-side blocking, and M6 Wall/Open boundary semantics.

## Local Validation

- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc`: PASS
- Surface2D subset 15/15 PASS (`surface_state`, `surface_step`, `dpm_fields`, `hydrostatic_reconstruction`, `hllc_flux`, `phi_t_source`, `surface2d_golden_minimal`, `cfl_rollback`, `conservative_update`, `wetting_drying`, `boundary_flux`, `boundary_conditions`, `momentum_transport`, `pressure_momentum`, `hllc_wave_momentum`)
- Full CTest: PASS, 19/19 tests passed

## Coverage

- `test_hllc_flux`: wave-speed bracket coverage, static hydrostatic pressure baseline, directional normal momentum flux, and existing DPM block behavior.
- `test_hllc_wave_momentum`: step-level directional momentum residual behavior, dry edge blocking, Wall zero contribution, and Open boundary wave-momentum contribution.
- Existing M3/M4/M5/M6/M7/M8 Surface2D tests remain passing.

## Boundaries

- `Surface2DCore` remains independent of SWMM, D-Flow FM, `CouplingLib`, and 1D engine ABIs.
- CUDA backends, prescribed external boundary states, and coupling ledger behavior remain out of scope.
- This evidence does not claim GoldenSuite release readiness.
```

- [ ] **Step 2: Inspect git status**

Run:

```bash
git status --short
```

Expected changed files:
- `libs/surface2d/include/surface2d/riemann/hllc.hpp`
- `libs/surface2d/src/riemann/hllc.cpp`
- `tests/unit/surface2d/test_hllc_flux.cpp`
- `tests/unit/surface2d/test_hllc_wave_momentum.cpp`
- `tests/unit/surface2d/CMakeLists.txt`
- `superpowers/plans/2026-05-17-plan-m9-surface2d-hllc-wave-speed-momentum.md`
- `superpowers/specs/2026-05-17-plan-m9-surface2d-hllc-wave-speed-momentum-evidence.md`

- [ ] **Step 3: Commit M9 plan separately before implementation if following the M6/M7/M8 cadence**

Run before Task 1 implementation if the plan is ready:

```bash
git add superpowers/plans/2026-05-17-plan-m9-surface2d-hllc-wave-speed-momentum.md
git commit -m "docs(m9): plan hllc wave-speed momentum slice"
```

- [ ] **Step 4: Commit M9 implementation and evidence after validation**

Run after Tasks 1-6 and evidence are complete:

```bash
git add \
  libs/surface2d/include/surface2d/riemann/hllc.hpp \
  libs/surface2d/src/riemann/hllc.cpp \
  tests/unit/surface2d/test_hllc_flux.cpp \
  tests/unit/surface2d/test_hllc_wave_momentum.cpp \
  tests/unit/surface2d/CMakeLists.txt \
  superpowers/specs/2026-05-17-plan-m9-surface2d-hllc-wave-speed-momentum-evidence.md
git commit -m "feat(surface2d): add hllc wave-speed momentum flux"
```

---

## Self-Review

- Spec coverage: The plan covers a minimal HLLC wave-speed helper, branch-selected normal momentum flux, step-level regression behavior, validation, and evidence. It excludes CUDA, prescribed external boundary states, coupling ledger semantics, and GoldenSuite release readiness.
- Placeholder scan: No placeholders remain; all planned tests, code snippets, commands, expected failures, and evidence content are specified.
- Type consistency: `WaveSpeeds`, `estimate_hllc_wave_speeds(...)`, `Normal2`, `EdgeFlux::momentum_n`, and `MomentumResidual` names match the current Surface2D naming style and existing M7/M8 API surface.

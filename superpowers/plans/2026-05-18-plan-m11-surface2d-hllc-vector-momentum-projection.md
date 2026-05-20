# M11 Surface2D HLLC Vector Momentum Projection Implementation Plan

> **For agentic workers:** Execute this plan task-by-task. Preserve `Surface2DCore` independence from SWMM, D-Flow FM, `CouplingLib`, and 1D engine ABIs. Keep M11 limited to the CPU reference Surface2D edge-flux and momentum-residual path. Do NOT introduce CUDA backends, external prescribed boundary states, coupling ledger behavior, GoldenSuite release claims, or 1D engine interface changes in this slice.

**Goal:** Replace the current upwind-velocity momentum transport approximation with a vector momentum flux assembled from HLLC normal momentum and tangential advection, so the residual path uses the same HLLC edge branch basis for mass and normal momentum while preserving existing pressure, wet/dry, DPM, Wall/Open, CFL, and depth-update semantics.

**Architecture:** M11 extends the existing `EdgeFlux` DTO inside `surface2d/riemann/hllc.hpp` with `momentum_x` and `momentum_y`, computed in `hllc_normal_flux(...)` by decomposing each side into normal and tangential components. The normal component reuses existing `momentum_n`; the tangential component is advected with the branch-selected HLLC mass flux and the corresponding side/star tangential velocity. `advance_one_step_cpu(...)` then accumulates vector momentum residuals directly from the returned vector flux instead of multiplying mass flux by an upwind cell velocity.

**Tech Stack:** C++20, CMake, GoogleTest, existing `scau::core`, `scau::mesh`, and `scau::surface2d` targets.

---

## File Structure

- Modify `libs/surface2d/include/surface2d/riemann/hllc.hpp`
  - Extend `EdgeFlux` with `momentum_x` and `momentum_y`, defaulting to zero for backwards-compatible aggregate construction.
- Modify `libs/surface2d/src/riemann/hllc.cpp`
  - Add tangential projection helpers.
  - Compute branch-selected vector momentum flux from normal/tangential components.
  - Keep `mass` and `momentum_n` semantics unchanged.
- Modify `libs/surface2d/src/time_integration/step.cpp`
  - Replace upwind-velocity momentum residual accumulation with direct vector-flux residual accumulation.
  - Keep pressure residual accumulation separate and unchanged.
- Modify `tests/unit/surface2d/test_hllc_flux.cpp`
  - Add flux-level vector projection tests for normal-only and tangential-only velocity cases.
- Modify `tests/unit/surface2d/test_momentum_transport.cpp`
  - Add step-level regression proving vector momentum residual follows `EdgeFlux::momentum_x/y`, not the old upwind mass-flux approximation.
- Create `superpowers/specs/2026-05-18-plan-m11-surface2d-hllc-vector-momentum-projection-evidence.md`
  - Record target, Surface2D subset, and full CTest evidence after implementation.

---

## Task 1: Add Flux-Level Vector Momentum Tests

**Files:**
- Modify: `tests/unit/surface2d/test_hllc_flux.cpp`

- [ ] **Step 1: Add normal projection test**

Append after the existing M10 mass flux tests:

```cpp
TEST(HllcFlux, VectorMomentumProjectsNormalFluxOntoCartesianAxes) {
    const scau::surface2d::CellState left{.conserved = {.h = 1.0, .hu = 6.0, .hv = 0.0}, .eta = 1.0};
    const scau::surface2d::CellState right{.conserved = {.h = 1.0, .hu = 4.0, .hv = 0.0}, .eta = 1.0};
    const scau::surface2d::EdgeDpmFields edge_fields{.phi_e_n = 1.0, .omega_edge = 1.0};
    const auto normal = scau::surface2d::Normal2{.x = 1.0, .y = 0.0};

    const auto flux = scau::surface2d::hllc_normal_flux(left, right, edge_fields, normal);

    EXPECT_DOUBLE_EQ(flux.momentum_x, flux.momentum_n);
    EXPECT_DOUBLE_EQ(flux.momentum_y, 0.0);
}
```

Expected before implementation: compile failure because `EdgeFlux` has no `momentum_x` or `momentum_y` members.

- [ ] **Step 2: Add tangential advection test**

Append after the normal projection test:

```cpp
TEST(HllcFlux, VectorMomentumCarriesTangentialVelocityWithMassFlux) {
    const scau::surface2d::CellState left{.conserved = {.h = 1.0, .hu = 6.0, .hv = 2.0}, .eta = 1.0};
    const scau::surface2d::CellState right{.conserved = {.h = 1.0, .hu = 4.0, .hv = -3.0}, .eta = 1.0};
    const scau::surface2d::EdgeDpmFields edge_fields{.phi_e_n = 1.0, .omega_edge = 1.0};
    const auto normal = scau::surface2d::Normal2{.x = 1.0, .y = 0.0};

    const auto flux = scau::surface2d::hllc_normal_flux(left, right, edge_fields, normal);

    EXPECT_DOUBLE_EQ(flux.mass, 6.0);
    EXPECT_DOUBLE_EQ(flux.momentum_y, 12.0);
}
```

This uses the right-going branch from M10. The old upwind residual path could transport tangential momentum later, but the flux DTO itself does not expose vector momentum until M11.

- [ ] **Step 3: Run the red flux target**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_hllc_flux --config Debug
```

Expected before implementation: build FAILS on missing `momentum_x` / `momentum_y` members.

---

## Task 2: Extend EdgeFlux With Cartesian Momentum

**Files:**
- Modify: `libs/surface2d/include/surface2d/riemann/hllc.hpp`
- Modify: `libs/surface2d/src/riemann/hllc.cpp`

- [ ] **Step 1: Extend `EdgeFlux`**

Change `EdgeFlux` to:

```cpp
struct EdgeFlux {
    core::Real mass{0.0};
    core::Real momentum_n{0.0};
    core::Real momentum_x{0.0};
    core::Real momentum_y{0.0};
};
```

- [ ] **Step 2: Add tangential projection helpers**

Add in the anonymous namespace in `hllc.cpp`:

```cpp
Normal2 tangent_from_normal(Normal2 normal) {
    return Normal2{.x = -normal.y, .y = normal.x};
}

core::Real tangential_velocity(const CellState& state, Normal2 normal) {
    const auto tangent = tangent_from_normal(normal);
    return state.u() * tangent.x + state.v() * tangent.y;
}
```

- [ ] **Step 3: Track branch tangential velocity**

Inside `hllc_normal_flux(...)`, add `core::Real tangential = 0.0;` next to `mass` and `momentum_n`. Set it in each branch:

```cpp
if (0.0 <= speeds.s_l) {
    mass = physical_normal_mass_flux(pair.left, normal);
    momentum_n = physical_normal_momentum_flux(pair.left, normal, 9.81);
    tangential = tangential_velocity(pair.left, normal);
} else if (speeds.s_l <= 0.0 && 0.0 <= speeds.s_star) {
    mass = hllc_star_normal_mass_flux(pair.left, normal, speeds.s_l, speeds.s_star);
    momentum_n = hllc_star_normal_momentum_flux(pair.left, normal, speeds.s_l, speeds.s_star, 9.81);
    tangential = tangential_velocity(pair.left, normal);
} else if (speeds.s_star <= 0.0 && 0.0 <= speeds.s_r) {
    mass = hllc_star_normal_mass_flux(pair.right, normal, speeds.s_r, speeds.s_star);
    momentum_n = hllc_star_normal_momentum_flux(pair.right, normal, speeds.s_r, speeds.s_star, 9.81);
    tangential = tangential_velocity(pair.right, normal);
} else {
    mass = physical_normal_mass_flux(pair.right, normal);
    momentum_n = physical_normal_momentum_flux(pair.right, normal, 9.81);
    tangential = tangential_velocity(pair.right, normal);
}
```

- [ ] **Step 4: Return vector momentum flux**

Before the return, compute:

```cpp
const auto tangent = tangent_from_normal(normal);
const core::Real scaled_mass = edge_fields.phi_e_n * mass;
const core::Real scaled_momentum_n = edge_fields.phi_e_n * momentum_n;
const core::Real scaled_momentum_t = scaled_mass * tangential;
```

Return:

```cpp
return EdgeFlux{
    .mass = scaled_mass,
    .momentum_n = scaled_momentum_n,
    .momentum_x = scaled_momentum_n * normal.x + scaled_momentum_t * tangent.x,
    .momentum_y = scaled_momentum_n * normal.y + scaled_momentum_t * tangent.y,
};
```

- [ ] **Step 5: Run flux tests**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_hllc_flux --config Debug && build/windows-msvc/tests/unit/surface2d/Debug/test_hllc_flux.exe
```

Expected after implementation: all `test_hllc_flux` cases PASS.

---

## Task 3: Add Step-Level Vector Residual Regression

**Files:**
- Modify: `tests/unit/surface2d/test_momentum_transport.cpp`

- [ ] **Step 1: Add direct vector residual test**

Append after `InternalEdgeMomentumResidualIsAntisymmetric`:

```cpp
TEST(SurfaceMomentumTransport, InternalEdgeUsesHllcVectorMomentumFlux) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto edge_index = first_internal_edge_index(mesh);
    const auto left_index = cell_index_by_id(mesh, *mesh.edges[edge_index].left_cell);
    const auto right_index = cell_index_by_id(mesh, *mesh.edges[edge_index].right_cell);

    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    state.cells[left_index].conserved.hu = 6.0;
    state.cells[left_index].conserved.hv = 2.0;
    state.cells[right_index].conserved.hu = 4.0;
    state.cells[right_index].conserved.hv = -3.0;
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    isolate_edge(dpm_fields, edge_index);

    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 10.0};
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields);
    const auto edge_diagnostics = diagnostics.edges[edge_index];
    const auto normal = scau::surface2d::Normal2{
        .x = mesh.edges[edge_index].normal.x,
        .y = mesh.edges[edge_index].normal.y,
    };
    const auto expected_flux = scau::surface2d::hllc_normal_flux(
        state.cells[left_index],
        state.cells[right_index],
        dpm_fields.edges[edge_index],
        normal,
        config.h_min);
    const double integrated_x = expected_flux.momentum_x * mesh.edges[edge_index].length;
    const double integrated_y = expected_flux.momentum_y * mesh.edges[edge_index].length;

    EXPECT_DOUBLE_EQ(edge_diagnostics.mass_flux, expected_flux.mass);
    EXPECT_DOUBLE_EQ(edge_diagnostics.momentum_flux_n, expected_flux.momentum_n);
    EXPECT_DOUBLE_EQ(diagnostics.cells[left_index].momentum_residual.x, -integrated_x);
    EXPECT_DOUBLE_EQ(diagnostics.cells[left_index].momentum_residual.y, -integrated_y);
    EXPECT_DOUBLE_EQ(diagnostics.cells[right_index].momentum_residual.x, integrated_x);
    EXPECT_DOUBLE_EQ(diagnostics.cells[right_index].momentum_residual.y, integrated_y);
}
```

Expected before Task 4: this FAILS because `advance_one_step_cpu(...)` still accumulates momentum as mass flux multiplied by upwind velocity plus separate pressure projection.

- [ ] **Step 2: Run red momentum transport target**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_momentum_transport --config Debug && build/windows-msvc/tests/unit/surface2d/Debug/test_momentum_transport.exe
```

Expected before Task 4: the new vector residual test FAILS.

---

## Task 4: Use HLLC Vector Flux In Step Residuals

**Files:**
- Modify: `libs/surface2d/src/time_integration/step.cpp`

- [ ] **Step 1: Replace upwind residual helper**

Replace `upwind_state(...)` and `accumulate_momentum_residual(...)` with:

```cpp
void accumulate_momentum_flux_residual(
    CellStepDiagnostics& sink,
    core::Real signed_integrated_flux_x,
    core::Real signed_integrated_flux_y) {
    sink.momentum_residual.x += signed_integrated_flux_x;
    sink.momentum_residual.y += signed_integrated_flux_y;
}
```

- [ ] **Step 2: Use vector flux for internal edges**

Replace internal-edge momentum transport accumulation:

```cpp
const auto& upwind = upwind_state(edge_diagnostics.mass_flux, state.cells[left_index], state.cells[right_index]);
accumulate_momentum_residual(diagnostics.cells[left_index], -integrated_flux, upwind);
accumulate_momentum_residual(diagnostics.cells[right_index], integrated_flux, upwind);
```

with:

```cpp
const auto flux = hllc_normal_flux(
    state.cells[left_index],
    state.cells[right_index],
    dpm_fields.edges[edge_index],
    Normal2{.x = edge.normal.x, .y = edge.normal.y},
    config.h_min);
const core::Real integrated_momentum_x = flux.momentum_x * edge.length;
const core::Real integrated_momentum_y = flux.momentum_y * edge.length;
accumulate_momentum_flux_residual(diagnostics.cells[left_index], -integrated_momentum_x, -integrated_momentum_y);
accumulate_momentum_flux_residual(diagnostics.cells[right_index], integrated_momentum_x, integrated_momentum_y);
```

If `edge_step_diagnostics(...)` already computed `flux`, refactor minimally so the same `EdgeFlux` is reused rather than calling `hllc_normal_flux(...)` twice.

- [ ] **Step 3: Use vector flux for open boundaries**

Replace open-boundary upwind momentum accumulation:

```cpp
const auto outside_for_upwind = open_boundary_outside_state(inside);
const auto& upwind_open = upwind_state(edge_diagnostics.mass_flux, inside, outside_for_upwind);
accumulate_momentum_residual(diagnostics.cells[inside_index], signed_integrated_flux, upwind_open);
```

with:

```cpp
const core::Real signed_momentum_x = edge.left_cell.has_value() ? -flux.momentum_x * edge.length : flux.momentum_x * edge.length;
const core::Real signed_momentum_y = edge.left_cell.has_value() ? -flux.momentum_y * edge.length : flux.momentum_y * edge.length;
accumulate_momentum_flux_residual(diagnostics.cells[inside_index], signed_momentum_x, signed_momentum_y);
```

- [ ] **Step 4: Run momentum transport target**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_momentum_transport --config Debug && build/windows-msvc/tests/unit/surface2d/Debug/test_momentum_transport.exe
```

Expected after implementation: all `test_momentum_transport` cases PASS.

---

## Task 5: Run M8/M9/M10/M11 Regression Targets

**Files:**
- No source changes expected.

- [ ] **Step 1: Build and run focused targets**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_hllc_flux test_hllc_wave_momentum test_hllc_wave_mass test_momentum_transport test_pressure_momentum --config Debug && PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug -R "^(test_hllc_flux|test_hllc_wave_momentum|test_hllc_wave_mass|test_momentum_transport|test_pressure_momentum)$" --output-on-failure
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

## Task 6: Record M11 Evidence And Commit

**Files:**
- Create: `superpowers/specs/2026-05-18-plan-m11-surface2d-hllc-vector-momentum-projection-evidence.md`

- [ ] **Step 1: Create evidence file**

Create `superpowers/specs/2026-05-18-plan-m11-surface2d-hllc-vector-momentum-projection-evidence.md`:

```markdown
# M11 Surface2D HLLC Vector Momentum Projection Evidence

**Date:** 2026-05-18
**Plan:** `superpowers/plans/2026-05-18-plan-m11-surface2d-hllc-vector-momentum-projection.md`
**M10 Basis:** `superpowers/specs/2026-05-18-plan-m10-surface2d-hllc-mass-flux-evidence.md`

## Scope

M11 extends the CPU reference HLLC edge flux path with Cartesian vector momentum flux and uses that vector flux in the Surface2D momentum residual path. It preserves existing mass flux, normal momentum flux, pressure projection, wet/dry blocking, DPM blocking, Wall/Open behavior, CFL diagnostics, and depth-update semantics.

## Local Validation

- Focused M8/M9/M10/M11 targets: PASS, update with observed count.
- Full Debug build: PASS.
- Surface2D CTest subset: PASS, update with observed count.
- Full CTest: PASS, update with observed count.

## Coverage

- `test_hllc_flux`: vector momentum projection onto Cartesian axes and tangential momentum advection with HLLC mass flux.
- `test_momentum_transport`: step-level residuals consume HLLC vector momentum flux directly.
- Existing mass, normal momentum, pressure, boundary, wetting/drying, conservative update, and golden-minimal tests remain passing.

## Boundaries

- `Surface2DCore` remains independent of SWMM, D-Flow FM, `CouplingLib`, and 1D engine ABIs.
- M11 remains limited to the CPU reference Surface2D edge-flux and momentum-residual path.
- M11 does not introduce CUDA backends, prescribed external boundary states, coupling ledger behavior, GoldenSuite release claims, or 1D engine interface changes.
```

- [ ] **Step 2: Review changed files**

Run:

```bash
git status --short && git diff -- libs/surface2d/include/surface2d/riemann/hllc.hpp libs/surface2d/src/riemann/hllc.cpp libs/surface2d/src/time_integration/step.cpp tests/unit/surface2d/test_hllc_flux.cpp tests/unit/surface2d/test_momentum_transport.cpp superpowers/plans/2026-05-18-plan-m11-surface2d-hllc-vector-momentum-projection.md superpowers/specs/2026-05-18-plan-m11-surface2d-hllc-vector-momentum-projection-evidence.md
```

Expected:
- Only M11 source, test, plan, and evidence files changed.
- No `Surface2DCore` dependency on SWMM, D-Flow FM, `CouplingLib`, or 1D engine ABIs.

- [ ] **Step 3: Commit implementation**

Run:

```bash
git add libs/surface2d/include/surface2d/riemann/hllc.hpp libs/surface2d/src/riemann/hllc.cpp libs/surface2d/src/time_integration/step.cpp tests/unit/surface2d/test_hllc_flux.cpp tests/unit/surface2d/test_momentum_transport.cpp superpowers/plans/2026-05-18-plan-m11-surface2d-hllc-vector-momentum-projection.md superpowers/specs/2026-05-18-plan-m11-surface2d-hllc-vector-momentum-projection-evidence.md
git commit -m "feat(surface2d): project HLLC vector momentum flux"
```

Expected: commit succeeds after all validation is recorded.

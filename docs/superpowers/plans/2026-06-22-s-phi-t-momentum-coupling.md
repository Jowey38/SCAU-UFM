# S_phi_t Momentum Coupling (Well-Balanced Pressure-Source Pairing) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Wire `S_phi_t` (porosity-gradient) and `S_topo` (bed-slope) into the Surface2D momentum update via a separated, same-template, Audusse single-sided well-balanced pressure-source pairing, so a lake at rest with a `phi_t` jump and/or a sloping bed stays exactly at rest (bit-wise, mesh-independent).

**Architecture:** HLLC momentum flux becomes advective-only (`h·u·u_n`, `phi_e_n`-scaled); the hydrostatic pressure is moved into a new `well_balanced` pure-function module that produces, per edge per cell-side, `−F_p,e + S_phi_t,e^(i) + S_topo,e^(i)` along that cell's outward normal. The Audusse square-difference topo source `½g·phi_t,i·(h̄_e²−h_i²)` (multiplied by the cell's own `phi_t,i`) makes the closed-polygon momentum residual reduce to a cell-center constant times `Σ n·L ≡ 0`. Wave speeds, mass flux, and CFL are untouched.

**Tech Stack:** C++20, CMake/CTest, GoogleTest, MSVC (`SCAU_WARNINGS_AS_ERRORS=ON`).

**Authoritative design:** `docs/superpowers/specs/2026-06-21-s-phi-t-momentum-coupling-design.md` (scope A; two numerical reviews incorporated). Main Spec §5.2/§5.5/§5.5.1/§5.5.2/§5.6.

**Build/test commands (this repo, Windows):**
- Configure: `cmake --preset windows-msvc`
- Build all: `cmake --build --preset windows-msvc`
- Build one target: `cmake --build --preset windows-msvc --target <target>`
- Run subset: `ctest --preset windows-msvc -R <regex>`
- Full suite: `ctest --preset windows-msvc --timeout 120`
- **GOTCHA (cerebrum):** on `LNK1168`, kill stray `test_*`/`ctest` processes and delete the locked `.exe` by basename (`find . -name X.exe -exec rm {}`), rebuild until `grep -c LNK1168 == 0` before trusting ctest. Keep all C++ comments ASCII-only (codepage 936 + warnings-as-errors). Use `EXPECT_NEAR` for recomposed floating aggregates.

---

## File Structure

- `libs/surface2d/src/riemann/hllc.cpp` (modify) — add `advective_normal_momentum_flux`; flux branches + star helper use advective momentum; `estimate_hllc_wave_speeds` keeps full `physical_normal_momentum_flux`.
- `libs/surface2d/include/surface2d/source_terms/well_balanced.hpp` (create) — pure WB pairing API.
- `libs/surface2d/src/source_terms/well_balanced.cpp` (create) — WB pairing implementation.
- `libs/surface2d/include/surface2d/time_integration/step.hpp` (modify) — add `wb_pressure`, `s_topo` to `EdgeStepDiagnostics`.
- `libs/surface2d/src/time_integration/step.cpp` (modify) — apply advective HLLC momentum + WB pairing on internal edges; phi_t-scaled WB pressure at wall/open boundaries.
- `libs/surface2d/CMakeLists.txt` (modify) — add `src/source_terms/well_balanced.cpp`.
- `tests/unit/surface2d/test_hllc_flux.cpp` (modify) — update pressure-momentum assertions (advective-only).
- `tests/unit/surface2d/test_well_balanced_pairing.cpp` (create) — WB module unit tests.
- `tests/unit/surface2d/test_well_balanced_phi_t_jump_at_rest.cpp` (create) — G4.
- `tests/unit/surface2d/test_well_balanced_sloping_bed_at_rest.cpp` (create) — G5.
- `tests/unit/surface2d/CMakeLists.txt` (modify) — add the three new test executables.

---

## Task 1: HLLC momentum becomes advective-only (wave speeds unchanged)

**Files:**
- Modify: `libs/surface2d/src/riemann/hllc.cpp`
- Test: `tests/unit/surface2d/test_hllc_flux.cpp`

- [ ] **Step 1: Update the failing assertions in test_hllc_flux.cpp**

In `tests/unit/surface2d/test_hllc_flux.cpp`, replace the test `ZeroVelocityHydrostaticStateHasZeroMassFluxAndPressureMomentum` (lines 14-27) with:

```cpp
TEST(HllcFlux, ZeroVelocityHydrostaticStateHasZeroMassAndZeroAdvectiveMomentum) {
    const scau::surface2d::CellState left{.conserved = {.h = 1.0, .hu = 0.0, .hv = 0.0}, .eta = 1.0};
    const scau::surface2d::CellState right{.conserved = {.h = 1.0, .hu = 0.0, .hv = 0.0}, .eta = 1.0};
    const scau::surface2d::EdgeDpmFields edge_fields{.phi_e_n = 1.0, .omega_edge = 1.0};

    const auto flux = scau::surface2d::hllc_normal_flux(
        left, right, edge_fields, scau::surface2d::Normal2{.x = 1.0, .y = 0.0});

    // Advective-only momentum: at rest the momentum flux carries no pressure.
    EXPECT_EQ(flux.mass, 0.0);
    EXPECT_EQ(flux.momentum_n, 0.0);
    EXPECT_EQ(flux.momentum_x, 0.0);
    EXPECT_EQ(flux.momentum_y, 0.0);
}
```

In `VectorMomentumProjectsNormalFluxOntoCartesianAxes` (lines 79-89) the relation `momentum_x == momentum_n`, `momentum_y == 0` still holds (momentum is now advective but still purely normal for normal-aligned flow) — leave it.

- [ ] **Step 2: Run the test to verify the rest-state assertion fails**

Run: `cmake --build --preset windows-msvc --target test_hllc_flux && ctest --preset windows-msvc -R "^test_hllc_flux$" -V`
Expected: FAIL — `flux.momentum_n` is `4.905`, expected `0.0` (pressure still bundled in momentum).

- [ ] **Step 3: Add advective-only momentum flux and use it for flux output only**

In `libs/surface2d/src/riemann/hllc.cpp`, inside the anonymous namespace, ADD a new helper right after `physical_normal_momentum_flux` (do NOT modify `physical_normal_momentum_flux` — it is still used by the wave-speed estimate):

```cpp
core::Real advective_normal_momentum_flux(const CellState& state, Normal2 normal) {
    const core::Real un = normal_velocity(state, normal);
    return state.conserved.h * un * un;
}
```

Change `hllc_star_normal_momentum_flux` to use the advective base and drop the now-unused gravity parameter. Replace the whole function with:

```cpp
core::Real hllc_star_normal_momentum_flux(
    const CellState& state,
    Normal2 normal,
    core::Real s_k,
    core::Real s_star) {
    const core::Real un = normal_velocity(state, normal);
    const core::Real denominator = s_k - s_star;
    if (denominator == 0.0) {
        return advective_normal_momentum_flux(state, normal);
    }
    const core::Real h_star = state.conserved.h * (s_k - un) / denominator;
    const core::Real momentum_star = h_star * s_star;
    return advective_normal_momentum_flux(state, normal) + s_k * (momentum_star - state.conserved.h * un);
}
```

In `hllc_normal_flux`, the four wave-branch assignments to `momentum_n` must use the advective forms. Change the two supersonic branches from `physical_normal_momentum_flux(pair.left, normal, 9.81)` / `physical_normal_momentum_flux(pair.right, normal, 9.81)` to `advective_normal_momentum_flux(pair.left, normal)` / `advective_normal_momentum_flux(pair.right, normal)`, and the two star branches from `hllc_star_normal_momentum_flux(pair.left, normal, speeds.s_l, speeds.s_star, 9.81)` / `...(pair.right, normal, speeds.s_r, speeds.s_star, 9.81)` to the new 4-arg form `hllc_star_normal_momentum_flux(pair.left, normal, speeds.s_l, speeds.s_star)` / `...(pair.right, normal, speeds.s_r, speeds.s_star)`.

Leave `estimate_hllc_wave_speeds` UNCHANGED — it must keep calling `physical_normal_momentum_flux(... , gravity)` (full flux with pressure) for the standard `s_star` (Main Spec §5.5.1).

- [ ] **Step 4: Run the full hllc flux suite to verify it passes**

Run: `cmake --build --preset windows-msvc --target test_hllc_flux && ctest --preset windows-msvc -R "^test_hllc_flux$" -V`
Expected: PASS (all cases). If `MassFluxAppliesPhiEdgeScalingAfterHllcBranchSelection` or wave-speed tests fail, the wave-speed path was wrongly modified — revert `estimate_hllc_wave_speeds` to use `physical_normal_momentum_flux`.

- [ ] **Step 5: Commit**

```bash
git add libs/surface2d/src/riemann/hllc.cpp tests/unit/surface2d/test_hllc_flux.cpp
git commit -m "feat(surface2d): HLLC momentum flux advective-only (pressure moves to WB pairing)

physical_normal_momentum_flux stays for the standard HLLC s_star wave
speed (spec 5.5.1); flux output uses a new advective_normal_momentum_flux
(h*un^2). Pressure is handled by the well-balanced pairing (next task)."
```

---

## Task 2: `well_balanced` pure-function pairing module

**Files:**
- Create: `libs/surface2d/include/surface2d/source_terms/well_balanced.hpp`
- Create: `libs/surface2d/src/source_terms/well_balanced.cpp`
- Modify: `libs/surface2d/CMakeLists.txt`
- Test: `tests/unit/surface2d/test_well_balanced_pairing.cpp`
- Modify: `tests/unit/surface2d/CMakeLists.txt`

- [ ] **Step 1: Write the header**

Create `libs/surface2d/include/surface2d/source_terms/well_balanced.hpp`:

```cpp
#pragma once

#include "core/types.hpp"

namespace scau::surface2d {

// Per-edge well-balanced pressure-source pairing for one internal edge
// (Main Spec 5.6, Audusse single-sided split). All quantities are scalar
// magnitudes along the edge normal n (which points from the left cell into
// the right cell). The caller applies:
//   dM_left  += left_normal  * n * len
//   dM_right += right_normal * (-n) * len
// so the pressure flux F_p (shared) is conservative (opposite signs) while
// each side's S_phi_t/S_topo source uses that cell's own phi_t and depth.
struct WellBalancedEdgePairing {
    core::Real left_normal{0.0};   // -F_p + S_phi_t^(L) + S_topo^(L)
    core::Real right_normal{0.0};  // -F_p + S_phi_t^(R) + S_topo^(R)
    core::Real pressure_flux{0.0}; // F_p,e (diagnostic)
    core::Real s_phi_t_left{0.0};  // diagnostic
    core::Real s_topo_left{0.0};   // diagnostic
};

// Inputs:
//   phi_t_left/right : cell-centre storage porosity of each side (phi_t,i)
//   h_left/right     : cell-centre water depth of each side (h_i)
//   h_left_star/right_star : hydrostatic-reconstructed depth at the edge (h_i*)
//   gravity          : g
// Edge templates (Main Spec 5.5.2 arithmetic mean):
//   h_bar = 0.5*(h_left_star + h_right_star);  phi_t_e = 0.5*(phi_t_left+phi_t_right)
//   F_p           = 0.5*g*phi_t_e*h_bar^2
//   S_phi_t^(i)   = 0.5*g*h_bar^2*(phi_t_e - phi_t_i)
//   S_topo^(i)    = 0.5*g*phi_t_i*(h_bar^2 - h_i^2)   (Audusse square-difference)
[[nodiscard]] WellBalancedEdgePairing well_balanced_edge_pairing(
    core::Real phi_t_left,
    core::Real phi_t_right,
    core::Real h_left,
    core::Real h_right,
    core::Real h_left_star,
    core::Real h_right_star,
    core::Real gravity = 9.81);

// Reflective-wall / open-boundary single-cell hydrostatic pressure magnitude:
// F_p = 0.5*g*phi_t_inside*h_inside^2 (phi_t-scaled; topo source is zero at a
// reflective boundary because the mirrored neighbour has the same bed).
[[nodiscard]] core::Real well_balanced_boundary_pressure(
    core::Real phi_t_inside,
    core::Real h_inside,
    core::Real gravity = 9.81);

}  // namespace scau::surface2d
```

- [ ] **Step 2: Write the failing module tests**

Create `tests/unit/surface2d/test_well_balanced_pairing.cpp`:

```cpp
#include <gtest/gtest.h>

#include "surface2d/source_terms/well_balanced.hpp"

namespace {
constexpr double kG = 9.81;
}

// Uniform phi_t = 1, flat (h_star = h on both sides): pure pressure -0.5 g h^2,
// no porosity or topo source.
TEST(WellBalancedPairing, UniformFlatReducesToPurePressure) {
    const auto p = scau::surface2d::well_balanced_edge_pairing(1.0, 1.0, 2.0, 2.0, 2.0, 2.0, kG);
    EXPECT_NEAR(p.pressure_flux, 0.5 * kG * 4.0, 1e-12);
    EXPECT_NEAR(p.s_phi_t_left, 0.0, 1e-12);
    EXPECT_NEAR(p.s_topo_left, 0.0, 1e-12);
    EXPECT_NEAR(p.left_normal, -0.5 * kG * 4.0, 1e-12);
    EXPECT_NEAR(p.right_normal, -0.5 * kG * 4.0, 1e-12);
}

// phi_t jump, flat bed (h_star = h = h_bar): each side reduces to its OWN
// -0.5 g phi_t_i h_bar^2 (Main Spec 5.6 single-sided cancellation).
TEST(WellBalancedPairing, PhiTJumpFlatReducesToLocalPorosityPressure) {
    const double h = 1.0;
    const auto p = scau::surface2d::well_balanced_edge_pairing(1.0, 1.25, h, h, h, h, kG);
    EXPECT_NEAR(p.left_normal, -0.5 * kG * 1.0 * h * h, 1e-12);
    EXPECT_NEAR(p.right_normal, -0.5 * kG * 1.25 * h * h, 1e-12);
}

// Sloping bed (h_bar != h_i), uniform phi_t: topo square-difference absorbs
// h_bar^2 so each side reduces to -0.5 g phi_t_i h_i^2.
TEST(WellBalancedPairing, SlopingBedReducesToLocalCellDepthPressure) {
    const double phi = 1.0;
    const double h_left = 2.0;
    const double h_right = 1.0;
    const double h_left_star = 0.8;   // arbitrary reconstructed edge depth
    const double h_right_star = 0.8;
    const double h_bar = 0.5 * (h_left_star + h_right_star);
    const auto p = scau::surface2d::well_balanced_edge_pairing(
        phi, phi, h_left, h_right, h_left_star, h_right_star, kG);
    EXPECT_NEAR(p.left_normal, -0.5 * kG * phi * h_left * h_left, 1e-12);
    EXPECT_NEAR(p.right_normal, -0.5 * kG * phi * h_right * h_right, 1e-12);
    // pressure flux uses the edge template, not the cell depth
    EXPECT_NEAR(p.pressure_flux, 0.5 * kG * phi * h_bar * h_bar, 1e-12);
}

// Combined phi_t jump + sloping bed: left side reduces to -0.5 g phi_t_L h_L^2.
TEST(WellBalancedPairing, CombinedJumpAndSlopeReducesToLocalConstant) {
    const auto p = scau::surface2d::well_balanced_edge_pairing(
        0.8, 1.0, 1.5, 0.9, 0.7, 0.7, kG);
    EXPECT_NEAR(p.left_normal, -0.5 * kG * 0.8 * 1.5 * 1.5, 1e-12);
    EXPECT_NEAR(p.right_normal, -0.5 * kG * 1.0 * 0.9 * 0.9, 1e-12);
}

TEST(WellBalancedPairing, BoundaryPressureIsPhiTScaled) {
    EXPECT_NEAR(scau::surface2d::well_balanced_boundary_pressure(1.0, 2.0, kG), 0.5 * kG * 4.0, 1e-12);
    EXPECT_NEAR(scau::surface2d::well_balanced_boundary_pressure(0.5, 2.0, kG), 0.5 * kG * 0.5 * 4.0, 1e-12);
}
```

- [ ] **Step 3: Run the tests to verify they fail to link/compile**

Run: `cmake --preset windows-msvc` (after wiring CMake in Step 5) then `cmake --build --preset windows-msvc --target test_well_balanced_pairing`
Expected: FAIL — `well_balanced_edge_pairing` undefined (no implementation yet). (If the target does not exist yet, do Step 5 CMake wiring first, then return here.)

- [ ] **Step 4: Write the implementation**

Create `libs/surface2d/src/source_terms/well_balanced.cpp`:

```cpp
#include "surface2d/source_terms/well_balanced.hpp"

namespace scau::surface2d {

WellBalancedEdgePairing well_balanced_edge_pairing(
    core::Real phi_t_left,
    core::Real phi_t_right,
    core::Real h_left,
    core::Real h_right,
    core::Real h_left_star,
    core::Real h_right_star,
    core::Real gravity) {
    const core::Real h_bar = 0.5 * (h_left_star + h_right_star);
    const core::Real h_bar_sq = h_bar * h_bar;
    const core::Real phi_t_e = 0.5 * (phi_t_left + phi_t_right);

    const core::Real pressure_flux = 0.5 * gravity * phi_t_e * h_bar_sq;

    const core::Real s_phi_t_left = 0.5 * gravity * h_bar_sq * (phi_t_e - phi_t_left);
    const core::Real s_phi_t_right = 0.5 * gravity * h_bar_sq * (phi_t_e - phi_t_right);

    const core::Real s_topo_left = 0.5 * gravity * phi_t_left * (h_bar_sq - h_left * h_left);
    const core::Real s_topo_right = 0.5 * gravity * phi_t_right * (h_bar_sq - h_right * h_right);

    return WellBalancedEdgePairing{
        .left_normal = -pressure_flux + s_phi_t_left + s_topo_left,
        .right_normal = -pressure_flux + s_phi_t_right + s_topo_right,
        .pressure_flux = pressure_flux,
        .s_phi_t_left = s_phi_t_left,
        .s_topo_left = s_topo_left,
    };
}

core::Real well_balanced_boundary_pressure(
    core::Real phi_t_inside,
    core::Real h_inside,
    core::Real gravity) {
    return 0.5 * gravity * phi_t_inside * h_inside * h_inside;
}

}  // namespace scau::surface2d
```

- [ ] **Step 5: Wire CMake (library source + test target)**

In `libs/surface2d/CMakeLists.txt`, add `src/source_terms/well_balanced.cpp` to the `add_library(scau_surface2d ...)` source list (keep the list alphabetical within `source_terms/`: it goes after `source_terms/rainfall.cpp`).

Append to `tests/unit/surface2d/CMakeLists.txt`:

```cmake
add_executable(test_well_balanced_pairing test_well_balanced_pairing.cpp)
target_link_libraries(test_well_balanced_pairing
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_well_balanced_pairing COMMAND test_well_balanced_pairing)
```

- [ ] **Step 6: Build and run the module tests**

Run: `cmake --preset windows-msvc && cmake --build --preset windows-msvc --target test_well_balanced_pairing && ctest --preset windows-msvc -R "^test_well_balanced_pairing$" -V`
Expected: PASS (5 cases).

- [ ] **Step 7: Commit**

```bash
git add libs/surface2d/include/surface2d/source_terms/well_balanced.hpp libs/surface2d/src/source_terms/well_balanced.cpp libs/surface2d/CMakeLists.txt tests/unit/surface2d/test_well_balanced_pairing.cpp tests/unit/surface2d/CMakeLists.txt
git commit -m "feat(surface2d): well_balanced pressure-source pairing module (Audusse single-sided)"
```

---

## Task 3: Wire WB pairing into the step (internal edges + boundaries)

**Files:**
- Modify: `libs/surface2d/include/surface2d/time_integration/step.hpp`
- Modify: `libs/surface2d/src/time_integration/step.cpp`
- Test: existing surface2d suite (regression)

- [ ] **Step 1: Add wb diagnostic fields to EdgeStepDiagnostics**

In `libs/surface2d/include/surface2d/time_integration/step.hpp`, inside `struct EdgeStepDiagnostics`, add two fields after `s_phi_t`:

```cpp
    core::Real wb_pressure{0.0};  // F_p,e: phi_t-scaled WB pressure flux magnitude
    core::Real s_topo{0.0};       // S_topo^(left) diagnostic for the edge
```

- [ ] **Step 2: Include the WB header and reconstruction in step.cpp**

In `libs/surface2d/src/time_integration/step.cpp`, add includes (keep grouped with the other `surface2d/...` includes, alphabetical):

```cpp
#include "surface2d/reconstruction/hydrostatic.hpp"
#include "surface2d/source_terms/well_balanced.hpp"
```

- [ ] **Step 3: Apply WB pairing on the internal-edge branch**

In `advance_one_step_cpu` (the 7-arg overload), inside the `if (adjacency.is_internal())` block, AFTER the existing advective momentum accumulation (the two `accumulate_momentum_flux_residual(... integrated_momentum_x ...)` calls) and BEFORE `continue;`, insert:

```cpp
            if (assemble_wb_pairing) {
                const auto pair = reconstruct_hydrostatic_pair(state.cells[left_index], state.cells[right_index]);
                const auto wb = well_balanced_edge_pairing(
                    dpm_fields.cells[left_index].phi_t,
                    dpm_fields.cells[right_index].phi_t,
                    state.cells[left_index].conserved.h,
                    state.cells[right_index].conserved.h,
                    pair.left.conserved.h,
                    pair.right.conserved.h,
                    config.gravity);
                diagnostics.edges.back().wb_pressure = wb.pressure_flux;
                diagnostics.edges.back().s_topo = wb.s_topo_left;
                // Left outward normal = +edge.normal; right outward normal = -edge.normal.
                accumulate_momentum_flux_residual(
                    diagnostics.cells[left_index],
                    wb.left_normal * edge.normal.x * edge.length,
                    wb.left_normal * edge.normal.y * edge.length);
                accumulate_momentum_flux_residual(
                    diagnostics.cells[right_index],
                    -wb.right_normal * edge.normal.x * edge.length,
                    -wb.right_normal * edge.normal.y * edge.length);
            }
```

Note: `assemble_wb_pairing` is already computed above in this branch via `classify_edge(...).wb_pairing_assembled`; reuse that variable.

- [ ] **Step 4: Replace the wall-branch pressure with the phi_t-scaled WB form**

In the `if (boundary.edges[edge_index] == BoundaryKind::Wall)` block, replace the line

```cpp
            const core::Real wall_pressure = 0.5 * 9.81 * h_inside * h_inside;
```

with

```cpp
            const core::Real wall_pressure = well_balanced_boundary_pressure(
                dpm_fields.cells[inside_index].phi_t, h_inside, config.gravity);
```

(The rest of the wall block — building `EdgeStepDiagnostics`, signing by `adjacency.left.has_value()`, accumulating — stays unchanged.)

- [ ] **Step 5: Build and run the full surface2d suite (regression)**

Run: `cmake --build --preset windows-msvc && ctest --preset windows-msvc -R "surface2d|hllc|momentum|pressure|dpm_edge|well_balanced|geometry|cfl|wetting|friction|rainfall|coupling_exchange|step|boundary|conservative|golden" --timeout 120`
Expected: PASS. Rationale for zero drift: for `phi_t=1` and flat bed, `well_balanced_edge_pairing` gives `left_normal = right_normal = -0.5 g h^2` and the advective HLLC momentum is `h un^2`; their sum reproduces the previous `phi_e_n=1` full-flux momentum, and the M249-migrated edge-diagnostic tests assert `momentum_x/y == hllc_normal_flux(...)` (advective, follows the impl). The wall tests use `phi_t=1` so `well_balanced_boundary_pressure = 0.5 g h^2` (unchanged).

If a momentum cell-total assertion fails because it predates M249 edge-diagnostic migration, fix it by asserting the per-edge diagnostic (`diagnostics.edges[edge_index].momentum_x/y`) plus, where the WB pairing is exercised, `wb_pressure`/`s_topo`; do NOT weaken a conservation assertion to chase green.

- [ ] **Step 6: Commit**

```bash
git add libs/surface2d/include/surface2d/time_integration/step.hpp libs/surface2d/src/time_integration/step.cpp tests/unit/surface2d
git commit -m "feat(surface2d): apply WB phi_t/topo pairing in step; phi_t-scaled wall pressure"
```

---

## Task 4: G4 (phi_t-jump at rest) and G5 (sloping-bed at rest) GoldenTests

**Files:**
- Create: `tests/unit/surface2d/test_well_balanced_phi_t_jump_at_rest.cpp`
- Create: `tests/unit/surface2d/test_well_balanced_sloping_bed_at_rest.cpp`
- Modify: `tests/unit/surface2d/CMakeLists.txt`

- [ ] **Step 1: Write G4 (phi_t jump, flat bed, walled, at rest stays at rest)**

Create `tests/unit/surface2d/test_well_balanced_phi_t_jump_at_rest.cpp`:

```cpp
#include <algorithm>
#include <cmath>
#include <cstddef>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/state/state.hpp"
#include "surface2d/time_integration/step.hpp"

namespace {
double max_abs_momentum(const scau::surface2d::SurfaceState& state) {
    double worst = 0.0;
    for (const auto& cell : state.cells) {
        worst = std::max({worst, std::abs(cell.conserved.hu), std::abs(cell.conserved.hv)});
    }
    return worst;
}
}  // namespace

// G4: flat bed (eta = h uniform => z_b = 0), zero velocity, walled domain,
// phi_t jump across cells. With the phi_t-scaled WB pressure + S_phi_t pairing
// the lake stays exactly at rest.
TEST(WellBalancedPhiTJumpAtRest, FlatBedPhiTJumpKeepsMomentumZero) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    for (std::size_t i = 0; i < dpm_fields.cells.size(); ++i) {
        dpm_fields.cells[i].phi_t = (i % 2 == 0) ? 1.0 : 1.5;  // strong jump
    }
    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 100.0, .h_min = 1.0e-8};

    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields);

    ASSERT_FALSE(diagnostics.rollback_required);
    EXPECT_NEAR(max_abs_momentum(state), 0.0, 1.0e-12);
}
```

- [ ] **Step 2: Write G5 (sloping bed, uniform phi_t, walled, at rest stays at rest)**

Create `tests/unit/surface2d/test_well_balanced_sloping_bed_at_rest.cpp`:

```cpp
#include <algorithm>
#include <cmath>
#include <cstddef>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/state/state.hpp"
#include "surface2d/time_integration/step.hpp"

namespace {
double max_abs_momentum(const scau::surface2d::SurfaceState& state) {
    double worst = 0.0;
    for (const auto& cell : state.cells) {
        worst = std::max({worst, std::abs(cell.conserved.hu), std::abs(cell.conserved.hv)});
    }
    return worst;
}
}  // namespace

// G5: sloping bed via a varying z_b = eta - h. Free surface eta is uniform
// (1.5 everywhere); each cell's depth h = eta - z_b differs (z_b = cell-index
// fraction). Zero velocity, walled domain, uniform phi_t. The Audusse
// square-difference S_topo pairing keeps the lake at rest, mesh-independent.
TEST(WellBalancedSlopingBedAtRest, SlopingBedKeepsMomentumZero) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const double eta = 1.5;
    auto state = scau::surface2d::SurfaceState::for_mesh(mesh);
    for (std::size_t i = 0; i < state.cells.size(); ++i) {
        const double z_b = 0.1 * static_cast<double>(i);  // sloping bed
        state.cells[i].eta = eta;
        state.cells[i].conserved.h = eta - z_b;            // h_i = eta - z_b > 0
        state.cells[i].conserved.hu = 0.0;
        state.cells[i].conserved.hv = 0.0;
    }
    const auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);  // uniform phi_t = 1
    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 100.0, .h_min = 1.0e-8};

    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields);

    ASSERT_FALSE(diagnostics.rollback_required);
    EXPECT_NEAR(max_abs_momentum(state), 0.0, 1.0e-12);
}
```

- [ ] **Step 3: Wire the two test executables**

Append to `tests/unit/surface2d/CMakeLists.txt`:

```cmake
add_executable(test_well_balanced_phi_t_jump_at_rest test_well_balanced_phi_t_jump_at_rest.cpp)
target_link_libraries(test_well_balanced_phi_t_jump_at_rest
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_well_balanced_phi_t_jump_at_rest COMMAND test_well_balanced_phi_t_jump_at_rest)

add_executable(test_well_balanced_sloping_bed_at_rest test_well_balanced_sloping_bed_at_rest.cpp)
target_link_libraries(test_well_balanced_sloping_bed_at_rest
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_well_balanced_sloping_bed_at_rest COMMAND test_well_balanced_sloping_bed_at_rest)
```

- [ ] **Step 4: Build and run G4/G5**

Run: `cmake --preset windows-msvc && cmake --build --preset windows-msvc --target test_well_balanced_phi_t_jump_at_rest test_well_balanced_sloping_bed_at_rest && ctest --preset windows-msvc -R "well_balanced_phi_t_jump_at_rest|well_balanced_sloping_bed_at_rest" -V`
Expected: PASS (both, residual < 1e-12).

If G5 fails at `O(dz_b)`: the reconstruction `reconstruct_hydrostatic_pair` does not match the Audusse `z_b_e = max(z_b_i, z_b_j)` template assumed by the topo proof. Diagnose by checking `pair.left.conserved.h` vs `eta - max(z_b_i, z_b_j)`. Fix path: in `reconstruct_hydrostatic_pair`, ensure `h_i* = max(0, eta_i - max(z_b_i, z_b_j))` (Audusse), or pass `h_i*` consistent with `well_balanced_edge_pairing`'s `h̄` template. Do not loosen the G5 tolerance to pass.

- [ ] **Step 5: Commit**

```bash
git add tests/unit/surface2d/test_well_balanced_phi_t_jump_at_rest.cpp tests/unit/surface2d/test_well_balanced_sloping_bed_at_rest.cpp tests/unit/surface2d/CMakeLists.txt
git commit -m "test(surface2d): G4 phi_t-jump and G5 sloping-bed lake-at-rest well-balancing"
```

---

## Task 5: Full self-check, security review, evidence archival

**Files:**
- Create: `superpowers/specs/2026-06-22-s-phi-t-momentum-coupling-evidence.md`
- Modify: `superpowers/INDEX.md`, `.wolf/anatomy.md`, `.wolf/memory.md`, `.wolf/cerebrum.md`

- [ ] **Step 1: Full clean build with zero link errors**

Run: kill stray `test_*`/`ctest`; then `cmake --build --preset windows-msvc 2>&1 | tee /tmp/sphit_build.txt`; verify `grep -cE "LNK1168|error C|: error" /tmp/sphit_build.txt` prints `0` (delete locked exes and rebuild if not).

- [ ] **Step 2: Full test suite**

Run: `ctest --preset windows-msvc --timeout 120`
Expected: 100% pass (previous 139 + 3 new module tests + 2 new GoldenTests; count rises accordingly). Record the exact `N/N passed` line.

- [ ] **Step 3: Security review of the diff**

Review the changed surface (`hllc.cpp`, `well_balanced.*`, `step.cpp`): all pure arithmetic; index access into `dpm_fields.cells`/`state.cells` is bounds-guarded by `validate_*_match_mesh` + `GeometryCache` adjacency validation; no raw memory, strings, paths, or third-party calls; `well_balanced` has no allocation. Confirm no new fail-open paths. Note findings (expected: none).

- [ ] **Step 4: Write the evidence document**

Create `superpowers/specs/2026-06-22-s-phi-t-momentum-coupling-evidence.md` recording: scope A landed; HLLC advective-only + WB Audusse single-sided pairing; G4/G5 bit-wise (1e-12); full-suite `N/N`; security review clean; the dynamic phi_t-jump remains the Spec §5.5.2 validation blank zone (not claimed); GoldenSuite manifest still a separate task (G4/G5 as candidate `G_n`).

- [ ] **Step 5: Update INDEX.md and OpenWolf records**

Add the evidence row to `superpowers/INDEX.md` 阶段性证据入口 table. Append `.wolf/anatomy.md` entries for `well_balanced.{hpp,cpp}` and the three new tests; append `.wolf/memory.md` session lines; append a `.wolf/cerebrum.md` decision entry (advective-only HLLC + WB pairing landed; lesson: `physical_normal_momentum_flux` is shared with the s_star wave-speed estimate, so advective-only must be a SEPARATE function, never a strip of the shared one).

- [ ] **Step 6: Commit**

```bash
git add superpowers/specs/2026-06-22-s-phi-t-momentum-coupling-evidence.md superpowers/INDEX.md .wolf
git commit -m "docs(surface2d): S_phi_t momentum-coupling evidence + OpenWolf records"
```

---

## Self-Review

**Spec coverage:** §1 architecture (advective-only HLLC + WB module + step wiring + phi_t source evolution) → Tasks 1-3. §2.2 discretization (F_p, S_phi_t single-sided, S_topo Audusse square-diff) → Task 2 module + Task 2 tests. §2.3 cancellation → Task 2 tests (per-side reductions) + Task 4 (closed-polygon at rest). §2.5 phi_t=1 zero-drift → Task 3 Step 5 regression. §3.1 G4/G5 → Task 4. §3.2 baseline impact (test_hllc_flux) → Task 1; M249 edge-diagnostic tests follow impl. §3.3 slices → Tasks 1-5. §3.5 naming (`wb_pressure`, `s_topo`, `h_bar`, `phi_t_e`) → Tasks 2-3. §4 boundary declaration → Task 5 evidence.

**Placeholder scan:** none — every code step shows full code; commands and expected output are explicit.

**Type consistency:** `well_balanced_edge_pairing` 7-arg signature and `WellBalancedEdgePairing` fields (`left_normal`, `right_normal`, `pressure_flux`, `s_phi_t_left`, `s_topo_left`) are identical across header (Task 2 Step 1), tests (Task 2 Step 2), impl (Task 2 Step 4), and step wiring (Task 3 Step 3). `well_balanced_boundary_pressure(phi_t_inside, h_inside, gravity)` consistent in Task 2 and Task 3 Step 4. `EdgeStepDiagnostics.wb_pressure`/`.s_topo` added in Task 3 Step 1, used in Task 3 Step 3. `reconstruct_hydrostatic_pair(left,right).left.conserved.h` matches the existing `HydrostaticPair` interface.

**Known implementation risk (flagged, not a placeholder):** Task 4 Step 4 carries the one residual numerical risk — whether the existing `reconstruct_hydrostatic_pair` (`eta = min(eta_L,eta_R)`) matches the Audusse `z_b_e = max` template the topo proof assumes. The fix path is specified inline (make `h_i*` Audusse-consistent), with G5 (tol 1e-12, never loosened) as the gate.

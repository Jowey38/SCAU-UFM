# M247-D Roof Drainage Intent / CouplingLib Boundary Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Wire the roof direct-drain chain into the step path: per cell run `evaluate_roof_emit`, emit a `RoofDrainageIntent`, obtain a `RoofDrainageAcceptance` from a CouplingLib **acceptance port** (mocked here — real SWMM adapter deferred), apply it (`apply_roof_drainage_acceptance`), route overflow to the mapped 2D surface cell, scatter roof state, and audit — all rollback-safe.

**Architecture:** Ports-and-adapters. `Surface2DCore` defines the port `RoofDrainageAcceptanceFn = std::function<RoofDrainageAcceptance(const RoofDrainageIntent&)>`; the step path calls it once per roof cell. A real CouplingLib/SWMM adapter (writes accepted volume as node lateral inflow `q = V_accepted/dt_sub`) plugs into this port later, once the M240 tri-coupling driver + SWMM engine are on the base. This branch (`master + surface2d foundation`) has no SWMM engine, so M247-D is verified with **mock** acceptance functions. The roof stage runs after ground runoff and before coupling exchange: `flux → ground runoff → roof runoff → exchange → friction`.

**Tech Stack:** C++20, CMake (preset `windows-msvc`, `VCPKG_ROOT=H:/githubcode/vcpkg`), GoogleTest. `core::Real` (double). Namespace `scau::surface2d`.

**Builds on:** `feat/m247c-step-seam` (foundation + runoff A/B + ground step integration). **Authoritative spec:** `superpowers/specs/2026-06-13-m247-urban-runoff-generation-design.md` §3.4, §4.6, §5, §6.2.

---

## Hard Constraints (every task)

- **ASCII-only**; **`EXPECT_NEAR`** tol ~1e-9; **fail-closed** validation; `std::size_t` indices; `[[nodiscard]]`; namespace `scau::surface2d`.
- Build/test with `VCPKG_ROOT=H:/githubcode/vcpkg`. Append-only CMake. Verify `grep -c LNK1168 == 0` after any killed ctest.
- **Rollback-safe:** the roof stage runs only on the accepted-step path (after the pre-mutation `rollback_required` early-return); `RunoffState` roof fields and the surface `h` are not mutated on rollback.
- **Scope:** mock acceptance port only. Do NOT add a real SWMM adapter / M240 driver call (absent on this branch). The port is a `std::function`; tests supply mocks.

## Key flow (per roof cell, one substep)

```text
emit  = evaluate_roof_emit(cell_inputs, params, cell_state, dt)   # fills roof_abstraction_filled, adds roof_input to pending, computes requested
intent = { source_cell_index=i, target_swmm_node_index = map.swmm_node_index[i],
           requested_volume = emit.requested_volume, source_roof_area = roof_fraction*area }
acc   = accept_fn(intent)                                          # CouplingLib port (mock here)
ar    = apply_roof_drainage_acceptance(cell_inputs, params, cell_state, acc,
                                       overflow_target_valid = map.roof_overflow_to_surface_cell_index[i] >= 0)
if ar.overflow_to_surface_volume > 0: h[overflow_cell] += ar.overflow_to_surface_volume / (phi_t[oc]*area[oc])
scatter roof_abstraction_filled, roof_pending_volume back to SoA
audit: roof_to_swmm_requested/accepted/rejected += ...; roof_pending_delta += emit.roof_input_volume + ar.pending_delta_volume;
       roof_overflow_to_surface += ar.overflow_to_surface_volume; abstraction_volume += emit.roof_abstraction_volume;
       rainfall_volume += rate*dt*roof_area
```

Roof params source: `roof_abstraction_capacity` / `roof_storage_capacity` from `RunoffFields` (per cell); `roof_drain_capacity` from `RoofDrainageMap` (the routing config, single source of truth for the physical drain limit).

## File Structure

| File | Responsibility |
|---|---|
| `.../runoff/roof_step.hpp` + `roof_step.cpp` | `RoofDrainageAcceptanceFn` port alias, `RoofStepInputs {RoofDrainageMap map; RoofDrainageAcceptanceFn accept;}` + `for_mesh` + validator; `apply_roof_runoff_stage(...)` |
| `step.hpp` / `step.cpp` (modify) | roof audit fields on `StepDiagnostics`; roof-aware `advance_one_step_cpu` overload |
| `tests/unit/surface2d/test_runoff_roof_step.cpp` (new) | mock-policy integration tests |

---

## Task 1: RoofStepInputs port + StepDiagnostics roof audit fields

**Files:**
- Create: `libs/surface2d/include/surface2d/source_terms/runoff/roof_step.hpp`, `.../src/.../roof_step.cpp`
- Modify: `step.hpp` (StepDiagnostics roof fields)
- Test: `tests/unit/surface2d/test_runoff_roof_step.cpp` (part 1)
- Modify: both CMakeLists

- [ ] **Step 1: Create `roof_step.hpp`**

```cpp
#pragma once

#include <functional>
#include <vector>

#include "core/types.hpp"
#include "mesh/mesh.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/geometry/cache.hpp"
#include "surface2d/source_terms/runoff/fields.hpp"
#include "surface2d/source_terms/runoff/result.hpp"
#include "surface2d/source_terms/runoff/step_inputs.hpp"
#include "surface2d/state/state.hpp"

namespace scau::surface2d {

struct StepConfig;        // fwd (defined in time_integration/step.hpp)
struct StepDiagnostics;   // fwd

// CouplingLib acceptance port: given a roof drainage intent, return the volume
// CouplingLib accepted into the pipe network. Mocked in tests; a real SWMM
// adapter (q = accepted/dt_sub) plugs in here once M240 is on the base.
using RoofDrainageAcceptanceFn = std::function<RoofDrainageAcceptance(const RoofDrainageIntent&)>;

// Per-step roof routing + the acceptance port.
struct RoofStepInputs {
    RoofDrainageMap map;
    RoofDrainageAcceptanceFn accept;

    // Neutral default: no roof targets (-1), an accept-nothing policy.
    [[nodiscard]] static RoofStepInputs for_mesh(const mesh::Mesh& mesh);
};

void validate_roof_step_inputs_match_mesh(const RoofStepInputs& inputs, const mesh::Mesh& mesh);

// Roof runoff stage (M247-D): per cell with roof_fraction>0, run evaluate_roof_emit,
// emit an intent, get acceptance from inputs.accept, apply it, route overflow to the
// mapped surface cell's h, scatter roof state, and audit. Uses runoff.fields for roof
// abstraction/storage capacities and inputs.map for drain capacity / node / overflow cell.
void apply_roof_runoff_stage(
    SurfaceState& state,
    const StepConfig& config,
    const DpmFields& dpm_fields,
    const RunoffStepInputs& runoff,
    const RoofStepInputs& roof,
    RunoffState& runoff_state,
    const GeometryCache& geometry,
    StepDiagnostics& diagnostics);

}  // namespace scau::surface2d
```

- [ ] **Step 2: Add roof audit fields to `StepDiagnostics` in `step.hpp`** (after `depression_storage_delta_volume`, before `exchange_volume`):

```cpp
    core::Real roof_to_swmm_requested_volume{0.0};
    core::Real roof_to_swmm_accepted_volume{0.0};
    core::Real roof_to_swmm_rejected_volume{0.0};
    core::Real roof_pending_delta_volume{0.0};
    core::Real roof_overflow_to_surface_volume{0.0};
```

- [ ] **Step 3: Failing test `test_runoff_roof_step.cpp` (part 1)**

```cpp
#include <stdexcept>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/source_terms/runoff/roof_step.hpp"

namespace {
using scau::surface2d::RoofStepInputs;
}  // namespace

TEST(RoofStepInputs, ForMeshHasNeutralDefaults) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto in = RoofStepInputs::for_mesh(mesh);
    ASSERT_EQ(in.map.swmm_node_index.size(), mesh.cells.size());
    for (std::size_t i = 0; i < mesh.cells.size(); ++i) {
        EXPECT_EQ(in.map.swmm_node_index[i], -1);
    }
    ASSERT_TRUE(static_cast<bool>(in.accept));  // accept-nothing fn is set
}

TEST(RoofStepInputs, NullAcceptFailsClosed) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto in = RoofStepInputs::for_mesh(mesh);
    in.accept = nullptr;
    EXPECT_THROW(scau::surface2d::validate_roof_step_inputs_match_mesh(in, mesh), std::invalid_argument);
}
```

- [ ] **Step 4: CMake.** `libs/surface2d/CMakeLists.txt` add `src/source_terms/runoff/roof_step.cpp`. `tests/unit/surface2d/CMakeLists.txt` append a `test_runoff_roof_step` target (same shape as other test targets).

- [ ] **Step 5: Build RED** `... --target test_runoff_roof_step` → FAIL.

- [ ] **Step 6: Create `roof_step.cpp` (Task-1 portion: for_mesh + validator only; the stage comes in Task 2)**

```cpp
#include "surface2d/source_terms/runoff/roof_step.hpp"

#include <stdexcept>

namespace scau::surface2d {

RoofStepInputs RoofStepInputs::for_mesh(const mesh::Mesh& mesh) {
    RoofStepInputs inputs;
    inputs.map = RoofDrainageMap::for_mesh(mesh);
    // Accept-nothing default: every request is fully rejected (CapacityLimited).
    inputs.accept = [](const RoofDrainageIntent& intent) {
        return RoofDrainageAcceptance{
            .requested_volume = intent.requested_volume,
            .accepted_volume = 0.0,
            .rejected_volume = intent.requested_volume,
            .rejection_reason = RoofDrainageRejectionReason::CapacityLimited,
        };
    };
    return inputs;
}

void validate_roof_step_inputs_match_mesh(const RoofStepInputs& inputs, const mesh::Mesh& mesh) {
    validate_roof_drainage_map_match_mesh(inputs.map, mesh);
    if (!inputs.accept) {
        throw std::invalid_argument("roof step acceptance function must be set");
    }
}

}  // namespace scau::surface2d
```

- [ ] **Step 7: Build + run** `... --target test_runoff_roof_step` then `ctest ... -R test_runoff_roof_step --output-on-failure` → PASS 2/2. Then regression `ctest ... -R "surface|runoff|step" --output-on-failure` → green.

- [ ] **Step 8: Commit**
```bash
git add libs/surface2d/include/surface2d/source_terms/runoff/roof_step.hpp libs/surface2d/src/source_terms/runoff/roof_step.cpp libs/surface2d/include/surface2d/time_integration/step.hpp tests/unit/surface2d/test_runoff_roof_step.cpp libs/surface2d/CMakeLists.txt tests/unit/surface2d/CMakeLists.txt
git commit -m "feat(surface2d): add RoofStepInputs acceptance port + StepDiagnostics roof audit fields (M247-D)"
```

---

## Task 2: apply_roof_runoff_stage

**Files:**
- Modify: `roof_step.cpp` (add the stage)
- Test: `test_runoff_roof_step.cpp` (extend)

- [ ] **Step 1: Failing test** (append). Includes: `surface2d/time_integration/step.hpp`, `surface2d/dpm/fields.hpp`, `surface2d/geometry/cache.hpp`, `surface2d/source_terms/runoff/fields.hpp`, `surface2d/state/state.hpp`.

```cpp
TEST(RoofRunoffStage, FullAcceptDrainsPendingNoSurface) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    const auto dpm = scau::surface2d::DpmFields::for_mesh(mesh);
    const auto geom = scau::surface2d::GeometryCache::for_mesh(mesh);
    auto rs = scau::surface2d::RunoffState::for_mesh(mesh);
    auto runoff = scau::surface2d::RunoffStepInputs::for_mesh(mesh);
    auto roof = scau::surface2d::RoofStepInputs::for_mesh(mesh);
    for (std::size_t i = 0; i < mesh.cells.size(); ++i) {
        runoff.fields.pervious_fraction[i] = 0.0;
        runoff.fields.roof_fraction[i] = 1.0;
        runoff.fields.roof_storage_capacity[i] = 1.0e-2;
        runoff.rainfall_rate[i] = 2.0e-3;
        roof.map.roof_drain_capacity[i] = 1.0e6;   // effectively unlimited drain
        roof.map.swmm_node_index[i] = 0;
    }
    roof.accept = [](const scau::surface2d::RoofDrainageIntent& in) {
        return scau::surface2d::RoofDrainageAcceptance{
            .requested_volume = in.requested_volume, .accepted_volume = in.requested_volume,
            .rejected_volume = 0.0, .rejection_reason = scau::surface2d::RoofDrainageRejectionReason::None};
    };
    const scau::surface2d::StepConfig config{.dt = 0.5, .h_min = 1.0e-8};
    scau::surface2d::StepDiagnostics diag;
    diag.cell_count = mesh.cells.size();

    const double h_before = state.cells[0].conserved.h;
    scau::surface2d::apply_roof_runoff_stage(state, config, dpm, runoff, roof, rs, geom, diag);

    double roof_rain = 0.0;
    for (const double a : geom.cell_areas) roof_rain += 2.0e-3 * 0.5 * a;
    EXPECT_NEAR(diag.roof_to_swmm_accepted_volume, roof_rain, 1.0e-9);  // all roof water drained
    EXPECT_NEAR(diag.roof_overflow_to_surface_volume, 0.0, 1.0e-12);
    EXPECT_NEAR(state.cells[0].conserved.h, h_before, 1.0e-12);          // no surface added
    for (const auto v : rs.roof_pending_volume) EXPECT_NEAR(v, 0.0, 1.0e-12);
}

TEST(RoofRunoffStage, RejectAccumulatesPendingThenOverflowsToSurface) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    const auto dpm = scau::surface2d::DpmFields::for_mesh(mesh);
    const auto geom = scau::surface2d::GeometryCache::for_mesh(mesh);
    auto rs = scau::surface2d::RunoffState::for_mesh(mesh);
    auto runoff = scau::surface2d::RunoffStepInputs::for_mesh(mesh);
    auto roof = scau::surface2d::RoofStepInputs::for_mesh(mesh);
    for (std::size_t i = 0; i < mesh.cells.size(); ++i) {
        runoff.fields.pervious_fraction[i] = 0.0;
        runoff.fields.roof_fraction[i] = 1.0;
        runoff.fields.roof_storage_capacity[i] = 0.0;   // zero pending buffer -> immediate overflow
        runoff.rainfall_rate[i] = 2.0e-3;
        roof.map.roof_drain_capacity[i] = 1.0e6;
        roof.map.swmm_node_index[i] = 0;
        roof.map.roof_overflow_to_surface_cell_index[i] = static_cast<int>(i);  // overflow to self
    }
    // accept nothing -> all roof water is rejected, stays pending, then overflows (storage cap 0)
    roof.accept = [](const scau::surface2d::RoofDrainageIntent& in) {
        return scau::surface2d::RoofDrainageAcceptance{
            .requested_volume = in.requested_volume, .accepted_volume = 0.0,
            .rejected_volume = in.requested_volume, .rejection_reason = scau::surface2d::RoofDrainageRejectionReason::NodeSurcharged};
    };
    const scau::surface2d::StepConfig config{.dt = 0.5, .h_min = 1.0e-8};
    scau::surface2d::StepDiagnostics diag;
    diag.cell_count = mesh.cells.size();
    scau::surface2d::apply_roof_runoff_stage(state, config, dpm, runoff, roof, rs, geom, diag);

    double roof_rain = 0.0;
    for (const double a : geom.cell_areas) roof_rain += 2.0e-3 * 0.5 * a;
    EXPECT_NEAR(diag.roof_to_swmm_accepted_volume, 0.0, 1.0e-12);
    EXPECT_NEAR(diag.roof_overflow_to_surface_volume, roof_rain, 1.0e-9);  // all overflowed to surface
    for (const auto v : rs.roof_pending_volume) EXPECT_NEAR(v, 0.0, 1.0e-9); // storage cap 0 -> nothing retained
}
```

- [ ] **Step 2: Build RED** → FAIL (`apply_roof_runoff_stage` unresolved).

- [ ] **Step 3: Implement `apply_roof_runoff_stage` in `roof_step.cpp`.** Add includes: `surface2d/source_terms/runoff/runoff_generation.hpp`, `surface2d/time_integration/step.hpp`.

```cpp
void apply_roof_runoff_stage(
    SurfaceState& state,
    const StepConfig& config,
    const DpmFields& dpm_fields,
    const RunoffStepInputs& runoff,
    const RoofStepInputs& roof,
    RunoffState& runoff_state,
    const GeometryCache& geometry,
    StepDiagnostics& diagnostics) {
    for (std::size_t i = 0; i < state.cells.size(); ++i) {
        const core::Real area = geometry.cell_areas[i];
        if (area <= 0.0 || runoff.fields.roof_fraction[i] <= 0.0) {
            continue;
        }
        const core::Real phi_t = dpm_fields.cells[i].phi_t;

        RunoffCellInputs cell_inputs{.rainfall_rate = runoff.rainfall_rate[i], .phi_t = phi_t, .cell_area = area};
        RunoffCellParams params{
            .pervious_fraction = runoff.fields.pervious_fraction[i],
            .impervious_fraction = runoff.fields.impervious_fraction[i],
            .roof_fraction = runoff.fields.roof_fraction[i],
            .initial_abstraction_capacity = runoff.fields.initial_abstraction_capacity[i],
            .depression_storage_capacity = runoff.fields.depression_storage_capacity[i],
            .roof_abstraction_capacity = runoff.fields.roof_abstraction_capacity[i],
            .roof_storage_capacity = runoff.fields.roof_storage_capacity[i],
            .roof_drain_capacity = roof.map.roof_drain_capacity[i],
            .soil = runoff.lut.at(runoff.fields.soil_type[i]),
        };
        RunoffCellState cell_state{
            .cumulative_infiltration = runoff_state.cumulative_infiltration[i],
            .ponding_time = runoff_state.ponding_time[i],
            .abstraction_filled = runoff_state.abstraction_filled[i],
            .depression_storage_filled = runoff_state.depression_storage_filled[i],
            .roof_abstraction_filled = runoff_state.roof_abstraction_filled[i],
            .roof_pending_volume = runoff_state.roof_pending_volume[i],
        };

        const RoofEmitResult emit = evaluate_roof_emit(cell_inputs, params, cell_state, config.dt);
        const RoofDrainageIntent intent{
            .source_cell_index = static_cast<int>(i),
            .target_swmm_node_index = roof.map.swmm_node_index[i],
            .requested_volume = emit.requested_volume,
            .source_roof_area = params.roof_fraction * area,
        };
        const RoofDrainageAcceptance acc = roof.accept(intent);
        const int overflow_cell = roof.map.roof_overflow_to_surface_cell_index[i];
        const bool overflow_valid = overflow_cell >= 0;
        const RoofAcceptanceResult ar = apply_roof_drainage_acceptance(cell_inputs, params, cell_state, acc, overflow_valid);

        if (ar.overflow_to_surface_volume > 0.0 && overflow_valid) {
            const std::size_t oc = static_cast<std::size_t>(overflow_cell);
            const core::Real oc_phi_t = dpm_fields.cells[oc].phi_t;
            const core::Real oc_area = geometry.cell_areas[oc];
            state.cells[oc].conserved.h += ar.overflow_to_surface_volume / (oc_phi_t * oc_area);
        }

        runoff_state.roof_abstraction_filled[i] = cell_state.roof_abstraction_filled;
        runoff_state.roof_pending_volume[i] = cell_state.roof_pending_volume;

        diagnostics.rainfall_volume += cell_inputs.rainfall_rate * config.dt * (params.roof_fraction * area);
        diagnostics.abstraction_volume += emit.roof_abstraction_volume;
        diagnostics.roof_to_swmm_requested_volume += emit.requested_volume;
        diagnostics.roof_to_swmm_accepted_volume += ar.accepted_volume;
        diagnostics.roof_to_swmm_rejected_volume += ar.rejected_volume;
        diagnostics.roof_pending_delta_volume += emit.roof_input_volume + ar.pending_delta_volume;
        diagnostics.roof_overflow_to_surface_volume += ar.overflow_to_surface_volume;
        if (ar.missing_overflow_target) {
            diagnostics.flags_missing_roof_overflow_target = true;  // see note
        }
    }
}
```
> NOTE on `flags_missing_roof_overflow_target`: `StepDiagnostics` has no flags struct. Either (a) add a single `bool missing_roof_overflow_target{false};` to `StepDiagnostics` in Task 1 Step 2 and set it here, or (b) drop the `if (ar.missing_overflow_target)` block. Choose (a): add the bool field in Task 1 and set it here. (Keep the plan consistent: add `bool missing_roof_overflow_target{false};` to the Task 1 Step 2 field list.)

- [ ] **Step 4: Build + run** `... --target test_runoff_roof_step` then `ctest ... -R test_runoff_roof_step --output-on-failure` → PASS 4/4.

- [ ] **Step 5: Commit**
```bash
git add libs/surface2d/include/surface2d/source_terms/runoff/roof_step.hpp libs/surface2d/src/source_terms/runoff/roof_step.cpp libs/surface2d/include/surface2d/time_integration/step.hpp tests/unit/surface2d/test_runoff_roof_step.cpp
git commit -m "feat(surface2d): add apply_roof_runoff_stage (emit->accept->apply->overflow->audit) (M247-D)"
```

---

## Task 3: Roof-aware advance_one_step_cpu overload

**Files:** modify `step.hpp` (declaration), `step.cpp` (definition); test `test_runoff_roof_step.cpp` (full-step + rollback).

- [ ] **Step 1: Declare** in `step.hpp` (after the M247-C 9-arg runoff overload). Add `#include "surface2d/source_terms/runoff/roof_step.hpp"`.

```cpp
// Roof-aware hot-path overload (M247-D). Order:
//   flux -> ground runoff -> roof runoff -> coupling exchange -> friction.
// runoff_state (ground + roof) mutated only on an accepted step. roof.accept is
// the CouplingLib acceptance port (mock in tests; real SWMM adapter later).
[[nodiscard]] StepDiagnostics advance_one_step_cpu(
    const mesh::Mesh& mesh,
    SurfaceState& state,
    const StepConfig& config,
    const DpmFields& dpm_fields,
    const BoundaryConditions& boundary,
    const SourceTermFields& sources,
    const GeometryCache& geometry,
    const RunoffStepInputs& runoff_inputs,
    const RoofStepInputs& roof_inputs,
    RunoffState& runoff_state);
```

- [ ] **Step 2: Failing test** (append): a closed-box where roof water is fully accepted (conserved out via roof_to_swmm_accepted) and a rollback test (large depth, tiny c_rollback) asserting `roof_pending_volume` unchanged and `roof_to_swmm_accepted_volume == 0`. (Model the two on the M247-C `RunoffStep` tests, swapping the call to the 10-arg overload and asserting roof audit fields.)

- [ ] **Step 3: Build RED** → FAIL.

- [ ] **Step 4: Define** in `step.cpp` (after the M247-C runoff overload):
```cpp
StepDiagnostics advance_one_step_cpu(
    const mesh::Mesh& mesh, SurfaceState& state, const StepConfig& config,
    const DpmFields& dpm_fields, const BoundaryConditions& boundary,
    const SourceTermFields& sources, const GeometryCache& geometry,
    const RunoffStepInputs& runoff_inputs, const RoofStepInputs& roof_inputs,
    RunoffState& runoff_state) {
    validate_step_inputs(mesh, state, config);
    validate_dpm_fields_match_mesh(dpm_fields, mesh);
    validate_boundary_conditions_match_mesh(boundary, mesh);
    validate_source_term_fields_match_mesh(sources, mesh);
    validate_geometry_cache_matches_mesh(geometry, mesh);
    validate_runoff_step_inputs_match_mesh(runoff_inputs, mesh);
    validate_roof_step_inputs_match_mesh(roof_inputs, mesh);
    validate_runoff_state_match_mesh(runoff_state, mesh);

    auto diagnostics = run_flux_core(mesh, state, config, dpm_fields, boundary, geometry);
    if (diagnostics.rollback_required) {
        return diagnostics;  // runoff_state + h untouched
    }
    apply_ground_runoff_stage(state, config, dpm_fields, runoff_inputs, runoff_state, geometry, diagnostics);
    apply_roof_runoff_stage(state, config, dpm_fields, runoff_inputs, roof_inputs, runoff_state, geometry, diagnostics);
    apply_coupling_and_friction(state, config, dpm_fields, sources, geometry, diagnostics);
    return diagnostics;
}
```

- [ ] **Step 5: Build + run** `... --target test_runoff_roof_step` → PASS; then FULL regression `ctest --preset windows-msvc --output-on-failure` → all green (0 LNK1168).

- [ ] **Step 6: Commit**
```bash
git add libs/surface2d/include/surface2d/time_integration/step.hpp libs/surface2d/src/time_integration/step.cpp tests/unit/surface2d/test_runoff_roof_step.cpp
git commit -m "feat(surface2d): add roof-aware advance_one_step_cpu overload (flux->ground->roof->exchange->friction) (M247-D)"
```

---

## Self-Review (completed during authoring)

**1. Spec coverage:** §3.4 single-buffer roof chain through the step (Task 2); §4.6 `RoofDrainageIntent`/`RoofDrainageAcceptance` exchanged via the acceptance port (Tasks 1-2); §5 order `flux→ground→roof→exchange→friction` + rollback-safe (Task 3); §6.2 roof audit terms (`roof_to_swmm_*`, `roof_pending_delta`, `roof_overflow_to_surface`, roof abstraction folded into `abstraction_volume`) (Tasks 1-2). Out of scope (documented): real SWMM adapter / `q=accepted/dt_sub` node write (needs M240 on base), per-step intent batching (the port is called per-cell here — a batched variant is a later refinement).

**2. Placeholder scan:** Task 3 Step 2 describes the two tests by analogy to existing M247-C `RunoffStep` tests rather than pasting them — acceptable because the referenced tests are in-repo (`test_runoff_step.cpp`) and the assertions are named explicitly (roof audit fields, `roof_pending_volume` unchanged on rollback). All Task 1-2 code/tests are complete. The `flags_missing_roof_overflow_target` note resolves to a concrete decision (add a bool field in Task 1).

**3. Type consistency:** `RoofStepInputs`/`RoofDrainageAcceptanceFn` defined Task 1, used Tasks 2-3. `apply_roof_runoff_stage` signature identical across decl (Task 1), defn (Task 2), call (Task 3). `RoofDrainageIntent`/`RoofDrainageAcceptance`/`RoofEmitResult`/`RoofAcceptanceResult`/`evaluate_roof_emit`/`apply_roof_drainage_acceptance` reused from M247-B exactly. New `StepDiagnostics` roof fields defined Task 1, populated Task 2, asserted Tasks 2-3.

## Decisions locked

- **Ports-and-adapters:** acceptance is a `std::function` port owned by surface2d; mock in tests; real CouplingLib/SWMM adapter (writes node lateral inflow) deferred until M240 is on the base. No virtual class, no surface2d->coupling dependency.
- Roof stage after ground, before exchange. Overflow routes to the `RoofDrainageMap`-mapped surface cell's `h` at zero momentum.
- `roof_drain_capacity` sourced from `RoofDrainageMap`; abstraction/storage capacities from `RunoffFields`.
- `StepDiagnostics` gains 5 roof volume fields + 1 `missing_roof_overflow_target` bool.

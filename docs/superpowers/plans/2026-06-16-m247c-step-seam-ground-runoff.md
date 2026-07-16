# M247-C Step-Seam Ground Runoff Integration Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Wire the ground runoff chain (`evaluate_ground_runoff`) into `libs/surface2d` `step.cpp`'s source seam, replacing M241's separate rainfall+infiltration blocks and superseding their order, with SoA gather/scatter of `RunoffState` and migrated audit/tests. Roof chain stays out (M247-D).

**Architecture:** Adds a runoff-aware `advance_one_step_cpu` overload whose source order is `flux → ground runoff → coupling exchange → friction` (supersedes M241's `flux → rainfall → exchange → infiltration → friction`). Per cell it gathers the ground portion of the SoA `RunoffState` into a scalar `RunoffCellState`, runs `evaluate_ground_runoff`, adds the net ground runoff depth to `h` (zero momentum), and scatters state back. Rollback-safe: the runoff stage runs only after the existing pre-mutation `rollback_required` early-return. The final task removes M241's rainfall/infiltration from `SourceTermFields` and the old blocks so only one runoff accounting path remains.

**Tech Stack:** C++20, CMake (preset `windows-msvc`, `VCPKG_ROOT=H:/githubcode/vcpkg`), GoogleTest. `core::Real` (double). Namespace `scau::surface2d`.

**Builds on:** `feat/surface2d-foundation` (master + surface2d foundation + runoff A/B kernel). **Authoritative spec:** `superpowers/specs/2026-06-13-m247-urban-runoff-generation-design.md` §5.

---

## Hard Constraints (every task)

- **ASCII-only** comments/code. **`EXPECT_NEAR`** (not `EXPECT_DOUBLE_EQ`) on recomposed volumes, tol ~1e-9. **Fail-closed** validation, `std::invalid_argument`. `std::size_t` indices, `[[nodiscard]]`, namespace `scau::surface2d`.
- Build/test with the `VCPKG_ROOT=H:/githubcode/vcpkg` prefix. Append-only CMake. After any killed ctest, verify `grep -c LNK1168 == 0`.
- **Rollback invariant:** the runoff stage must run only inside the accepted-step path (after the `if (diagnostics.rollback_required) return;` at `step.cpp` ~line 320), so `RunoffState` is never mutated on a rolled-back step.
- **Scope:** ground chain only. Do NOT call `evaluate_roof_emit` / `apply_roof_drainage_acceptance` / emit intents here — that is M247-D.
- **Ponded-water infiltration is out of scope:** `evaluate_ground_runoff` infiltrates only this substep's rainfall; existing ponded `h` is not drained by infiltration in this milestone (documented limitation; the coupling to live `h` is a later refinement).

## Key behavioral change vs M241

| | M241 (current) | M247-C |
|---|---|---|
| rainfall | `SourceTermFields.rainfall_rate` → `h += rate*dt/phi_t` | `RunoffStepInputs.rainfall_rate` → ground chain → `h += surface_added/(phi_t*A)` |
| infiltration | `SourceTermFields.infiltration_rate` constant-rate, decrements `h` | Green-Ampt inside ground chain, consumes rain budget (does not decrement existing `h`) |
| order | flux → rainfall → exchange → infiltration → friction | flux → ground runoff → exchange → friction |
| diagnostics | `rainfall_volume`, `infiltration_volume`, `exchange_volume` | `rainfall_volume`(gross ground), `surface_added_volume`, `infiltration_volume`, `abstraction_volume`, `depression_storage_delta_volume`, `exchange_volume` |

## File Structure

| File | Responsibility |
|---|---|
| `.../runoff/step_inputs.hpp` + `step_inputs.cpp` | `RunoffStepInputs` bundle (`RunoffFields` + `SoilParamsLUT` + per-cell `rainfall_rate` + `f_inf_floor`) + `for_mesh` + mesh-match validator |
| `step.hpp` / `step.cpp` (modify) | new `StepDiagnostics` runoff fields; `apply_ground_runoff_stage`; runoff-aware `advance_one_step_cpu` overload; (Task 5) remove M241 rainfall/infiltration |
| `source_terms/fields.hpp` / `.cpp` (modify, Task 5) | drop `rainfall_rate` + `infiltration_rate` |
| `tests/unit/surface2d/test_runoff_step.cpp` (new) | step-level ground-runoff integration + rollback safety |
| `tests/unit/surface2d/test_rainfall_infiltration_source.cpp` (modify, Task 4/5) | migrate step-level rain test onto the runoff path; drop constant-rate infiltration step tests |

---

## Task 1: RunoffStepInputs bundle + StepDiagnostics runoff fields

**Files:**
- Create: `libs/surface2d/include/surface2d/source_terms/runoff/step_inputs.hpp`, `.../src/source_terms/runoff/step_inputs.cpp`
- Modify: `libs/surface2d/include/surface2d/time_integration/step.hpp` (StepDiagnostics fields)
- Test: `tests/unit/surface2d/test_runoff_step.cpp` (part 1)
- Modify: both CMakeLists

- [ ] **Step 1: Create `step_inputs.hpp`**

```cpp
#pragma once

#include <vector>

#include "core/types.hpp"
#include "mesh/mesh.hpp"
#include "surface2d/source_terms/runoff/fields.hpp"
#include "surface2d/source_terms/runoff/soil_params.hpp"

namespace scau::surface2d {

// Per-step runoff configuration handed to the runoff-aware advance_one_step_cpu.
// rainfall_rate is per-cell (m/s); fields/lut/f_inf_floor parameterise the
// ground chain. RunoffState (the mutable per-cell state) is passed separately.
struct RunoffStepInputs {
    RunoffFields fields;
    SoilParamsLUT lut;
    std::vector<core::Real> rainfall_rate;
    core::Real f_inf_floor{1.0e-4};

    // Neutral default: fully pervious fields, single valid soil entry, zero rain.
    [[nodiscard]] static RunoffStepInputs for_mesh(const mesh::Mesh& mesh);
};

void validate_runoff_step_inputs_match_mesh(const RunoffStepInputs& inputs, const mesh::Mesh& mesh);

}  // namespace scau::surface2d
```

- [ ] **Step 2: Add runoff fields to `StepDiagnostics` in `step.hpp`**

Replace the existing audit-fields block (currently `rainfall_volume`, `infiltration_volume`, `exchange_volume`) with:

```cpp
    // Plan volumes (m^3) actually applied this step, for system mass audit.
    // All zero when the step rolls back. Ground runoff closure (M247-C):
    // rainfall_volume == surface_added_volume + infiltration_volume
    //                    + abstraction_volume + depression_storage_delta_volume.
    core::Real rainfall_volume{0.0};            // gross rain on ground area
    core::Real surface_added_volume{0.0};       // net ground runoff added to h
    core::Real infiltration_volume{0.0};
    core::Real abstraction_volume{0.0};
    core::Real depression_storage_delta_volume{0.0};
    core::Real exchange_volume{0.0};
```

- [ ] **Step 3: Write the failing test `tests/unit/surface2d/test_runoff_step.cpp`** (Task-1 portion: bundle defaults + validation)

```cpp
#include <stdexcept>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/source_terms/runoff/step_inputs.hpp"

namespace {
using scau::surface2d::RunoffStepInputs;
}  // namespace

TEST(RunoffStepInputs, ForMeshHasNeutralDefaults) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto in = RunoffStepInputs::for_mesh(mesh);
    ASSERT_EQ(in.rainfall_rate.size(), mesh.cells.size());
    ASSERT_EQ(in.fields.pervious_fraction.size(), mesh.cells.size());
    ASSERT_FALSE(in.lut.entries.empty());
    EXPECT_GT(in.f_inf_floor, 0.0);
    for (std::size_t i = 0; i < mesh.cells.size(); ++i) {
        EXPECT_EQ(in.rainfall_rate[i], 0.0);
        EXPECT_EQ(in.fields.pervious_fraction[i], 1.0);
    }
}

TEST(RunoffStepInputs, MismatchedRainfallSizeFailsClosed) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto in = RunoffStepInputs::for_mesh(mesh);
    in.rainfall_rate.pop_back();
    EXPECT_THROW(scau::surface2d::validate_runoff_step_inputs_match_mesh(in, mesh), std::invalid_argument);
}
```

- [ ] **Step 4: Register in CMake.** `libs/surface2d/CMakeLists.txt` add `src/source_terms/runoff/step_inputs.cpp`. `tests/unit/surface2d/CMakeLists.txt` append:
```cmake
add_executable(test_runoff_step test_runoff_step.cpp)
target_link_libraries(test_runoff_step
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_runoff_step COMMAND test_runoff_step)
```

- [ ] **Step 5: Build to verify red.** `VCPKG_ROOT=H:/githubcode/vcpkg cmake --build --preset windows-msvc --target test_runoff_step` → FAIL (step_inputs.cpp missing).

- [ ] **Step 6: Create `step_inputs.cpp`**

```cpp
#include "surface2d/source_terms/runoff/step_inputs.hpp"

#include <stdexcept>

namespace scau::surface2d {

RunoffStepInputs RunoffStepInputs::for_mesh(const mesh::Mesh& mesh) {
    RunoffStepInputs inputs;
    inputs.fields = RunoffFields::for_mesh(mesh);
    inputs.rainfall_rate.assign(mesh.cells.size(), 0.0);
    inputs.f_inf_floor = 1.0e-4;
    // One neutral soil entry so soil_type 0 resolves; loam-ish defaults.
    inputs.lut.entries.push_back(SoilParams{.k_s = 1.0e-5, .psi_f = 0.1, .theta_s = 0.4, .theta_i = 0.1});
    return inputs;
}

void validate_runoff_step_inputs_match_mesh(const RunoffStepInputs& inputs, const mesh::Mesh& mesh) {
    if (inputs.rainfall_rate.size() != mesh.cells.size()) {
        throw std::invalid_argument("runoff step rainfall_rate size must match mesh cell count");
    }
    validate_runoff_fields_match_mesh(inputs.fields, mesh);
    validate_soil_params_lut(inputs.lut);
    if (!(inputs.f_inf_floor > 0.0)) {
        throw std::invalid_argument("runoff step f_inf_floor must be positive");
    }
}

}  // namespace scau::surface2d
```

- [ ] **Step 7: Build + run.** `... --target test_runoff_step` then `ctest ... -R test_runoff_step --output-on-failure` → PASS 2/2.

- [ ] **Step 8: Commit**
```bash
git add libs/surface2d/include/surface2d/source_terms/runoff/step_inputs.hpp \
        libs/surface2d/src/source_terms/runoff/step_inputs.cpp \
        libs/surface2d/include/surface2d/time_integration/step.hpp \
        tests/unit/surface2d/test_runoff_step.cpp \
        libs/surface2d/CMakeLists.txt tests/unit/surface2d/CMakeLists.txt
git commit -m "feat(surface2d): add RunoffStepInputs bundle + StepDiagnostics runoff audit fields (M247-C)"
```

> NOTE: After Step 2 the build of existing code that reads `diagnostics.infiltration_volume`/`rainfall_volume` still compiles (those fields remain). Code that the old `apply_source_terms` writes still compiles. The new `surface_added_volume` etc. are added, not yet populated. Existing tests referencing `rainfall_volume`/`infiltration_volume`/`exchange_volume` still pass until Task 4/5.

---

## Task 2: apply_ground_runoff_stage (gather/scatter + h update + audit)

**Files:**
- Modify: `libs/surface2d/src/time_integration/step.cpp` (add the stage function in the anonymous namespace, include runoff headers)
- Modify: `libs/surface2d/include/surface2d/time_integration/step.hpp` (declare the stage for unit testing)
- Test: `tests/unit/surface2d/test_runoff_step.cpp` (extend)

- [ ] **Step 1: Declare the stage in `step.hpp`** (before the `advance_one_step_cpu` declarations, after the includes add the runoff include and the declaration). Add include `#include "surface2d/source_terms/runoff/step_inputs.hpp"` and:

```cpp
// Ground runoff source stage (M247-C): per cell, gather the ground portion of
// runoff_state, run evaluate_ground_runoff using inputs.rainfall_rate and the
// per-cell fields/soil, add the net ground runoff depth to h (zero momentum),
// scatter state back, and accumulate the ground volume audit into diagnostics.
// Roof fields of runoff_state are not touched (M247-D owns the roof chain).
void apply_ground_runoff_stage(
    SurfaceState& state,
    const StepConfig& config,
    const DpmFields& dpm_fields,
    const RunoffStepInputs& inputs,
    RunoffState& runoff_state,
    const GeometryCache& geometry,
    StepDiagnostics& diagnostics);
```

- [ ] **Step 2: Write the failing test** (append to `test_runoff_step.cpp`)

```cpp
#include "mesh/mesh.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/geometry/cache.hpp"
#include "surface2d/source_terms/runoff/fields.hpp"
#include "surface2d/state/state.hpp"
#include "surface2d/time_integration/step.hpp"

TEST(GroundRunoffStage, ImperviousRainAddsNetRunoffDepth) {
    // Fully impervious cells, no losses: all rain becomes surface runoff.
    // dh = surface_added/(phi_t*A); surface_added = rate*dt*A (impervious).
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    const auto dpm = scau::surface2d::DpmFields::for_mesh(mesh);          // phi_t defaults to 1.0
    const auto geom = scau::surface2d::GeometryCache::for_mesh(mesh);
    auto rs = scau::surface2d::RunoffState::for_mesh(mesh);
    auto in = scau::surface2d::RunoffStepInputs::for_mesh(mesh);
    for (std::size_t i = 0; i < mesh.cells.size(); ++i) {
        in.fields.pervious_fraction[i] = 0.0;
        in.fields.impervious_fraction[i] = 1.0;
        in.rainfall_rate[i] = 2.0e-3;
    }
    const scau::surface2d::StepConfig config{.dt = 0.5, .h_min = 1.0e-8};
    scau::surface2d::StepDiagnostics diag;
    diag.cell_count = mesh.cells.size();

    scau::surface2d::apply_ground_runoff_stage(state, config, dpm, in, rs, geom, diag);

    double expected_surface = 0.0;
    for (const double a : geom.cell_areas) expected_surface += 2.0e-3 * 0.5 * a;
    EXPECT_NEAR(diag.surface_added_volume, expected_surface, 1.0e-9);
    EXPECT_NEAR(diag.infiltration_volume, 0.0, 1.0e-12);
    // each cell h rose by rate*dt/phi_t = 1e-3 above the initial 1.0
    for (const auto& c : state.cells) EXPECT_NEAR(c.conserved.h, 1.0 + 1.0e-3, 1.0e-9);
}

TEST(GroundRunoffStage, GroundClosureHolds) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    const auto dpm = scau::surface2d::DpmFields::for_mesh(mesh);
    const auto geom = scau::surface2d::GeometryCache::for_mesh(mesh);
    auto rs = scau::surface2d::RunoffState::for_mesh(mesh);
    auto in = scau::surface2d::RunoffStepInputs::for_mesh(mesh);
    for (std::size_t i = 0; i < mesh.cells.size(); ++i) {
        in.fields.pervious_fraction[i] = 0.5;
        in.fields.impervious_fraction[i] = 0.5;
        in.fields.initial_abstraction_capacity[i] = 1.0e-3;
        in.rainfall_rate[i] = 2.0e-3;
    }
    const scau::surface2d::StepConfig config{.dt = 1.0, .h_min = 1.0e-8};
    scau::surface2d::StepDiagnostics diag;
    diag.cell_count = mesh.cells.size();
    scau::surface2d::apply_ground_runoff_stage(state, config, dpm, in, rs, geom, diag);
    EXPECT_NEAR(diag.rainfall_volume,
                diag.surface_added_volume + diag.infiltration_volume +
                diag.abstraction_volume + diag.depression_storage_delta_volume,
                1.0e-9);
}
```

- [ ] **Step 3: Build to verify red.** `... --target test_runoff_step` → FAIL (`apply_ground_runoff_stage` unresolved).

- [ ] **Step 4: Implement in `step.cpp`.** Add includes near the top (after existing source_terms includes):
```cpp
#include "surface2d/source_terms/runoff/runoff_generation.hpp"
#include "surface2d/source_terms/runoff/step_inputs.hpp"
```
Add the function (outside the anonymous namespace, since it is declared in the header):
```cpp
void apply_ground_runoff_stage(
    SurfaceState& state,
    const StepConfig& config,
    const DpmFields& dpm_fields,
    const RunoffStepInputs& inputs,
    RunoffState& runoff_state,
    const GeometryCache& geometry,
    StepDiagnostics& diagnostics) {
    for (std::size_t i = 0; i < state.cells.size(); ++i) {
        const core::Real area = geometry.cell_areas[i];
        if (area <= 0.0) {
            continue;
        }
        const core::Real phi_t = dpm_fields.cells[i].phi_t;

        RunoffCellInputs cell_inputs{
            .rainfall_rate = inputs.rainfall_rate[i],
            .phi_t = phi_t,
            .cell_area = area,
        };
        RunoffCellParams params{
            .pervious_fraction = inputs.fields.pervious_fraction[i],
            .impervious_fraction = inputs.fields.impervious_fraction[i],
            .roof_fraction = inputs.fields.roof_fraction[i],
            .initial_abstraction_capacity = inputs.fields.initial_abstraction_capacity[i],
            .depression_storage_capacity = inputs.fields.depression_storage_capacity[i],
            .roof_abstraction_capacity = inputs.fields.roof_abstraction_capacity[i],
            .roof_storage_capacity = inputs.fields.roof_storage_capacity[i],
            .roof_drain_capacity = 0.0,  // roof chain is M247-D; ground stage ignores it
            .soil = inputs.lut.at(inputs.fields.soil_type[i]),
        };
        RunoffCellState cell_state{
            .cumulative_infiltration = runoff_state.cumulative_infiltration[i],
            .ponding_time = runoff_state.ponding_time[i],
            .abstraction_filled = runoff_state.abstraction_filled[i],
            .depression_storage_filled = runoff_state.depression_storage_filled[i],
            .roof_abstraction_filled = runoff_state.roof_abstraction_filled[i],
            .roof_pending_volume = runoff_state.roof_pending_volume[i],
        };

        const GroundRunoffResult g = evaluate_ground_runoff(
            cell_inputs, params, cell_state, config.dt, inputs.f_inf_floor);

        // Net ground runoff raises h at zero horizontal momentum.
        state.cells[i].conserved.h += g.surface_added_volume / (phi_t * area);

        // Scatter the ground portion of state back (roof fields unchanged here).
        runoff_state.cumulative_infiltration[i] = cell_state.cumulative_infiltration;
        runoff_state.ponding_time[i] = cell_state.ponding_time;
        runoff_state.abstraction_filled[i] = cell_state.abstraction_filled;
        runoff_state.depression_storage_filled[i] = cell_state.depression_storage_filled;

        const core::Real area_ground =
            (params.pervious_fraction + params.impervious_fraction) * area;
        diagnostics.rainfall_volume += cell_inputs.rainfall_rate * config.dt * area_ground;
        diagnostics.surface_added_volume += g.surface_added_volume;
        diagnostics.infiltration_volume += g.infiltration_volume;
        diagnostics.abstraction_volume += g.abstraction_volume;
        diagnostics.depression_storage_delta_volume += g.depression_storage_delta_volume;
    }
}
```

- [ ] **Step 5: Build + run.** `... --target test_runoff_step` then `ctest ... -R test_runoff_step --output-on-failure` → PASS 4/4.

- [ ] **Step 6: Commit**
```bash
git add libs/surface2d/include/surface2d/time_integration/step.hpp \
        libs/surface2d/src/time_integration/step.cpp \
        tests/unit/surface2d/test_runoff_step.cpp
git commit -m "feat(surface2d): add apply_ground_runoff_stage with SoA gather/scatter + ground audit (M247-C)"
```

---

## Task 3: Runoff-aware advance_one_step_cpu overload

**Files:**
- Modify: `step.hpp` (new overload declaration), `step.cpp` (definition)
- Test: `tests/unit/surface2d/test_runoff_step.cpp` (extend: full-step conservation + rollback safety)

- [ ] **Step 1: Declare the overload in `step.hpp`** (after the existing geometry overload):

```cpp
// Runoff-aware hot-path overload (M247-C). Same flux/CFL/rollback path as the
// SourceTermFields overload, but the source order is
//   flux -> ground runoff -> coupling exchange -> friction
// (supersedes M241's rainfall/infiltration). runoff_state is mutated only on an
// accepted step (untouched on rollback). sources supplies manning_n and
// exchange_volume; its rainfall/infiltration are NOT read on this path.
[[nodiscard]] StepDiagnostics advance_one_step_cpu(
    const mesh::Mesh& mesh,
    SurfaceState& state,
    const StepConfig& config,
    const DpmFields& dpm_fields,
    const BoundaryConditions& boundary,
    const SourceTermFields& sources,
    const GeometryCache& geometry,
    const RunoffStepInputs& runoff_inputs,
    RunoffState& runoff_state);
```

- [ ] **Step 2: Write the failing test** (append to `test_runoff_step.cpp`)

```cpp
#include "surface2d/boundary/conditions.hpp"
#include "surface2d/source_terms/fields.hpp"

TEST(RunoffStep, ClosedBoxRainConservesThroughRunoffPath) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    const auto dpm = scau::surface2d::DpmFields::for_mesh(mesh);
    const auto geom = scau::surface2d::GeometryCache::for_mesh(mesh);
    const auto boundary = scau::surface2d::BoundaryConditions::for_mesh(mesh);
    const auto sources = scau::surface2d::SourceTermFields::for_mesh(mesh);
    auto rs = scau::surface2d::RunoffState::for_mesh(mesh);
    auto in = scau::surface2d::RunoffStepInputs::for_mesh(mesh);
    for (std::size_t i = 0; i < mesh.cells.size(); ++i) {
        in.fields.pervious_fraction[i] = 0.0;
        in.fields.impervious_fraction[i] = 1.0;  // all rain runs off, exact accounting
        in.rainfall_rate[i] = 2.0e-3;
    }
    const scau::surface2d::StepConfig config{.dt = 0.5, .cfl_safety = 0.45, .c_rollback = 100.0, .h_min = 1.0e-8};

    const auto diag = scau::surface2d::advance_one_step_cpu(
        mesh, state, config, dpm, boundary, sources, geom, in, rs);

    ASSERT_FALSE(diag.rollback_required);
    double expected = 0.0;
    for (const double a : geom.cell_areas) expected += 2.0e-3 * 0.5 * a;
    EXPECT_NEAR(diag.surface_added_volume, expected, 1.0e-9);
    EXPECT_NEAR(diag.rainfall_volume, expected, 1.0e-9);
}

TEST(RunoffStep, RollbackLeavesRunoffStateUntouched) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    // Large depth + tiny c_rollback forces CFL rollback before any mutation.
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 10.0, 10.0);
    const auto dpm = scau::surface2d::DpmFields::for_mesh(mesh);
    const auto geom = scau::surface2d::GeometryCache::for_mesh(mesh);
    const auto boundary = scau::surface2d::BoundaryConditions::for_mesh(mesh);
    const auto sources = scau::surface2d::SourceTermFields::for_mesh(mesh);
    auto rs = scau::surface2d::RunoffState::for_mesh(mesh);
    for (auto& v : rs.cumulative_infiltration) v = 0.05;  // pre-existing state
    auto in = scau::surface2d::RunoffStepInputs::for_mesh(mesh);
    for (auto& r : in.rainfall_rate) r = 5.0e-3;
    const scau::surface2d::StepConfig config{.dt = 100.0, .cfl_safety = 0.45, .c_rollback = 1.0e-6, .h_min = 1.0e-8};

    const auto diag = scau::surface2d::advance_one_step_cpu(
        mesh, state, config, dpm, boundary, sources, geom, in, rs);

    ASSERT_TRUE(diag.rollback_required);
    EXPECT_EQ(diag.surface_added_volume, 0.0);
    for (const auto v : rs.cumulative_infiltration) EXPECT_EQ(v, 0.05);  // unchanged
}
```

- [ ] **Step 3: Build to verify red.** `... --target test_runoff_step` → FAIL (new overload unresolved).

- [ ] **Step 4: Define the overload in `step.cpp`** (place after the existing 7-arg geometry overload). It duplicates that overload's validated flux loop, then on the accepted path runs ground runoff → exchange → friction. To avoid copying the long flux loop, refactor: extract the existing flux-accumulation body of the 7-arg overload into a private helper `run_flux_and_updates(...)` that returns diagnostics and applies depth/momentum updates on accept, then both overloads call it and append their source stages. Concretely:

  a. In the anonymous namespace add a helper that contains everything the 7-arg overload does **up to and including** `apply_momentum_update` (i.e. lines 234-324 of the current body), returning `diagnostics` and leaving the caller to run source stages:

```cpp
StepDiagnostics run_flux_core(
    const mesh::Mesh& mesh,
    SurfaceState& state,
    const StepConfig& config,
    const DpmFields& dpm_fields,
    const BoundaryConditions& boundary,
    const GeometryCache& geometry) {
    auto diagnostics = base_diagnostics(mesh, state, config, boundary, geometry);
    diagnostics.edges.reserve(mesh.edges.size());
    // ... (move the existing edge loop body here verbatim from the 7-arg overload) ...
    if (diagnostics.rollback_required) {
        return diagnostics;
    }
    apply_depth_update(state, config, diagnostics.cells, geometry);
    apply_momentum_update(state, config, diagnostics.cells, geometry);
    return diagnostics;
}
```

  b. Rewrite the existing 7-arg `SourceTermFields` overload body to:
```cpp
    validate_step_inputs(mesh, state, config);
    validate_dpm_fields_match_mesh(dpm_fields, mesh);
    validate_boundary_conditions_match_mesh(boundary, mesh);
    validate_source_term_fields_match_mesh(sources, mesh);
    validate_geometry_cache_matches_mesh(geometry, mesh);
    auto diagnostics = run_flux_core(mesh, state, config, dpm_fields, boundary, geometry);
    if (diagnostics.rollback_required) {
        return diagnostics;
    }
    apply_source_terms(state, config, dpm_fields, sources, geometry, diagnostics);
    return diagnostics;
```

  c. Add the new runoff overload:
```cpp
StepDiagnostics advance_one_step_cpu(
    const mesh::Mesh& mesh,
    SurfaceState& state,
    const StepConfig& config,
    const DpmFields& dpm_fields,
    const BoundaryConditions& boundary,
    const SourceTermFields& sources,
    const GeometryCache& geometry,
    const RunoffStepInputs& runoff_inputs,
    RunoffState& runoff_state) {
    validate_step_inputs(mesh, state, config);
    validate_dpm_fields_match_mesh(dpm_fields, mesh);
    validate_boundary_conditions_match_mesh(boundary, mesh);
    validate_source_term_fields_match_mesh(sources, mesh);
    validate_geometry_cache_matches_mesh(geometry, mesh);
    validate_runoff_step_inputs_match_mesh(runoff_inputs, mesh);
    validate_runoff_state_match_mesh(runoff_state, mesh);

    auto diagnostics = run_flux_core(mesh, state, config, dpm_fields, boundary, geometry);
    if (diagnostics.rollback_required) {
        return diagnostics;  // runoff_state untouched
    }
    apply_ground_runoff_stage(state, config, dpm_fields, runoff_inputs, runoff_state, geometry, diagnostics);
    apply_coupling_and_friction(state, config, dpm_fields, sources, geometry, diagnostics);
    return diagnostics;
}
```

  d. Because the new path needs exchange + friction WITHOUT the M241 rainfall/infiltration, extract those two blocks from `apply_source_terms` into a helper `apply_coupling_and_friction(...)` (exchange block + the friction/dry-limiter tail of the current `apply_source_terms`), and have the old `apply_source_terms` call rainfall + infiltration + `apply_coupling_and_friction`. Full helper:
```cpp
void apply_coupling_and_friction(
    SurfaceState& state,
    const StepConfig& config,
    const DpmFields& dpm_fields,
    const SourceTermFields& sources,
    const GeometryCache& geometry,
    StepDiagnostics& diagnostics) {
    for (std::size_t cell_index = 0; cell_index < state.cells.size(); ++cell_index) {
        const core::Real area = geometry.cell_areas[cell_index];
        if (area <= 0.0) {
            continue;
        }
        auto& cell = state.cells[cell_index];
        const core::Real phi_t = dpm_fields.cells[cell_index].phi_t;
        if (sources.exchange_volume[cell_index] != 0.0) {
            const auto exchange = exchange_depth_increment(
                sources.exchange_volume[cell_index], cell.conserved.h, phi_t, area);
            cell.conserved.h += exchange.depth_increment;
            diagnostics.exchange_volume += exchange.applied_volume;
        }
        cell.conserved = apply_manning_friction(
            apply_dry_cell_momentum_limit(cell.conserved, config.h_min),
            sources.manning_n[cell_index], config.dt, config.h_min, config.gravity);
    }
}
```
  (The old `apply_source_terms` keeps the rainfall + infiltration loops then calls `apply_coupling_and_friction`; this preserves M241 behavior for the non-runoff overloads until Task 5 removes it.)

- [ ] **Step 5: Build + run.** `... --target test_runoff_step` then `ctest ... -R test_runoff_step --output-on-failure` → PASS 6/6. Then full surface2d regression `ctest ... -R "runoff|surface|dpm|cfl|hllc|wetting|boundary|momentum|geometry|friction|step|conservative|edge|pressure" --output-on-failure` → all green (existing M241 tests still pass via the unchanged old overload).

- [ ] **Step 6: Commit**
```bash
git add libs/surface2d/include/surface2d/time_integration/step.hpp libs/surface2d/src/time_integration/step.cpp tests/unit/surface2d/test_runoff_step.cpp
git commit -m "feat(surface2d): add runoff-aware advance_one_step_cpu overload (flux->runoff->exchange->friction) (M247-C)"
```

---

## Task 4: Migrate the rainfall step test onto the runoff path

**Files:**
- Modify: `tests/unit/surface2d/test_rainfall_infiltration_source.cpp`

- [ ] **Step 1: Replace the step-level rain test.** The pure-function tests (`rainfall_depth_increment`, `infiltration_depth_decrement`) stay. Replace `TEST(RainfallSource, ClosedBoxStepConservesRainfallVolume)` so it drives rain through the runoff overload instead of `SourceTermFields.rainfall_rate`:

```cpp
TEST(RainfallSource, ClosedBoxStepConservesRainfallVolumeViaRunoff) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    const auto dpm = scau::surface2d::DpmFields::for_mesh(mesh);
    const auto geom = scau::surface2d::GeometryCache::for_mesh(mesh);
    const auto boundary = scau::surface2d::BoundaryConditions::for_mesh(mesh);
    const auto sources = scau::surface2d::SourceTermFields::for_mesh(mesh);
    auto rs = scau::surface2d::RunoffState::for_mesh(mesh);
    auto in = scau::surface2d::RunoffStepInputs::for_mesh(mesh);
    for (std::size_t i = 0; i < mesh.cells.size(); ++i) {
        in.fields.pervious_fraction[i] = 0.0;
        in.fields.impervious_fraction[i] = 1.0;  // no infiltration -> all rain conserved into h
        in.rainfall_rate[i] = 2.0e-3;
    }
    const double before = system_volume(state, dpm, geom);
    double expected_added = 0.0;
    for (const double a : geom.cell_areas) expected_added += 2.0e-3 * 0.5 * a;
    const scau::surface2d::StepConfig config{.dt = 0.5, .cfl_safety = 0.45, .c_rollback = 100.0, .h_min = 1.0e-8};
    const auto diag = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm, boundary, sources, geom, in, rs);
    ASSERT_FALSE(diag.rollback_required);
    EXPECT_NEAR(system_volume(state, dpm, geom) - before, expected_added, 1.0e-9);
    EXPECT_NEAR(diag.surface_added_volume, expected_added, 1.0e-9);
}
```

Add the needed includes to the test file: `#include "surface2d/source_terms/runoff/fields.hpp"` and `#include "surface2d/source_terms/runoff/step_inputs.hpp"`.

- [ ] **Step 2: Delete the obsolete constant-rate infiltration STEP test** `TEST(InfiltrationSource, ClosedBoxStepRemovesClampedVolume)` (constant-rate step infiltration is superseded by Green-Ampt; the pure-function `infiltration_depth_decrement` unit test stays). Also delete `TEST(SourceTermFields, MismatchedSizesFailsClosed)`'s use of `rainfall_rate.pop_back()` only if Task 5 removes that field — leave it for now (still valid until Task 5), but change it in Task 5.

- [ ] **Step 3: Build + run.** `... --target test_rainfall_infiltration_source` then `ctest ... -R test_rainfall_infiltration_source --output-on-failure` → PASS.

- [ ] **Step 4: Commit**
```bash
git add tests/unit/surface2d/test_rainfall_infiltration_source.cpp
git commit -m "test(surface2d): migrate closed-box rain step test onto runoff path (M247-C)"
```

---

## Task 5: Supersede cleanup — remove M241 rainfall/infiltration from the seam

**Files:**
- Modify: `source_terms/fields.hpp` + `.cpp` (drop `rainfall_rate`, `infiltration_rate`)
- Modify: `step.cpp` (drop the rainfall + infiltration blocks from `apply_source_terms`; it becomes a thin wrapper over `apply_coupling_and_friction`)
- Modify: `tests/unit/surface2d/test_rainfall_infiltration_source.cpp` (fix the `SourceTermFields` size test to use a remaining field)

- [ ] **Step 1: Write/adjust the failing test.** In `test_rainfall_infiltration_source.cpp`, change the mismatched-size test to pop a field that still exists:
```cpp
TEST(SourceTermFields, MismatchedSizesFailsClosed) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    const auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    auto sources = scau::surface2d::SourceTermFields::for_mesh(mesh);
    sources.manning_n.pop_back();
    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 100.0, .h_min = 1.0e-8};
    const auto boundary = scau::surface2d::BoundaryConditions::for_mesh(mesh);
    EXPECT_THROW(
        static_cast<void>(scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields, boundary, sources)),
        std::invalid_argument);
}
```

- [ ] **Step 2: Remove fields from `source_terms/fields.hpp`** — `SourceTermFields` becomes:
```cpp
struct SourceTermFields {
    std::vector<core::Real> manning_n;
    std::vector<core::Real> exchange_volume;
    [[nodiscard]] static SourceTermFields for_mesh(const mesh::Mesh& mesh);
};
```
Update `fields.cpp` `for_mesh` (drop the two `assign`s) and `validate_source_term_fields_match_mesh` (drop the two size checks).

- [ ] **Step 3: Simplify `apply_source_terms` in `step.cpp`** — remove the rainfall and infiltration loops; it now just calls the shared helper:
```cpp
void apply_source_terms(
    SurfaceState& state,
    const StepConfig& config,
    const DpmFields& dpm_fields,
    const SourceTermFields& sources,
    const GeometryCache& geometry,
    StepDiagnostics& diagnostics) {
    apply_coupling_and_friction(state, config, dpm_fields, sources, geometry, diagnostics);
}
```
Remove the now-unused `#include` of `rainfall.hpp` / `infiltration.hpp` from `step.cpp` only if no longer referenced there (the pure functions still exist in their own TUs).

- [ ] **Step 4: Build + full regression.** `VCPKG_ROOT=H:/githubcode/vcpkg cmake --build --preset windows-msvc` (0 errors, 0 LNK1168) then `ctest --preset windows-msvc --output-on-failure` → all green. Fix any remaining caller of `SourceTermFields.rainfall_rate`/`infiltration_rate` (grep first: `grep -rn "rainfall_rate\|infiltration_rate" libs tests | grep -v runoff`).

- [ ] **Step 5: Commit**
```bash
git add libs/surface2d/include/surface2d/source_terms/fields.hpp libs/surface2d/src/source_terms/fields.cpp libs/surface2d/src/time_integration/step.cpp tests/unit/surface2d/test_rainfall_infiltration_source.cpp
git commit -m "refactor(surface2d): remove M241 rainfall/infiltration from source seam, superseded by runoff stage (M247-C)"
```

---

## Self-Review (completed during authoring)

**1. Spec coverage (§5):** step order `flux → runoff → exchange → friction` (Task 3); rainfall+infiltration merged into one runoff stage and old order superseded (Tasks 3+5); SoA `RunoffState` gather/scatter (Task 2); rollback-safety via pre-mutation early return (Task 3, tested `RollbackLeavesRunoffStateUntouched`); StepDiagnostics audit migration + ground closure (Tasks 1-2); `test_rainfall_infiltration_source` migration (Tasks 4-5). Out of scope (M247-D): roof emit/intent/acceptance, ponded-`h` infiltration coupling — explicitly stated.

**2. Placeholder scan:** No "TBD"/"add validation"/"similar to". Task 3 Step 4 references moving the existing edge loop "verbatim from the 7-arg overload" — this is a mechanical code move of code already present in the repo (shown at `step.cpp:240-319`), not a placeholder; the surrounding helper signatures and call sites are given in full.

**3. Type consistency:** `RunoffStepInputs` (Task 1) consumed unchanged in Tasks 2-4. `apply_ground_runoff_stage` signature identical in header decl (Task 2 Step 1) and definition (Task 2 Step 4) and call site (Task 3 Step 4c). `RunoffCellState`/`RunoffCellParams`/`RunoffCellInputs`/`GroundRunoffResult`/`evaluate_ground_runoff` reused from M247-B exactly. `apply_coupling_and_friction` defined once (Task 3 Step 4d) and reused by `apply_source_terms` (Task 5 Step 3). New `StepDiagnostics` fields (`surface_added_volume`, `abstraction_volume`, `depression_storage_delta_volume`) defined Task 1, populated Task 2, asserted Tasks 2-4.

## Decisions locked

- New runoff path is an additive overload; M241 path removed only in Task 5 (so each task stays green). Final state has ONE runoff accounting path (cerebrum constraint satisfied).
- Ground runoff raises `h` by `surface_added_volume/(phi_t*A)` at zero momentum; infiltration consumes rain budget and does not drain existing ponded `h` (documented M247-C limitation).
- `RunoffStepInputs.lut` carries one neutral soil entry by default so `soil_type 0` resolves; real soils come from PreProc later.
- Roof: `roof_drain_capacity` forced 0 in the ground stage params; roof state fields are read for gather but only ground fields are scattered back.

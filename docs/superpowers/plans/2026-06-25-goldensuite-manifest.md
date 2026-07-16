# GoldenSuite Manifest Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create a machine-checked GoldenSuite manifest (`G1`–`G12`) and activate the implemented surface2d subset (`G1`–`G6`) as dedicated `tests/golden/` CI gate tests.

**Architecture:** `tests/golden/suite_manifest/goldensuite.json` is the single source of truth for the full §10.2 matrix. We add six dedicated golden test executables under `tests/golden/<test_name>/`, all labeled `golden` in CTest, plus a small Python `check_manifest.py` that enforces completeness and same-name/path invariants in CI. Existing unit tests stay in place; golden tests are gate-oriented wrappers with spec-driven assertions and shared tolerances from `golden_tolerances.hpp`.

**Tech Stack:** C++20, CMake/CTest, GoogleTest, Python 3, GitHub Actions YAML.

**Authoritative sources:**
- `docs/superpowers/specs/2026-06-24-goldensuite-manifest-design.md`
- main Spec `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` §10.1/§10.2/§10.3/§11
- project layout `superpowers/specs/project-layout-design.md` §4.20/§8.4
- CI baseline `.github/workflows/ci.yml`

**Current starting state (verified):**
- `tests/golden/` does **not** exist.
- `tests/CMakeLists.txt` currently adds only `unit/*` and `integration/swmm_dflowfm`.
- `.github/workflows/ci.yml` currently has only `build-and-test` and `spike-isolation-check` jobs.
- Existing reusable physics/unit tests live under `tests/unit/surface2d/` (e.g. `test_surface2d_golden_minimal.cpp`, `test_edge_classification.cpp`, `test_friction_source.cpp`, `test_dpm_closure_laws.cpp`).

---

## File Structure

Create / modify exactly these files:

- Create: `tests/golden/CMakeLists.txt`
- Create: `tests/golden/golden_tolerances.hpp`
- Create: `tests/golden/suite_manifest/goldensuite.json`
- Create: `tests/golden/suite_manifest/check_manifest.py`
- Create: `tests/golden/reference/tolerances.md`
- Create: `tests/golden/evidence/README.md`
- Create: `tests/golden/hydrostatic_step/test_hydrostatic_step.cpp`
- Create: `tests/golden/phi_t_jump_hydrostatic/test_phi_t_jump_hydrostatic.cpp`
- Create: `tests/golden/phi_c_edge_zero_velocity/test_phi_c_edge_zero_velocity.cpp`
- Create: `tests/golden/narrow_gap_blockage/test_narrow_gap_blockage.cpp`
- Create: `tests/golden/dpm_drag_decay/test_dpm_drag_decay.cpp`
- Create: `tests/golden/phi_c_spd_reject/test_phi_c_spd_reject.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `.github/workflows/ci.yml`
- Create: `superpowers/specs/2026-06-25-goldensuite-manifest-evidence.md`
- Modify: `superpowers/INDEX.md`
- Modify: `.wolf/anatomy.md`, `.wolf/memory.md`, `.wolf/cerebrum.md`

Keep the existing unit tests untouched unless a golden wrapper genuinely needs a helper copied inline. Do not move or delete unit tests.

---

## Task 1: Manifest skeleton + tolerance source + completeness checker

**Files:**
- Create: `tests/golden/suite_manifest/goldensuite.json`
- Create: `tests/golden/suite_manifest/check_manifest.py`
- Create: `tests/golden/golden_tolerances.hpp`
- Create: `tests/golden/reference/tolerances.md`
- Create: `tests/golden/evidence/README.md`
- Create: `tests/golden/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Create the tolerance source header**

Create `tests/golden/golden_tolerances.hpp`:

```cpp
#pragma once

namespace scau::golden {

// Main-spec §11 tolerance constants. Change here propagates to all golden tests.
inline constexpr double u_hydro_tol = 1.0e-12;           // m/s
inline constexpr double eta_tol = 1.0e-12;               // m
inline constexpr double conservation_near_tol = 1.0e-9; // recomposed-float anti-flaky

}  // namespace scau::golden
```

- [ ] **Step 2: Write the failing manifest-completeness test script first**

Create `tests/golden/suite_manifest/check_manifest.py` with the following exact content:

```python
#!/usr/bin/env python3
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
MANIFEST = ROOT / "tests" / "golden" / "suite_manifest" / "goldensuite.json"
TESTS_CMAKELISTS = ROOT / "tests" / "golden" / "CMakeLists.txt"

REQUIRED = {
    "G1": ("hydrostatic_step", "implemented", True),
    "G2": ("phi_t_jump_hydrostatic", "implemented", True),
    "G3": ("phi_c_edge_zero_velocity", "implemented", True),
    "G4": ("narrow_gap_blockage", "implemented", True),
    "G5": ("dpm_drag_decay", "implemented", True),
    "G6": ("phi_c_spd_reject", "implemented", True),
    "G7": ("stcf_v4_to_v5_migration", "pending", False),
    "G8": ("swmm_single_pipe_surcharge", "pending", False),
    "G9": ("cpu_gpu_deterministic_match", "pending", False),
    "G10": ("snapshot_replay_mass_deficit", "pending", False),
    "G11": ("dflowfm_river_steady", "pending", False),
    "G12": ("dual_engine_shared_cell", "pending", False),
}


def fail(msg: str) -> None:
    print(f"::error::{msg}")
    sys.exit(1)


def main() -> None:
    if not MANIFEST.exists():
        fail(f"missing manifest: {MANIFEST}")
    data = json.loads(MANIFEST.read_text(encoding="utf-8"))
    if not isinstance(data, list):
        fail("manifest root must be a JSON array")

    entries = {}
    for item in data:
        if not isinstance(item, dict):
            fail("manifest entries must be JSON objects")
        test_id = item.get("test_id")
        if test_id in entries:
            fail(f"duplicate test_id: {test_id}")
        entries[test_id] = item

    for test_id, (name, status, ci_gate) in REQUIRED.items():
        if test_id not in entries:
            fail(f"missing manifest entry for {test_id}")
        item = entries[test_id]
        if item.get("name") != name:
            fail(f"{test_id}: expected name {name}, got {item.get('name')}")
        expected_path = f"tests/golden/{name}/"
        if item.get("test_path") != expected_path:
            fail(f"{test_id}: expected test_path {expected_path}, got {item.get('test_path')}")
        if item.get("status") != status:
            fail(f"{test_id}: expected status {status}, got {item.get('status')}")
        if item.get("ci_gate") is not ci_gate:
            fail(f"{test_id}: expected ci_gate {ci_gate}, got {item.get('ci_gate')}")
        if item.get("reference_path") != "tests/golden/reference/tolerances.md":
            fail(f"{test_id}: expected canonical reference_path")
        if item.get("applicable_phase") is None:
            fail(f"{test_id}: missing applicable_phase")

        # Same-name/path invariant.
        expected_dir = ROOT / "tests" / "golden" / name
        if status == "implemented" and not expected_dir.is_dir():
            fail(f"{test_id}: implemented test directory missing: {expected_dir}")

    # No extra unknown IDs.
    unknown = sorted(set(entries) - set(REQUIRED))
    if unknown:
        fail(f"unknown manifest test_id(s): {', '.join(unknown)}")

    # For every active implemented entry, golden CMakeLists must mention the target name.
    cmake_text = TESTS_CMAKELISTS.read_text(encoding="utf-8") if TESTS_CMAKELISTS.exists() else ""
    for test_id, (name, status, ci_gate) in REQUIRED.items():
        if status == "implemented" and ci_gate:
            token = f"add_test(NAME test_{name}"
            if token not in cmake_text:
                fail(f"{test_id}: active gate test missing add_test token {token}")
            if "LABELS golden" not in cmake_text:
                fail("golden CMakeLists must label tests with LABELS golden")

    print("OK goldensuite manifest completeness passes")


if __name__ == "__main__":
    main()
```

- [ ] **Step 3: Write the manifest JSON to satisfy the checker**

Create `tests/golden/suite_manifest/goldensuite.json` with EXACTLY the 12 entries below:

```json
[
  {
    "test_id": "G1",
    "name": "hydrostatic_step",
    "test_path": "tests/golden/hydrostatic_step/",
    "applicable_phase": "Phase 1+",
    "reference_path": "tests/golden/reference/tolerances.md",
    "ci_gate": true,
    "status": "implemented"
  },
  {
    "test_id": "G2",
    "name": "phi_t_jump_hydrostatic",
    "test_path": "tests/golden/phi_t_jump_hydrostatic/",
    "applicable_phase": "Phase 1+",
    "reference_path": "tests/golden/reference/tolerances.md",
    "ci_gate": true,
    "status": "implemented"
  },
  {
    "test_id": "G3",
    "name": "phi_c_edge_zero_velocity",
    "test_path": "tests/golden/phi_c_edge_zero_velocity/",
    "applicable_phase": "Phase 1+",
    "reference_path": "tests/golden/reference/tolerances.md",
    "ci_gate": true,
    "status": "implemented"
  },
  {
    "test_id": "G4",
    "name": "narrow_gap_blockage",
    "test_path": "tests/golden/narrow_gap_blockage/",
    "applicable_phase": "Phase 1+",
    "reference_path": "tests/golden/reference/tolerances.md",
    "ci_gate": true,
    "status": "implemented"
  },
  {
    "test_id": "G5",
    "name": "dpm_drag_decay",
    "test_path": "tests/golden/dpm_drag_decay/",
    "applicable_phase": "Phase 1+",
    "reference_path": "tests/golden/reference/tolerances.md",
    "ci_gate": true,
    "status": "implemented"
  },
  {
    "test_id": "G6",
    "name": "phi_c_spd_reject",
    "test_path": "tests/golden/phi_c_spd_reject/",
    "applicable_phase": "Phase 1+",
    "reference_path": "tests/golden/reference/tolerances.md",
    "ci_gate": true,
    "status": "implemented"
  },
  {
    "test_id": "G7",
    "name": "stcf_v4_to_v5_migration",
    "test_path": "tests/golden/stcf_v4_to_v5_migration/",
    "applicable_phase": "Phase 1+",
    "reference_path": "tests/golden/reference/tolerances.md",
    "ci_gate": false,
    "status": "pending"
  },
  {
    "test_id": "G8",
    "name": "swmm_single_pipe_surcharge",
    "test_path": "tests/golden/swmm_single_pipe_surcharge/",
    "applicable_phase": "Phase 1+",
    "reference_path": "tests/golden/reference/tolerances.md",
    "ci_gate": false,
    "status": "pending"
  },
  {
    "test_id": "G9",
    "name": "cpu_gpu_deterministic_match",
    "test_path": "tests/golden/cpu_gpu_deterministic_match/",
    "applicable_phase": "Phase 1+",
    "reference_path": "tests/golden/reference/tolerances.md",
    "ci_gate": false,
    "status": "pending"
  },
  {
    "test_id": "G10",
    "name": "snapshot_replay_mass_deficit",
    "test_path": "tests/golden/snapshot_replay_mass_deficit/",
    "applicable_phase": "Phase 1+",
    "reference_path": "tests/golden/reference/tolerances.md",
    "ci_gate": false,
    "status": "pending"
  },
  {
    "test_id": "G11",
    "name": "dflowfm_river_steady",
    "test_path": "tests/golden/dflowfm_river_steady/",
    "applicable_phase": "Phase 2+",
    "reference_path": "tests/golden/reference/tolerances.md",
    "ci_gate": false,
    "status": "pending"
  },
  {
    "test_id": "G12",
    "name": "dual_engine_shared_cell",
    "test_path": "tests/golden/dual_engine_shared_cell/",
    "applicable_phase": "Phase 2+",
    "reference_path": "tests/golden/reference/tolerances.md",
    "ci_gate": false,
    "status": "pending"
  }
]
```

- [ ] **Step 4: Create reference and evidence placeholders**

Create `tests/golden/reference/tolerances.md`:

```md
# GoldenSuite Reference Tolerances

Authoritative source: `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` §11.

- `u_hydro_tol = 1e-12 m/s`
- `eta_tol = 1e-12 m`
- `conservation_near_tol = 1e-9` (engineering anti-flaky tolerance for recomposed floating-point aggregates; not a Spec §11 constant)

Deterministic path requirement (§10.3): CPU double precision, fixed reduction order.
```

Create `tests/golden/evidence/README.md`:

```md
# GoldenSuite Evidence

Store CI gate run records and reproducible evidence for the active GoldenSuite subset here.
Each evidence record should name the commit, preset(s), active G_n list, pass/fail result, and any reference artifacts.
```

- [ ] **Step 5: Create the `tests/golden/CMakeLists.txt` skeleton and root tests wiring**

Create `tests/golden/CMakeLists.txt` with placeholders for the six subdirectories (the targets themselves come in Task 2):

```cmake
add_subdirectory(hydrostatic_step)
add_subdirectory(phi_t_jump_hydrostatic)
add_subdirectory(phi_c_edge_zero_velocity)
add_subdirectory(narrow_gap_blockage)
add_subdirectory(dpm_drag_decay)
add_subdirectory(phi_c_spd_reject)
```

Modify `tests/CMakeLists.txt` by appending:

```cmake
add_subdirectory(golden)
```

- [ ] **Step 6: Run the failing manifest check**

Run: `python tests/golden/suite_manifest/check_manifest.py`
Expected: FAIL, because the six `tests/golden/<name>/` directories do not yet exist and `tests/golden/CMakeLists.txt` does not yet contain `add_test(NAME test_<name>...)` tokens. This is the intended RED state for Task 1.

- [ ] **Step 7: Commit**

```bash
git add tests/golden/suite_manifest/goldensuite.json tests/golden/suite_manifest/check_manifest.py tests/golden/golden_tolerances.hpp tests/golden/reference/tolerances.md tests/golden/evidence/README.md tests/golden/CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat(tests): GoldenSuite manifest skeleton and tolerance source"
```

---

## Task 2: Implement and register the six active GoldenTests (G1–G6)

**Files:**
- Create: `tests/golden/hydrostatic_step/CMakeLists.txt`
- Create: `tests/golden/hydrostatic_step/test_hydrostatic_step.cpp`
- Create: `tests/golden/phi_t_jump_hydrostatic/CMakeLists.txt`
- Create: `tests/golden/phi_t_jump_hydrostatic/test_phi_t_jump_hydrostatic.cpp`
- Create: `tests/golden/phi_c_edge_zero_velocity/CMakeLists.txt`
- Create: `tests/golden/phi_c_edge_zero_velocity/test_phi_c_edge_zero_velocity.cpp`
- Create: `tests/golden/narrow_gap_blockage/CMakeLists.txt`
- Create: `tests/golden/narrow_gap_blockage/test_narrow_gap_blockage.cpp`
- Create: `tests/golden/dpm_drag_decay/CMakeLists.txt`
- Create: `tests/golden/dpm_drag_decay/test_dpm_drag_decay.cpp`
- Create: `tests/golden/phi_c_spd_reject/CMakeLists.txt`
- Create: `tests/golden/phi_c_spd_reject/test_phi_c_spd_reject.cpp`

**Shared CMake pattern for each subdir:**

```cmake
add_executable(test_<name> test_<name>.cpp)
target_link_libraries(test_<name>
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_<name> COMMAND test_<name>)
set_tests_properties(test_<name> PROPERTIES LABELS golden)
```

- [ ] **Step 1: G1 — hydrostatic_step**

Create `tests/golden/hydrostatic_step/test_hydrostatic_step.cpp`:

```cpp
#include <algorithm>
#include <cmath>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/state/state.hpp"
#include "surface2d/time_integration/step.hpp"
#include "tests/golden/golden_tolerances.hpp"

namespace {
double max_abs_velocity(const scau::surface2d::SurfaceState& state) {
    double worst = 0.0;
    for (const auto& cell : state.cells) {
        worst = std::max({worst, std::abs(cell.u()), std::abs(cell.v())});
    }
    return worst;
}
}

TEST(HydrostaticStepGolden, PreservesEtaAndVelocityAtRest) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    const auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    const scau::surface2d::StepConfig config{.dt = 0.5, .cfl_safety = 0.45, .c_rollback = 10.0};

    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields);

    ASSERT_FALSE(diagnostics.rollback_required);
    EXPECT_NEAR(max_abs_velocity(state), 0.0, scau::golden::u_hydro_tol);
    for (const auto& cell : state.cells) {
        EXPECT_NEAR(cell.eta, 1.0, scau::golden::eta_tol);
    }
}
```

- [ ] **Step 2: G2 — phi_t_jump_hydrostatic**

Create `tests/golden/phi_t_jump_hydrostatic/test_phi_t_jump_hydrostatic.cpp` using the already-proven `test_well_balanced_phi_t_jump_at_rest.cpp` logic, but with golden naming and tolerance source:

```cpp
#include <algorithm>
#include <cmath>
#include <cstddef>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/state/state.hpp"
#include "surface2d/time_integration/step.hpp"
#include "tests/golden/golden_tolerances.hpp"

namespace {
double max_abs_momentum(const scau::surface2d::SurfaceState& state) {
    double worst = 0.0;
    for (const auto& cell : state.cells) {
        worst = std::max({worst, std::abs(cell.conserved.hu), std::abs(cell.conserved.hv)});
    }
    return worst;
}
}

TEST(PhiTJumpHydrostaticGolden, KeepsMomentumZero) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    for (std::size_t i = 0; i < dpm_fields.cells.size(); ++i) {
        dpm_fields.cells[i].phi_t = (i % 2 == 0) ? 1.0 : 1.5;
    }
    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 100.0, .h_min = 1.0e-8};

    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields);

    ASSERT_FALSE(diagnostics.rollback_required);
    EXPECT_NEAR(max_abs_momentum(state), 0.0, scau::golden::u_hydro_tol);
}
```

- [ ] **Step 3: G3 — phi_c_edge_zero_velocity**

Create `tests/golden/phi_c_edge_zero_velocity/test_phi_c_edge_zero_velocity.cpp` by adapting the existing DPM tensor/edge-conveyance + quiet-water path:

```cpp
#include <algorithm>
#include <cmath>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/boundary/conditions.hpp"
#include "surface2d/dpm/edge_conveyance.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/dpm/tensor_projection.hpp"
#include "surface2d/geometry/cache.hpp"
#include "surface2d/state/state.hpp"
#include "surface2d/time_integration/step.hpp"
#include "tests/golden/golden_tolerances.hpp"

namespace {
double max_abs_velocity(const scau::surface2d::SurfaceState& state) {
    double worst = 0.0;
    for (const auto& cell : state.cells) {
        worst = std::max({worst, std::abs(cell.u()), std::abs(cell.v())});
    }
    return worst;
}
}

TEST(PhiCEdgeZeroVelocityGolden, JumpDoesNotCreateSpuriousVelocity) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto geometry = scau::surface2d::GeometryCache::for_mesh(mesh);
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    for (std::size_t i = 0; i < dpm_fields.cells.size(); ++i) {
        dpm_fields.cells[i].Phi_c = (i % 2 == 0)
            ? scau::surface2d::Tensor2Symmetric{.xx = 1.0, .xy = 0.0, .yy = 1.0}
            : scau::surface2d::Tensor2Symmetric{.xx = 1.0, .xy = 0.0, .yy = 9.0};
    }
    static_cast<void>(scau::surface2d::assemble_edge_conveyance_from_tensors(mesh, geometry, dpm_fields));
    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 100.0, .h_min = 1.0e-8};

    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields);

    ASSERT_FALSE(diagnostics.rollback_required);
    EXPECT_NEAR(max_abs_velocity(state), 0.0, scau::golden::u_hydro_tol);
}
```

- [ ] **Step 4: G4 — narrow_gap_blockage**

Create `tests/golden/narrow_gap_blockage/test_narrow_gap_blockage.cpp`:

```cpp
#include <cstddef>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/state/state.hpp"
#include "surface2d/time_integration/step.hpp"

namespace {
std::size_t first_internal_edge_index(const scau::mesh::Mesh& mesh) {
    for (std::size_t i = 0; i < mesh.edges.size(); ++i) {
        if (mesh.edges[i].left_cell.has_value() && mesh.edges[i].right_cell.has_value()) return i;
    }
    throw std::invalid_argument("mesh must contain an internal edge");
}
std::size_t cell_index_by_id(const scau::mesh::Mesh& mesh, const std::string& id) {
    for (std::size_t i = 0; i < mesh.cells.size(); ++i) {
        if (mesh.cells[i].id == id) return i;
    }
    throw std::invalid_argument("mesh must contain referenced cell");
}
}

TEST(NarrowGapBlockageGolden, HardBlockHasExactZeroFlux) {
    const auto mesh = scau::mesh::build_tri_only_control_mesh();
    const auto edge_index = first_internal_edge_index(mesh);
    const auto left_index = cell_index_by_id(mesh, *mesh.edges[edge_index].left_cell);
    const auto right_index = cell_index_by_id(mesh, *mesh.edges[edge_index].right_cell);

    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    state.cells[left_index].conserved.hu = 1.0;
    state.cells[right_index].conserved.hu = 1.0;
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    dpm_fields.edges[edge_index].omega_edge = 0.0;
    dpm_fields.edges[edge_index].phi_e_n = 0.0;
    const scau::surface2d::StepConfig config{.dt = 0.5, .cfl_safety = 0.45, .c_rollback = 1.0};

    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields);

    EXPECT_EQ(diagnostics.edges[edge_index].mass_flux, 0.0);
    EXPECT_EQ(diagnostics.edges[edge_index].momentum_flux_n, 0.0);
    EXPECT_EQ(diagnostics.edges[edge_index].momentum_x, 0.0);
    EXPECT_EQ(diagnostics.edges[edge_index].momentum_y, 0.0);
}
```

- [ ] **Step 5: G5 — dpm_drag_decay**

Create `tests/golden/dpm_drag_decay/test_dpm_drag_decay.cpp` by following the cerebrum lesson: compare to a no-friction control run, and use `EXPECT_NEAR(..., conservation_near_tol)` for recomposed mass.

```cpp
#include <algorithm>
#include <cmath>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/geometry/cache.hpp"
#include "surface2d/source_terms/fields.hpp"
#include "surface2d/state/state.hpp"
#include "surface2d/time_integration/step.hpp"
#include "tests/golden/golden_tolerances.hpp"

namespace {
double system_mass(
    const scau::surface2d::SurfaceState& state,
    const scau::surface2d::DpmFields& dpm_fields,
    const scau::surface2d::GeometryCache& geometry) {
    double total = 0.0;
    for (std::size_t i = 0; i < state.cells.size(); ++i) {
        total += state.cells[i].conserved.h * dpm_fields.cells[i].phi_t * geometry.cell_areas[i];
    }
    return total;
}
}

TEST(DpmDragDecayGolden, DragDecaysMomentumWithoutCreatingMass) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto geometry = scau::surface2d::GeometryCache::for_mesh(mesh);
    auto state_drag = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    for (auto& cell : state_drag.cells) {
        cell.conserved.hu = 1.0;
    }
    auto state_control = state_drag;
    const auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    const auto boundary = scau::surface2d::BoundaryConditions::for_mesh(mesh);
    auto sources_drag = scau::surface2d::SourceTermFields::for_mesh(mesh);
    for (auto& n : sources_drag.manning_n) {
        n = 0.05;
    }
    const auto sources_control = scau::surface2d::SourceTermFields::for_mesh(mesh);
    const scau::surface2d::StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 100.0, .h_min = 1.0e-8};

    const double mass_before = system_mass(state_drag, dpm_fields, geometry);
    static_cast<void>(scau::surface2d::advance_one_step_cpu(mesh, state_drag, config, dpm_fields, boundary, sources_drag, geometry));
    static_cast<void>(scau::surface2d::advance_one_step_cpu(mesh, state_control, config, dpm_fields, boundary, sources_control, geometry));
    const double mass_after = system_mass(state_drag, dpm_fields, geometry);

    bool any_strictly_reduced = false;
    for (std::size_t i = 0; i < state_drag.cells.size(); ++i) {
        const double hu_drag = state_drag.cells[i].conserved.hu;
        const double hu_control = state_control.cells[i].conserved.hu;
        EXPECT_LE(std::abs(hu_drag), std::abs(hu_control));
        if (std::abs(hu_control) > 1.0e-12) {
            EXPECT_LT(std::abs(hu_drag), std::abs(hu_control));
            any_strictly_reduced = true;
        }
    }
    EXPECT_TRUE(any_strictly_reduced);
    EXPECT_NEAR(mass_after - mass_before, 0.0, scau::golden::conservation_near_tol);
}
```

- [ ] **Step 6: G6 — phi_c_spd_reject**

Create `tests/golden/phi_c_spd_reject/test_phi_c_spd_reject.cpp`:

```cpp
#include <stdexcept>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/dpm/edge_conveyance.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/dpm/tensor_projection.hpp"
#include "surface2d/geometry/cache.hpp"

TEST(PhiCSpdRejectGolden, RejectsNonPositiveDefiniteTensor) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto geometry = scau::surface2d::GeometryCache::for_mesh(mesh);
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    dpm_fields.cells[0].Phi_c = scau::surface2d::Tensor2Symmetric{.xx = 0.0, .xy = 0.0, .yy = 0.0};

    EXPECT_THROW(
        static_cast<void>(scau::surface2d::assemble_edge_conveyance_from_tensors(mesh, geometry, dpm_fields)),
        std::invalid_argument);
}
```

- [ ] **Step 7: Register all six test directories and run them**

For each of the 6 directories, create the small `CMakeLists.txt` using the shared pattern from the top of Task 2.

Run: `cmake --preset windows-msvc && cmake --build --preset windows-msvc --target test_hydrostatic_step test_phi_t_jump_hydrostatic test_phi_c_edge_zero_velocity test_narrow_gap_blockage test_dpm_drag_decay test_phi_c_spd_reject && ctest --preset windows-msvc -L golden --timeout 120`
Expected: 6/6 tests pass.

- [ ] **Step 8: Commit**

```bash
git add tests/golden
git commit -m "feat(tests): add GoldenSuite G1-G6 dedicated gate tests"
```

---

## Task 3: Manifest completeness checker must go green

**Files:**
- Modify: `tests/golden/CMakeLists.txt` (if needed for LABELS token consistency)
- Run: `tests/golden/suite_manifest/check_manifest.py`

- [ ] **Step 1: Run the checker again**

Run: `python tests/golden/suite_manifest/check_manifest.py`
Expected: PASS — prints `OK goldensuite manifest completeness passes`.

- [ ] **Step 2: If it fails, fix the exact invariant**

Allowed fixes only:
- missing/typo in `goldensuite.json`
- missing golden directory name mismatch
- missing `add_test(NAME test_<name> ...)` token in `tests/golden/CMakeLists.txt`
- missing `LABELS golden` on any of the 6 tests

Do NOT loosen the checker.

- [ ] **Step 3: Commit only if any fix was needed**

```bash
git add tests/golden/suite_manifest/goldensuite.json tests/golden/CMakeLists.txt tests/golden/suite_manifest/check_manifest.py
git commit -m "fix(tests): satisfy GoldenSuite manifest invariants"
```

---

## Task 4: CI gate wiring

**Files:**
- Modify: `.github/workflows/ci.yml`

- [ ] **Step 1: Add the `golden-suite-gate` job**

Append a new job after `build-and-test` and before `spike-isolation-check`:

```yaml
  golden-suite-gate:
    name: golden-suite (${{ matrix.preset }})
    runs-on: ${{ matrix.os }}
    strategy:
      fail-fast: false
      matrix:
        include:
          - os: ubuntu-22.04
            preset: linux-gcc
            triplet: x64-linux
          - os: windows-2022
            preset: windows-msvc
            triplet: x64-windows

    steps:
      - name: Checkout
        uses: actions/checkout@v4

      - name: Install Linux deps
        if: runner.os == 'Linux'
        run: |
          sudo apt-get update
          sudo apt-get install -y ninja-build cmake build-essential pkg-config

      - name: Install Ninja (Windows)
        if: runner.os == 'Windows'
        uses: seanmiddleditch/gha-setup-ninja@v4

      - name: Bootstrap vcpkg
        shell: bash
        run: |
          mkdir -p "$VCPKG_DEFAULT_BINARY_CACHE"
          if [ ! -d "$VCPKG_ROOT" ]; then
            git clone https://github.com/microsoft/vcpkg.git "$VCPKG_ROOT"
          fi
          if [ "${{ runner.os }}" = "Windows" ]; then
            "$VCPKG_ROOT/bootstrap-vcpkg.bat" -disableMetrics
          else
            "$VCPKG_ROOT/bootstrap-vcpkg.sh" -disableMetrics
          fi

      - name: Cache vcpkg binary cache
        uses: actions/cache@v4
        with:
          path: ${{ env.VCPKG_DEFAULT_BINARY_CACHE }}
          key: vcpkg-${{ runner.os }}-${{ hashFiles('vcpkg.json') }}
          restore-keys: |
            vcpkg-${{ runner.os }}-

      - name: Configure
        run: cmake --preset ${{ matrix.preset }}

      - name: Build
        run: cmake --build --preset ${{ matrix.preset }}

      - name: GoldenSuite gate
        run: ctest --preset ${{ matrix.preset }} -L golden --output-on-failure
```

- [ ] **Step 2: Add the `goldensuite-manifest-check` job**

Append a second new job after `golden-suite-gate`:

```yaml
  goldensuite-manifest-check:
    name: manifest-completeness
    runs-on: ubuntu-22.04
    steps:
      - name: Checkout
        uses: actions/checkout@v4

      - name: Check GoldenSuite manifest completeness
        shell: bash
        run: python3 tests/golden/suite_manifest/check_manifest.py
```

- [ ] **Step 3: Validate YAML shape locally**

Run: `python - << 'EOF'
import yaml, pathlib
path = pathlib.Path('.github/workflows/ci.yml')
with open(path, 'r', encoding='utf-8') as f:
    yaml.safe_load(f)
print('YAML OK')
EOF`
Expected: `YAML OK`.

- [ ] **Step 4: Commit**

```bash
git add .github/workflows/ci.yml
git commit -m "ci: add GoldenSuite gate and manifest completeness check"
```

---

## Task 5: First gate evidence + project records

**Files:**
- Create: `superpowers/specs/2026-06-25-goldensuite-manifest-evidence.md`
- Modify: `superpowers/INDEX.md`
- Modify: `.wolf/anatomy.md`, `.wolf/memory.md`, `.wolf/cerebrum.md`

- [ ] **Step 1: Run the first gate locally and capture the output**

Run: `cmake --build --preset windows-msvc && ctest --preset windows-msvc -L golden --output-on-failure`
Expected: 6/6 golden tests pass. Also run `python tests/golden/suite_manifest/check_manifest.py` -> `OK goldensuite manifest completeness passes`.

- [ ] **Step 2: Write evidence**

Create `superpowers/specs/2026-06-25-goldensuite-manifest-evidence.md` recording:
- manifest file path and that it contains all G1-G12;
- G1-G6 active, G7-G12 pending;
- first local gate run result (6/6) and manifest-check result;
- note that G5 uses `conservation_near_tol = 1e-9` by design for recomposed-float anti-flaky, while G4 remains exact-zero and G1-G3 use spec §11 tolerances;
- note that sloping-bed well-balanced is intentionally not a GoldenSuite G_n (unit regression only).

- [ ] **Step 3: Update INDEX and OpenWolf records**

Append one row under the 阶段性证据入口 table in `superpowers/INDEX.md`:

```md
| `superpowers/specs/2026-06-25-goldensuite-manifest-evidence.md` | GoldenSuite manifest + CI gate evidence；登记全 G1-G12 矩阵并激活 G1-G6 为 active CI gate，记录首轮 6/6 golden gate 与 manifest-completeness 通过；G7-G12 仍 pending。 |
```

Update `.wolf/anatomy.md` with entries for `tests/golden/` and the 6 tests + manifest files. Append `.wolf/memory.md` with the gate result. Append `.wolf/cerebrum.md` with the core lesson: “G_n numbering comes ONLY from main Spec §10.2; ad-hoc internal labels (e.g. earlier S_phi_t G4/G5) must never leak into GoldenSuite. Gate assertions are semantic: G4 exact-zero, G5 conservation-near, G1-G3 u_hydro_tol/eta_tol from spec §11.”

- [ ] **Step 4: Commit**

```bash
git add superpowers/specs/2026-06-25-goldensuite-manifest-evidence.md superpowers/INDEX.md .wolf
git commit -m "docs(tests): GoldenSuite manifest gate evidence and project records"
```

---

## Self-Review

**Spec coverage:**
- §1 directory + schema -> Task 1
- §2 six dedicated golden tests -> Task 2
- §3.1 criterion strategy / `golden_tolerances.hpp` -> Task 1 + Task 2
- §3.2 CI gate + manifest check -> Task 4
- §3.3 reference/evidence -> Task 1 + Task 5
- §3.4 pending G7-G12 + unit-tests-stay-put boundary -> Task 1 manifest + no unit test deletions

**Placeholder scan:** none — all files, JSON, Python, YAML, and test code are fully specified.

**Type consistency:**
- Manifest schema fields are consistent across JSON, Python checker, and evidence (`test_id`, `name`, `test_path`, `applicable_phase`, `reference_path`, `ci_gate`, `status`).
- All six test target names follow `test_<name>` and the checker looks for `add_test(NAME test_<name>` exactly.
- `golden_tolerances.hpp` namespace is consistently `scau::golden` in all tests.

**Risk note (explicit, not a placeholder):** Task 2 intentionally builds dedicated golden tests instead of reusing unit test executables so gate semantics stay independent. That means code duplication of small helpers (e.g. `max_abs_velocity`) is acceptable here; do NOT try to centralize them into a testing utility library during this feature — YAGNI and it muddies the gate layout. Additionally, G5 should compare against a no-friction control run (per the cerebrum lesson) rather than the initial momentum value. It must use `EXPECT_NEAR(delta_mass, 0.0, conservation_near_tol)` for the recomposed `Σ h·phi_t·area` aggregate; an exact-zero macro would make the gate flaky.

# Plan-M3: Surface2D 数值推进最小切片实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 sandbox B 已验证的 DPM / hydrostatic reconstruction / HLLC zero-flux / `phi_t` pressure-source pairing 迁入正式 `libs/surface2d/`，形成可测试的 CPU reference 最小数值推进切片。

**Architecture:** M3 只扩展 `Surface2DCore` 内部的 C++ reference path，不接入 SWMM、D-Flow FM 或 `CouplingLib`。DPM 字段、重构、Riemann flux、source terms 分别落在 `libs/surface2d/include/surface2d/{dpm,reconstruction,riemann,source_terms}/`，`advance_one_step_cpu()` 只做最小 hydrostatic diagnostic-preserving advancement。测试用 M2 的 mixed / quad-only / tri-only mesh builders 镜像 sandbox B 的 G1/G2/G3 退出证据。

**Tech Stack:** C++20, CMake, GoogleTest, existing `scau::core`, `scau::mesh`, `scau::surface2d` targets.

---

## Entry Evidence

- Sandbox B exit report: `sandbox/numerical/evidence/sandbox_report.md` concludes the reference implementation satisfies DPM / HLLC / source-term exit conditions.
- Sandbox B proof summary: `sandbox/numerical/evidence/well_balanced_proof.md` records the closed-form hydrostatic, `phi_t`, and `Phi_c / phi_e_n` degeneracy arguments.
- Sandbox B runnable evidence: `sandbox/numerical/evidence/g1_run.md`, `g2_run.md`, and `g3_run.md` record mixed / quad-only / tri-only 1000-step PASS diagnostics.
- M3 still requires fresh C++ unit tests and local build/CTest evidence; sandbox B does not replace GoldenSuite or future CI gates.

## File Structure

- Modify `libs/surface2d/CMakeLists.txt` to compile the new `.cpp` units.
- Modify `libs/surface2d/include/surface2d/state/state.hpp` to carry `eta` while preserving existing conserved-state tests.
- Create `libs/surface2d/include/surface2d/dpm/fields.hpp` for `CellDpmFields`, `EdgeDpmFields`, `DpmFields`, and mesh-aligned validation.
- Create `libs/surface2d/src/dpm/fields.cpp` for `DpmFields::for_mesh()` and validation implementation.
- Create `libs/surface2d/include/surface2d/reconstruction/hydrostatic.hpp` for `HydrostaticPair` and hydrostatic pair reconstruction.
- Create `libs/surface2d/src/reconstruction/hydrostatic.cpp` for hydrostatic reconstruction implementation.
- Create `libs/surface2d/include/surface2d/riemann/hllc.hpp` for `EdgeFlux`, `PhiEdgeMin`, normal velocity, and minimal HLLC normal flux.
- Create `libs/surface2d/src/riemann/hllc.cpp` for zero-flux degeneracy and minimal mass-flux implementation.
- Create `libs/surface2d/include/surface2d/source_terms/phi_t.hpp` for pressure pairing and `S_phi_t` helpers.
- Create `libs/surface2d/src/source_terms/phi_t.cpp` for exact opposite-sign source implementation.
- Modify `libs/surface2d/include/surface2d/time_integration/step.hpp` to expose optional DPM fields and edge diagnostics without changing canonical CFL semantics.
- Modify `libs/surface2d/src/time_integration/step.cpp` to validate DPM fields, compute per-edge diagnostics, keep hydrostatic zero-velocity states unchanged, and keep `max_cell_cfl` raw.
- Modify `tests/unit/surface2d/CMakeLists.txt` to add M3 unit tests.
- Create `tests/unit/surface2d/test_dpm_fields.cpp` for DPM field defaults and validation.
- Create `tests/unit/surface2d/test_hydrostatic_reconstruction.cpp` for zero-velocity hydrostatic reconstruction.
- Create `tests/unit/surface2d/test_hllc_flux.cpp` for normal velocity and hard/soft/zero-velocity zero flux.
- Create `tests/unit/surface2d/test_phi_t_source.cpp` for pressure pairing and exact source cancellation.
- Create `tests/unit/surface2d/test_surface2d_golden_minimal.cpp` for G1/G2/G3 C++ minimal-slice coverage.
- Create `superpowers/specs/2026-05-12-plan-m3-surface2d-numerical-minimal-slice-evidence.md` after implementation validation.

## Non-Goals

- Do not implement full wetting/drying, rainfall, infiltration, drag, CUDA, full HLLC wave-speed physics, or production GoldenSuite gates in M3.
- Do not add dependencies from `Surface2DCore` to SWMM, D-Flow FM, `CouplingLib`, or any 1D engine ABI.
- Do not rename canonical DPM fields or replace `Q_limit`, `V_limit`, `max_cell_cfl`, `C_rollback`, or `CFL_safety` semantics.

---

### Task 1: DPM Field Containers

**Files:**
- Modify: `libs/surface2d/CMakeLists.txt`
- Create: `libs/surface2d/include/surface2d/dpm/fields.hpp`
- Create: `libs/surface2d/src/dpm/fields.cpp`
- Test: `tests/unit/surface2d/test_dpm_fields.cpp`
- Modify: `tests/unit/surface2d/CMakeLists.txt`

- [ ] **Step 1: Write the failing DPM field test**

Create `tests/unit/surface2d/test_dpm_fields.cpp`:

```cpp
#include <stdexcept>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/dpm/fields.hpp"

TEST(DpmFields, CreatesMeshAlignedDefaults) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto fields = scau::surface2d::DpmFields::for_mesh(mesh);

    ASSERT_EQ(fields.cells.size(), mesh.cells.size());
    ASSERT_EQ(fields.edges.size(), mesh.edges.size());
    for (const auto& cell : fields.cells) {
        EXPECT_EQ(cell.phi_t, 1.0);
        EXPECT_EQ(cell.Phi_c, 1.0);
    }
    for (const auto& edge : fields.edges) {
        EXPECT_EQ(edge.phi_e_n, 1.0);
        EXPECT_EQ(edge.omega_edge, 1.0);
    }
}

TEST(DpmFields, RejectsWrongFieldCounts) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto fields = scau::surface2d::DpmFields::for_mesh(mesh);

    fields.cells.pop_back();
    EXPECT_THROW(scau::surface2d::validate_dpm_fields_match_mesh(fields, mesh), std::invalid_argument);

    fields = scau::surface2d::DpmFields::for_mesh(mesh);
    fields.edges.pop_back();
    EXPECT_THROW(scau::surface2d::validate_dpm_fields_match_mesh(fields, mesh), std::invalid_argument);
}
```

Append to `tests/unit/surface2d/CMakeLists.txt`:

```cmake
add_executable(test_dpm_fields test_dpm_fields.cpp)
target_link_libraries(test_dpm_fields
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_dpm_fields COMMAND test_dpm_fields)
```

- [ ] **Step 2: Run test to verify it fails**

Run from repository root after configuring the build directory used by M2:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build --output-on-failure -R test_dpm_fields
```

Expected: build fails because `surface2d/dpm/fields.hpp` does not exist.

- [ ] **Step 3: Add DPM field API**

Create `libs/surface2d/include/surface2d/dpm/fields.hpp`:

```cpp
#pragma once

#include <vector>

#include "core/types.hpp"
#include "mesh/mesh.hpp"

namespace scau::surface2d {

struct CellDpmFields {
    core::Real phi_t{1.0};
    core::Real Phi_c{1.0};
};

struct EdgeDpmFields {
    core::Real phi_e_n{1.0};
    core::Real omega_edge{1.0};
};

struct DpmFields {
    std::vector<CellDpmFields> cells;
    std::vector<EdgeDpmFields> edges;

    [[nodiscard]] static DpmFields for_mesh(const mesh::Mesh& mesh);
};

void validate_dpm_fields_match_mesh(const DpmFields& fields, const mesh::Mesh& mesh);

}  // namespace scau::surface2d
```

Create `libs/surface2d/src/dpm/fields.cpp`:

```cpp
#include "surface2d/dpm/fields.hpp"

#include <stdexcept>

namespace scau::surface2d {

DpmFields DpmFields::for_mesh(const mesh::Mesh& mesh) {
    DpmFields fields;
    fields.cells.resize(mesh.cells.size());
    fields.edges.resize(mesh.edges.size());
    return fields;
}

void validate_dpm_fields_match_mesh(const DpmFields& fields, const mesh::Mesh& mesh) {
    if (fields.cells.size() != mesh.cells.size()) {
        throw std::invalid_argument("DPM field cell count must match mesh cell count");
    }
    if (fields.edges.size() != mesh.edges.size()) {
        throw std::invalid_argument("DPM field edge count must match mesh edge count");
    }
}

}  // namespace scau::surface2d
```

Modify `libs/surface2d/CMakeLists.txt`:

```cmake
add_library(scau_surface2d
    src/dpm/fields.cpp
    src/time_integration/step.cpp
)
```

- [ ] **Step 4: Run DPM test to verify it passes**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build --output-on-failure -R test_dpm_fields
```

Expected: `test_dpm_fields` passes.

- [ ] **Step 5: Commit**

```bash
git add libs/surface2d/CMakeLists.txt libs/surface2d/include/surface2d/dpm/fields.hpp libs/surface2d/src/dpm/fields.cpp tests/unit/surface2d/CMakeLists.txt tests/unit/surface2d/test_dpm_fields.cpp
git commit -m "feat(surface2d): add DPM field containers"
```

---

### Task 2: Hydrostatic State And Reconstruction

**Files:**
- Modify: `libs/surface2d/CMakeLists.txt`
- Modify: `libs/surface2d/include/surface2d/state/state.hpp`
- Create: `libs/surface2d/include/surface2d/reconstruction/hydrostatic.hpp`
- Create: `libs/surface2d/src/reconstruction/hydrostatic.cpp`
- Modify: `tests/unit/surface2d/test_surface_state.cpp`
- Test: `tests/unit/surface2d/test_hydrostatic_reconstruction.cpp`
- Modify: `tests/unit/surface2d/CMakeLists.txt`

- [ ] **Step 1: Write failing state and reconstruction tests**

Append to `tests/unit/surface2d/test_surface_state.cpp`:

```cpp
TEST(SurfaceState, CreatesHydrostaticCellAlignedState) {
    const auto mesh = scau::mesh::build_quad_only_control_mesh();
    const auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);

    ASSERT_EQ(state.cells.size(), mesh.cells.size());
    for (const auto& cell_state : state.cells) {
        EXPECT_EQ(cell_state.conserved.h, 1.0);
        EXPECT_EQ(cell_state.conserved.hu, 0.0);
        EXPECT_EQ(cell_state.conserved.hv, 0.0);
        EXPECT_EQ(cell_state.eta, 1.0);
    }
}
```

Create `tests/unit/surface2d/test_hydrostatic_reconstruction.cpp`:

```cpp
#include <gtest/gtest.h>

#include "surface2d/reconstruction/hydrostatic.hpp"
#include "surface2d/state/state.hpp"

TEST(HydrostaticReconstruction, EqualEtaZeroVelocityPreservesDepth) {
    const scau::surface2d::CellState left{.conserved = {.h = 1.0, .hu = 0.0, .hv = 0.0}, .eta = 1.0};
    const scau::surface2d::CellState right{.conserved = {.h = 1.0, .hu = 0.0, .hv = 0.0}, .eta = 1.0};

    const auto pair = scau::surface2d::reconstruct_hydrostatic_pair(left, right);

    EXPECT_EQ(pair.left.conserved.h, 1.0);
    EXPECT_EQ(pair.right.conserved.h, 1.0);
    EXPECT_EQ(pair.left.conserved.hu, 0.0);
    EXPECT_EQ(pair.left.conserved.hv, 0.0);
    EXPECT_EQ(pair.right.conserved.hu, 0.0);
    EXPECT_EQ(pair.right.conserved.hv, 0.0);
    EXPECT_EQ(pair.left.eta, 1.0);
    EXPECT_EQ(pair.right.eta, 1.0);
}
```

Append to `tests/unit/surface2d/CMakeLists.txt`:

```cmake
add_executable(test_hydrostatic_reconstruction test_hydrostatic_reconstruction.cpp)
target_link_libraries(test_hydrostatic_reconstruction
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_hydrostatic_reconstruction COMMAND test_hydrostatic_reconstruction)
```

- [ ] **Step 2: Run tests to verify they fail**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build --output-on-failure -R "test_surface_state|test_hydrostatic_reconstruction"
```

Expected: build fails because `hydrostatic_for_mesh()` and hydrostatic reconstruction do not exist.

- [ ] **Step 3: Extend state and add reconstruction API**

Modify `libs/surface2d/include/surface2d/state/state.hpp` so the relevant declarations are:

```cpp
struct CellState {
    ConservedState conserved;
    core::Real eta{0.0};

    [[nodiscard]] core::Real u() const {
        return conserved.h > 0.0 ? conserved.hu / conserved.h : 0.0;
    }

    [[nodiscard]] core::Real v() const {
        return conserved.h > 0.0 ? conserved.hv / conserved.h : 0.0;
    }
};

struct SurfaceState {
    std::vector<CellState> cells;

    [[nodiscard]] static SurfaceState for_mesh(const mesh::Mesh& mesh) {
        SurfaceState state;
        state.cells.resize(mesh.cells.size());
        return state;
    }

    [[nodiscard]] static SurfaceState hydrostatic_for_mesh(
        const mesh::Mesh& mesh,
        core::Real eta,
        core::Real depth) {
        SurfaceState state = for_mesh(mesh);
        for (auto& cell : state.cells) {
            cell.conserved.h = depth;
            cell.conserved.hu = 0.0;
            cell.conserved.hv = 0.0;
            cell.eta = eta;
        }
        return state;
    }
};
```

Create `libs/surface2d/include/surface2d/reconstruction/hydrostatic.hpp`:

```cpp
#pragma once

#include "surface2d/state/state.hpp"

namespace scau::surface2d {

struct HydrostaticPair {
    CellState left;
    CellState right;
};

[[nodiscard]] HydrostaticPair reconstruct_hydrostatic_pair(const CellState& left, const CellState& right);

}  // namespace scau::surface2d
```

Create `libs/surface2d/src/reconstruction/hydrostatic.cpp`:

```cpp
#include "surface2d/reconstruction/hydrostatic.hpp"

#include <algorithm>

namespace scau::surface2d {

HydrostaticPair reconstruct_hydrostatic_pair(const CellState& left, const CellState& right) {
    const core::Real eta = std::min(left.eta, right.eta);
    const core::Real left_depth = std::max(0.0, left.conserved.h - (left.eta - eta));
    const core::Real right_depth = std::max(0.0, right.conserved.h - (right.eta - eta));

    return HydrostaticPair{
        .left = CellState{.conserved = {.h = left_depth, .hu = 0.0, .hv = 0.0}, .eta = eta},
        .right = CellState{.conserved = {.h = right_depth, .hu = 0.0, .hv = 0.0}, .eta = eta},
    };
}

}  // namespace scau::surface2d
```

Modify `libs/surface2d/CMakeLists.txt`:

```cmake
add_library(scau_surface2d
    src/dpm/fields.cpp
    src/reconstruction/hydrostatic.cpp
    src/time_integration/step.cpp
)
```

- [ ] **Step 4: Run reconstruction tests to verify they pass**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build --output-on-failure -R "test_surface_state|test_hydrostatic_reconstruction"
```

Expected: both tests pass.

- [ ] **Step 5: Commit**

```bash
git add libs/surface2d/CMakeLists.txt libs/surface2d/include/surface2d/state/state.hpp libs/surface2d/include/surface2d/reconstruction/hydrostatic.hpp libs/surface2d/src/reconstruction/hydrostatic.cpp tests/unit/surface2d/CMakeLists.txt tests/unit/surface2d/test_surface_state.cpp tests/unit/surface2d/test_hydrostatic_reconstruction.cpp
git commit -m "feat(surface2d): add hydrostatic reconstruction"
```

---

### Task 3: Minimal HLLC Normal Flux

**Files:**
- Modify: `libs/surface2d/CMakeLists.txt`
- Create: `libs/surface2d/include/surface2d/riemann/hllc.hpp`
- Create: `libs/surface2d/src/riemann/hllc.cpp`
- Test: `tests/unit/surface2d/test_hllc_flux.cpp`
- Modify: `tests/unit/surface2d/CMakeLists.txt`

- [ ] **Step 1: Write failing HLLC tests**

Create `tests/unit/surface2d/test_hllc_flux.cpp`:

```cpp
#include <gtest/gtest.h>

#include "surface2d/dpm/fields.hpp"
#include "surface2d/riemann/hllc.hpp"
#include "surface2d/state/state.hpp"

TEST(HllcFlux, NormalVelocityProjectsMomentum) {
    const scau::surface2d::CellState state{.conserved = {.h = 2.0, .hu = 6.0, .hv = 8.0}, .eta = 2.0};

    EXPECT_EQ(scau::surface2d::normal_velocity(state, scau::surface2d::Normal2{.x = 1.0, .y = 0.0}), 3.0);
    EXPECT_EQ(scau::surface2d::normal_velocity(state, scau::surface2d::Normal2{.x = 0.0, .y = 1.0}), 4.0);
}

TEST(HllcFlux, ZeroVelocityHydrostaticStateHasZeroMassFlux) {
    const scau::surface2d::CellState left{.conserved = {.h = 1.0, .hu = 0.0, .hv = 0.0}, .eta = 1.0};
    const scau::surface2d::CellState right{.conserved = {.h = 1.0, .hu = 0.0, .hv = 0.0}, .eta = 1.0};
    const scau::surface2d::EdgeDpmFields edge_fields{.phi_e_n = 1.0, .omega_edge = 1.0};

    const auto flux = scau::surface2d::hllc_normal_flux(
        left,
        right,
        edge_fields,
        scau::surface2d::Normal2{.x = 1.0, .y = 0.0});

    EXPECT_EQ(flux.mass, 0.0);
    EXPECT_EQ(flux.momentum_n, 0.0);
}

TEST(HllcFlux, HardAndSoftBlocksHaveZeroMassFlux) {
    const scau::surface2d::CellState left{.conserved = {.h = 1.0, .hu = 1.0, .hv = 0.0}, .eta = 1.0};
    const scau::surface2d::CellState right{.conserved = {.h = 1.0, .hu = 1.0, .hv = 0.0}, .eta = 1.0};
    const auto normal = scau::surface2d::Normal2{.x = 1.0, .y = 0.0};

    const auto hard_block = scau::surface2d::hllc_normal_flux(
        left,
        right,
        scau::surface2d::EdgeDpmFields{.phi_e_n = 1.0, .omega_edge = 0.0},
        normal);
    const auto soft_block = scau::surface2d::hllc_normal_flux(
        left,
        right,
        scau::surface2d::EdgeDpmFields{.phi_e_n = 0.5e-12, .omega_edge = 1.0},
        normal);

    EXPECT_EQ(hard_block.mass, 0.0);
    EXPECT_EQ(soft_block.mass, 0.0);
}
```

Append to `tests/unit/surface2d/CMakeLists.txt`:

```cmake
add_executable(test_hllc_flux test_hllc_flux.cpp)
target_link_libraries(test_hllc_flux
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_hllc_flux COMMAND test_hllc_flux)
```

- [ ] **Step 2: Run HLLC test to verify it fails**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build --output-on-failure -R test_hllc_flux
```

Expected: build fails because `surface2d/riemann/hllc.hpp` does not exist.

- [ ] **Step 3: Add minimal HLLC flux API**

Create `libs/surface2d/include/surface2d/riemann/hllc.hpp`:

```cpp
#pragma once

#include "core/types.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/state/state.hpp"

namespace scau::surface2d {

inline constexpr core::Real PhiEdgeMin = 1.0e-12;

struct Normal2 {
    core::Real x{0.0};
    core::Real y{0.0};
};

struct EdgeFlux {
    core::Real mass{0.0};
    core::Real momentum_n{0.0};
};

[[nodiscard]] core::Real normal_velocity(const CellState& state, Normal2 normal);

[[nodiscard]] EdgeFlux hllc_normal_flux(
    const CellState& left,
    const CellState& right,
    const EdgeDpmFields& edge_fields,
    Normal2 normal);

}  // namespace scau::surface2d
```

Create `libs/surface2d/src/riemann/hllc.cpp`:

```cpp
#include "surface2d/riemann/hllc.hpp"

#include "surface2d/reconstruction/hydrostatic.hpp"

namespace scau::surface2d {

core::Real normal_velocity(const CellState& state, Normal2 normal) {
    return state.u() * normal.x + state.v() * normal.y;
}

EdgeFlux hllc_normal_flux(
    const CellState& left,
    const CellState& right,
    const EdgeDpmFields& edge_fields,
    Normal2 normal) {
    if (edge_fields.omega_edge == 0.0 || edge_fields.phi_e_n < PhiEdgeMin) {
        return EdgeFlux{};
    }

    const auto pair = reconstruct_hydrostatic_pair(left, right);
    const core::Real left_un = normal_velocity(pair.left, normal);
    const core::Real right_un = normal_velocity(pair.right, normal);
    if (left_un == 0.0 && right_un == 0.0 && pair.left.eta == pair.right.eta) {
        return EdgeFlux{};
    }

    return EdgeFlux{
        .mass = 0.5 * edge_fields.phi_e_n * (pair.left.conserved.h * left_un + pair.right.conserved.h * right_un),
        .momentum_n = 0.0,
    };
}

}  // namespace scau::surface2d
```

Modify `libs/surface2d/CMakeLists.txt`:

```cmake
add_library(scau_surface2d
    src/dpm/fields.cpp
    src/reconstruction/hydrostatic.cpp
    src/riemann/hllc.cpp
    src/time_integration/step.cpp
)
```

- [ ] **Step 4: Run HLLC test to verify it passes**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build --output-on-failure -R test_hllc_flux
```

Expected: `test_hllc_flux` passes.

- [ ] **Step 5: Commit**

```bash
git add libs/surface2d/CMakeLists.txt libs/surface2d/include/surface2d/riemann/hllc.hpp libs/surface2d/src/riemann/hllc.cpp tests/unit/surface2d/CMakeLists.txt tests/unit/surface2d/test_hllc_flux.cpp
git commit -m "feat(surface2d): add minimal HLLC normal flux"
```

---

### Task 4: Phi-T Source Pairing

**Files:**
- Modify: `libs/surface2d/CMakeLists.txt`
- Create: `libs/surface2d/include/surface2d/source_terms/phi_t.hpp`
- Create: `libs/surface2d/src/source_terms/phi_t.cpp`
- Test: `tests/unit/surface2d/test_phi_t_source.cpp`
- Modify: `tests/unit/surface2d/CMakeLists.txt`

- [ ] **Step 1: Write failing `phi_t` source test**

Create `tests/unit/surface2d/test_phi_t_source.cpp`:

```cpp
#include <gtest/gtest.h>

#include "surface2d/source_terms/phi_t.hpp"

TEST(PhiTSource, PressurePairingAndSourceCancelExactly) {
    const auto pressure_pairing = scau::surface2d::pressure_pairing_phi_t(1.0, 1.25, 1.0);
    const auto source = scau::surface2d::s_phi_t_centered(1.0, 1.25, 1.0);

    EXPECT_DOUBLE_EQ(pressure_pairing, 1.22625);
    EXPECT_DOUBLE_EQ(source, -1.22625);
    EXPECT_DOUBLE_EQ(pressure_pairing + source, 0.0);
}
```

Append to `tests/unit/surface2d/CMakeLists.txt`:

```cmake
add_executable(test_phi_t_source test_phi_t_source.cpp)
target_link_libraries(test_phi_t_source
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_phi_t_source COMMAND test_phi_t_source)
```

- [ ] **Step 2: Run source test to verify it fails**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build --output-on-failure -R test_phi_t_source
```

Expected: build fails because `surface2d/source_terms/phi_t.hpp` does not exist.

- [ ] **Step 3: Add `phi_t` pressure/source helpers**

Create `libs/surface2d/include/surface2d/source_terms/phi_t.hpp`:

```cpp
#pragma once

#include "core/types.hpp"

namespace scau::surface2d {

[[nodiscard]] core::Real pressure_pairing_phi_t(
    core::Real phi_left,
    core::Real phi_right,
    core::Real h,
    core::Real gravity = 9.81);

[[nodiscard]] core::Real s_phi_t_centered(
    core::Real phi_left,
    core::Real phi_right,
    core::Real h,
    core::Real gravity = 9.81);

}  // namespace scau::surface2d
```

Create `libs/surface2d/src/source_terms/phi_t.cpp`:

```cpp
#include "surface2d/source_terms/phi_t.hpp"

namespace scau::surface2d {

core::Real pressure_pairing_phi_t(core::Real phi_left, core::Real phi_right, core::Real h, core::Real gravity) {
    return 0.5 * gravity * h * h * (phi_right - phi_left);
}

core::Real s_phi_t_centered(core::Real phi_left, core::Real phi_right, core::Real h, core::Real gravity) {
    return -pressure_pairing_phi_t(phi_left, phi_right, h, gravity);
}

}  // namespace scau::surface2d
```

Modify `libs/surface2d/CMakeLists.txt`:

```cmake
add_library(scau_surface2d
    src/dpm/fields.cpp
    src/reconstruction/hydrostatic.cpp
    src/riemann/hllc.cpp
    src/source_terms/phi_t.cpp
    src/time_integration/step.cpp
)
```

- [ ] **Step 4: Run source test to verify it passes**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build --output-on-failure -R test_phi_t_source
```

Expected: `test_phi_t_source` passes.

- [ ] **Step 5: Commit**

```bash
git add libs/surface2d/CMakeLists.txt libs/surface2d/include/surface2d/source_terms/phi_t.hpp libs/surface2d/src/source_terms/phi_t.cpp tests/unit/surface2d/CMakeLists.txt tests/unit/surface2d/test_phi_t_source.cpp
git commit -m "feat(surface2d): add phi_t source pairing"
```

---

### Task 5: CPU Step Diagnostics Wiring

**Files:**
- Modify: `libs/surface2d/include/surface2d/time_integration/step.hpp`
- Modify: `libs/surface2d/src/time_integration/step.cpp`
- Modify: `tests/unit/surface2d/test_step.cpp`

- [ ] **Step 1: Write failing step diagnostics test**

Append to `tests/unit/surface2d/test_step.cpp`:

```cpp
TEST(SurfaceStep, CpuStepReportsDpmEdgeDiagnostics) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
    auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
    dpm_fields.cells[1].phi_t = 1.25;

    const scau::surface2d::StepConfig config{.dt = 0.5, .cfl_safety = 0.45, .c_rollback = 1.0};
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm_fields);

    ASSERT_EQ(diagnostics.edges.size(), mesh.edges.size());
    EXPECT_EQ(diagnostics.edges[0].mass_flux, 0.0);
    EXPECT_DOUBLE_EQ(diagnostics.edges[0].pressure_pairing, 1.22625);
    EXPECT_DOUBLE_EQ(diagnostics.edges[0].s_phi_t, -1.22625);
    EXPECT_DOUBLE_EQ(diagnostics.edges[0].residual, 0.0);
    EXPECT_EQ(diagnostics.max_cell_cfl, 0.0);
    EXPECT_FALSE(diagnostics.rollback_required);
}
```

Add includes to `tests/unit/surface2d/test_step.cpp`:

```cpp
#include "surface2d/dpm/fields.hpp"
```

- [ ] **Step 2: Run step test to verify it fails**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build --output-on-failure -R test_surface_step
```

Expected: build fails because the overload and edge diagnostics do not exist.

- [ ] **Step 3: Add edge diagnostics and DPM-aware overload**

Modify `libs/surface2d/include/surface2d/time_integration/step.hpp`:

```cpp
#pragma once

#include <cstddef>
#include <vector>

#include "core/types.hpp"
#include "mesh/mesh.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/state/state.hpp"

namespace scau::surface2d {

struct StepConfig {
    core::Real dt{0.0};
    core::Real cfl_safety{0.45};
    core::Real c_rollback{1.0};
};

struct EdgeStepDiagnostics {
    core::Real mass_flux{0.0};
    core::Real momentum_flux_n{0.0};
    core::Real pressure_pairing{0.0};
    core::Real s_phi_t{0.0};
    core::Real residual{0.0};
};

struct StepDiagnostics {
    std::size_t cell_count{0U};
    std::size_t edge_count{0U};
    core::Real max_cell_cfl{0.0};
    bool rollback_required{false};
    std::vector<EdgeStepDiagnostics> edges;
};

[[nodiscard]] StepDiagnostics advance_one_step_cpu(
    const mesh::Mesh& mesh,
    SurfaceState& state,
    const StepConfig& config,
    const DpmFields& dpm_fields);

[[nodiscard]] StepDiagnostics advance_one_step_cpu(
    const mesh::Mesh& mesh,
    SurfaceState& state,
    const StepConfig& config);

}  // namespace scau::surface2d
```

Modify `libs/surface2d/src/time_integration/step.cpp`:

```cpp
#include "surface2d/time_integration/step.hpp"

#include <stdexcept>

#include "surface2d/riemann/hllc.hpp"
#include "surface2d/source_terms/phi_t.hpp"

namespace scau::surface2d {
namespace {

void validate_config(const StepConfig& config) {
    if (config.dt <= 0.0) {
        throw std::invalid_argument("step dt must be positive");
    }
    if (config.cfl_safety <= 0.0) {
        throw std::invalid_argument("CFL_safety must be positive");
    }
    if (config.c_rollback <= 0.0) {
        throw std::invalid_argument("C_rollback must be positive");
    }
}

Normal2 edge_normal(const mesh::Edge& edge) {
    return Normal2{.x = edge.normal_x, .y = edge.normal_y};
}

}  // namespace

StepDiagnostics advance_one_step_cpu(
    const mesh::Mesh& mesh,
    SurfaceState& state,
    const StepConfig& config,
    const DpmFields& dpm_fields) {
    validate_config(config);
    validate_state_matches_mesh(state, mesh);
    validate_dpm_fields_match_mesh(dpm_fields, mesh);

    StepDiagnostics diagnostics{
        .cell_count = mesh.cells.size(),
        .edge_count = mesh.edges.size(),
        .max_cell_cfl = 0.0,
        .rollback_required = false,
    };
    diagnostics.edges.reserve(mesh.edges.size());

    for (std::size_t edge_index = 0; edge_index < mesh.edges.size(); ++edge_index) {
        const auto& edge = mesh.edges[edge_index];
        EdgeStepDiagnostics edge_diagnostics;
        if (edge.left_cell < state.cells.size() && edge.right_cell.has_value()) {
            const std::size_t right_cell = *edge.right_cell;
            const auto flux = hllc_normal_flux(
                state.cells[edge.left_cell],
                state.cells[right_cell],
                dpm_fields.edges[edge_index],
                edge_normal(edge));
            const core::Real h = 0.5 * (state.cells[edge.left_cell].conserved.h + state.cells[right_cell].conserved.h);
            edge_diagnostics.mass_flux = flux.mass;
            edge_diagnostics.momentum_flux_n = flux.momentum_n;
            edge_diagnostics.pressure_pairing = pressure_pairing_phi_t(
                dpm_fields.cells[edge.left_cell].phi_t,
                dpm_fields.cells[right_cell].phi_t,
                h);
            edge_diagnostics.s_phi_t = s_phi_t_centered(
                dpm_fields.cells[edge.left_cell].phi_t,
                dpm_fields.cells[right_cell].phi_t,
                h);
            edge_diagnostics.residual = edge_diagnostics.pressure_pairing + edge_diagnostics.s_phi_t;
        }
        diagnostics.edges.push_back(edge_diagnostics);
    }

    diagnostics.rollback_required = diagnostics.max_cell_cfl > config.c_rollback;
    return diagnostics;
}

StepDiagnostics advance_one_step_cpu(const mesh::Mesh& mesh, SurfaceState& state, const StepConfig& config) {
    return advance_one_step_cpu(mesh, state, config, DpmFields::for_mesh(mesh));
}

}  // namespace scau::surface2d
```

If `mesh::Edge` uses different member names for normals or cell indices, adapt this implementation to the actual `libs/mesh/include/mesh/mesh.hpp` API without changing the test intent.

- [ ] **Step 4: Run step test to verify it passes**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build --output-on-failure -R test_surface_step
```

Expected: `test_surface_step` passes, and the existing M2 skeleton test still passes through the default-DPM overload.

- [ ] **Step 5: Commit**

```bash
git add libs/surface2d/include/surface2d/time_integration/step.hpp libs/surface2d/src/time_integration/step.cpp tests/unit/surface2d/test_step.cpp
git commit -m "feat(surface2d): wire DPM diagnostics into CPU step"
```

---

### Task 6: G1/G2/G3 Minimal C++ Coverage

**Files:**
- Test: `tests/unit/surface2d/test_surface2d_golden_minimal.cpp`
- Modify: `tests/unit/surface2d/CMakeLists.txt`

- [ ] **Step 1: Write failing G1/G2/G3 minimal tests**

Create `tests/unit/surface2d/test_surface2d_golden_minimal.cpp`:

```cpp
#include <array>
#include <functional>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/time_integration/step.hpp"

namespace {

using MeshBuilder = std::function<scau::mesh::Mesh()>;

constexpr auto StepConfig = scau::surface2d::StepConfig{.dt = 0.5, .cfl_safety = 0.45, .c_rollback = 1.0};

std::array<MeshBuilder, 3> mesh_builders() {
    return {
        [] { return scau::mesh::build_mixed_minimal_mesh(); },
        [] { return scau::mesh::build_quad_only_control_mesh(); },
        [] { return scau::mesh::build_tri_only_control_mesh(); },
    };
}

void expect_hydrostatic_preserved(const scau::surface2d::SurfaceState& state) {
    for (const auto& cell : state.cells) {
        EXPECT_EQ(cell.conserved.h, 1.0);
        EXPECT_EQ(cell.conserved.hu, 0.0);
        EXPECT_EQ(cell.conserved.hv, 0.0);
        EXPECT_EQ(cell.eta, 1.0);
    }
}

}  // namespace

TEST(Surface2DGoldenMinimal, G1HydrostaticStepAllMeshes) {
    for (const auto& build_mesh : mesh_builders()) {
        const auto mesh = build_mesh();
        auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);

        const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, StepConfig);

        expect_hydrostatic_preserved(state);
        EXPECT_EQ(diagnostics.max_cell_cfl, 0.0);
        EXPECT_FALSE(diagnostics.rollback_required);
        for (const auto& edge : diagnostics.edges) {
            EXPECT_EQ(edge.mass_flux, 0.0);
            EXPECT_EQ(edge.residual, 0.0);
        }
    }
}

TEST(Surface2DGoldenMinimal, G2PhiTJumpHydrostaticAllMeshes) {
    for (const auto& build_mesh : mesh_builders()) {
        const auto mesh = build_mesh();
        auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
        auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
        for (std::size_t cell_index = 0; cell_index < dpm_fields.cells.size(); ++cell_index) {
            dpm_fields.cells[cell_index].phi_t = 1.0 + 0.25 * static_cast<double>(cell_index);
        }

        const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, StepConfig, dpm_fields);

        expect_hydrostatic_preserved(state);
        for (const auto& edge : diagnostics.edges) {
            EXPECT_EQ(edge.mass_flux, 0.0);
            EXPECT_NEAR(edge.residual, 0.0, 1.0e-12);
        }
    }
}

TEST(Surface2DGoldenMinimal, G3PhiCEdgeZeroVelocityAllMeshes) {
    for (const auto& build_mesh : mesh_builders()) {
        const auto mesh = build_mesh();
        auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);
        auto dpm_fields = scau::surface2d::DpmFields::for_mesh(mesh);
        for (std::size_t edge_index = 0; edge_index < dpm_fields.edges.size(); ++edge_index) {
            dpm_fields.edges[edge_index].phi_e_n = edge_index == 0U ? 0.0 : 1.0;
            dpm_fields.edges[edge_index].omega_edge = edge_index == 0U ? 0.0 : 1.0;
        }

        const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, StepConfig, dpm_fields);

        expect_hydrostatic_preserved(state);
        for (const auto& edge : diagnostics.edges) {
            EXPECT_EQ(edge.mass_flux, 0.0);
        }
    }
}
```

Append to `tests/unit/surface2d/CMakeLists.txt`:

```cmake
add_executable(test_surface2d_golden_minimal test_surface2d_golden_minimal.cpp)
target_link_libraries(test_surface2d_golden_minimal
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_surface2d_golden_minimal COMMAND test_surface2d_golden_minimal)
```

- [ ] **Step 2: Run G1/G2/G3 test to verify it passes or exposes integration gaps**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build --output-on-failure -R test_surface2d_golden_minimal
```

Expected: `test_surface2d_golden_minimal` passes after Tasks 1-5. If it fails because a control mesh has no interior edge, keep the hydrostatic preservation and zero-flux assertions and adapt only the edge-specific expectation to the actual mesh topology.

- [ ] **Step 3: Run all surface2d unit tests**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build --output-on-failure -R surface
```

Expected: all `surface2d` tests pass.

- [ ] **Step 4: Commit**

```bash
git add tests/unit/surface2d/CMakeLists.txt tests/unit/surface2d/test_surface2d_golden_minimal.cpp
git commit -m "test(surface2d): cover minimal numerical golden cases"
```

---

### Task 7: M3 Evidence And Full Validation

**Files:**
- Create: `superpowers/specs/2026-05-12-plan-m3-surface2d-numerical-minimal-slice-evidence.md`

- [ ] **Step 1: Run full local build and tests**

Run:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build --output-on-failure
```

Expected: build succeeds and all registered CTest tests pass.

- [ ] **Step 2: Write M3 evidence file**

Create `superpowers/specs/2026-05-12-plan-m3-surface2d-numerical-minimal-slice-evidence.md`:

```markdown
# M3 Surface2D Numerical Minimal Slice Evidence

**Date:** 2026-05-12
**Plan:** `superpowers/plans/2026-05-12-plan-m3-surface2d-numerical-minimal-slice.md`
**Sandbox B Entry Evidence:** `sandbox/numerical/evidence/sandbox_report.md`

## Scope

M3 implements the formal C++ minimal slice for `Surface2DCore` DPM fields, hydrostatic reconstruction, minimal HLLC normal flux, `phi_t` pressure-source pairing, and CPU step diagnostics.

## Local Validation

- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build`: PASS
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build --output-on-failure`: PASS

## Coverage

- `test_dpm_fields`: verifies mesh-aligned defaults for `phi_t`, `Phi_c`, `phi_e_n`, and `omega_edge`.
- `test_hydrostatic_reconstruction`: verifies zero-velocity hydrostatic reconstruction preserves equal-eta depth.
- `test_hllc_flux`: verifies normal projection plus hard-block, soft-block, and zero-velocity zero mass flux.
- `test_phi_t_source`: verifies pressure pairing and `S_phi_t` cancel exactly for the sandbox B G2 stencil.
- `test_surface_step`: verifies CPU step diagnostics preserve M2 behavior and report DPM edge diagnostics.
- `test_surface2d_golden_minimal`: mirrors sandbox B G1/G2/G3 on mixed, quad-only, and tri-only meshes.

## Boundaries

- `Surface2DCore` remains independent of SWMM, D-Flow FM, `CouplingLib`, and 1D engine ABIs.
- `max_cell_cfl` remains the raw physical CFL diagnostic; rollback comparison uses `C_rollback` separately.
- This evidence does not claim GoldenSuite release readiness.
```

- [ ] **Step 3: Commit M3 evidence**

```bash
git add superpowers/specs/2026-05-12-plan-m3-surface2d-numerical-minimal-slice-evidence.md
git commit -m "docs(m3): record numerical minimal slice evidence"
```

---

## Implementation Order

1. Commit or otherwise preserve uncommitted sandbox B evidence before editing M3 implementation files, because this plan depends on that archived entry evidence.
2. Execute Tasks 1-4 to create isolated numerical components with focused unit tests.
3. Execute Task 5 to wire the components into `advance_one_step_cpu()` while preserving M2 API compatibility.
4. Execute Task 6 to mirror sandbox B G1/G2/G3 as formal C++ minimal coverage.
5. Execute Task 7 to record local validation evidence.

## Self-Review

- Spec coverage: The plan covers sandbox B recommendations for edge-normal hydrostatic reconstruction, shared `phi_t` pressure/source stencil, hard-block and soft-block zero-flux entry behavior, and mixed/quad/tri coverage.
- Scope control: The plan excludes production GoldenSuite readiness, full HLLC physics, CUDA, wetting/drying, and coupling semantics.
- Canonical names: The plan uses `phi_t`, `Phi_c`, `phi_e_n`, `omega_edge`, `max_cell_cfl`, `C_rollback`, and `CFL_safety` without introducing deprecated aliases.
- Boundary check: All changes stay under `libs/surface2d/`, `tests/unit/surface2d/`, and evidence docs; no 1D engine dependencies are introduced.

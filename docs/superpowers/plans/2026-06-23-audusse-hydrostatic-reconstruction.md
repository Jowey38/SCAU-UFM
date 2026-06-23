# Audusse Hydrostatic Reconstruction (Main-Spec §5.4) + Enable G5 — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:subagent-driven-development (two-stage review per task). **Prerequisite: subagent review credits available** — this is a foundational kernel change and must get the spec-compliance + code-quality review gates. Steps use `- [ ]` checkboxes.

**Goal:** Replace the non-spec `reconstruct_hydrostatic_pair` with the main-spec §5.4 Audusse hydrostatic reconstruction, migrate the tests that encode the old eta-clipping behavior, and re-enable the G5 sloping-bed lake-at-rest well-balancing GoldenTest (which then passes bit-wise, 1e-12).

**Why:** `S_phi_t` scope-A delivered flat-bed well-balancing (G4 bit-wise). Sloping-bed well-balancing (G5) is blocked because the shared reconstruction deviates from §5.4: it uses `h_i* = max(0, eta_min − z_b_i)` instead of Audusse `z_b* = max(z_b_L,z_b_R); h_i* = max(0, eta_i − z_b*)`. Over variable bed this gives `h_L* ≠ h_R*` at rest, so standard HLLC emits a spurious advective flux. The WB pairing itself is already correct (`left_normal = −½ g phi_t_i h_i²`, independent of h̄; proven by G4 and the well_balanced module tests).

**Tech Stack:** C++20, CMake/CTest, GoogleTest, MSVC (`SCAU_WARNINGS_AS_ERRORS=ON`, ASCII-only comments). Build: `cmake --build --preset windows-msvc`; test: `ctest --preset windows-msvc -R <re> --timeout 120`. On `LNK1168`: kill stray `test_*`/`ctest`, delete locked exe, rebuild until `grep -c LNK1168 == 0`.

**Authoritative:** main Spec §5.4 (z_b* = max(z_{b,L}, z_{b,R}); h_L* = max(eta_L − z_b*, 0); h_R* = max(eta_R − z_b*, 0); reconstructed momentum = h* · u with velocity recovered before reconstruction). Evidence of the gap: `superpowers/specs/2026-06-23-s-phi-t-momentum-coupling-evidence.md` §3.

---

## File Structure

- `libs/surface2d/src/reconstruction/hydrostatic.cpp` (modify) — the only production change.
- `tests/unit/surface2d/test_hydrostatic_reconstruction.cpp` (modify) — migrate the clipping test to a real bed-step (the only place that must change expected reconstruction values).
- `tests/unit/surface2d/test_well_balanced_sloping_bed_at_rest.cpp` (modify) — remove `DISABLED_` prefix.
- Possibly `tests/unit/surface2d/test_dpm_edge_source_conservation.cpp` and other HLLC tests using `set_edge_aligned_state` (modify ONLY if the Audusse change shifts a hard-coded expectation; their edge-diagnostic assertions recompute via `hllc_normal_flux` so most follow the implementation automatically — verify, do not pre-emptively edit).

---

## Task 1: Audusse reconstruction (TDD on the reconstruction unit test)

**Files:** Modify `libs/surface2d/src/reconstruction/hydrostatic.cpp`, `tests/unit/surface2d/test_hydrostatic_reconstruction.cpp`.

- [ ] **Step 1: Migrate the reconstruction unit tests to spec §5.4 expectations.**

In `test_hydrostatic_reconstruction.cpp`, `EqualEtaZeroVelocityPreservesDepth` stays as-is (flat bed, eta=1 both, expects h*=1, eta*=1 — Audusse gives z_b*=0, h*=1, and reconstructed eta = z_b* + h* = 1). Confirm it still holds under the new formula; no edit expected.

Replace `PreservesVelocityWhenDepthIsClipped` (which used flat bed eta_L=2, eta_R=1 and expected h_L* clipped to 1 — non-Audusse) with a test where Audusse genuinely clips, i.e. a bed step where the neighbour bed rises above this cell's free surface:

```cpp
TEST(HydrostaticReconstruction, AudusseClipsDepthAtHigherNeighbourBedAndPreservesVelocity) {
    // Left: deep cell, z_b_L = 0 (eta_L = h_L = 2), moving.
    // Right: high bed z_b_R = 1.5 with shallow water (eta_R = 1.0 < z_b... no:
    // choose eta_R = 1.0, h_R = ... ) -> we want z_b* = max(0, z_b_R) to clip left.
    // Use right bed z_b_R = 1.2, h_R = 0.3 => eta_R = 1.5; left eta_L = 2.0, h_L = 2.0 (z_b_L = 0).
    const scau::surface2d::CellState left{.conserved = {.h = 2.0, .hu = 4.0, .hv = -2.0}, .eta = 2.0};
    const scau::surface2d::CellState right{.conserved = {.h = 0.3, .hu = 0.15, .hv = 0.075}, .eta = 1.5};

    const auto pair = scau::surface2d::reconstruct_hydrostatic_pair(left, right);

    // z_b_L = 0, z_b_R = 1.2, z_b* = 1.2.
    // h_L* = max(0, 2.0 - 1.2) = 0.8 ; h_R* = max(0, 1.5 - 1.2) = 0.3.
    EXPECT_DOUBLE_EQ(pair.left.conserved.h, 0.8);
    EXPECT_DOUBLE_EQ(pair.right.conserved.h, 0.3);
    // Velocity (u = hu/h) preserved from the ORIGINAL cell, applied to h*:
    // u_L = 4.0/2.0 = 2.0 -> hu_L* = 0.8*2.0 = 1.6 ; v_L = -1.0 -> hv_L* = -0.8.
    EXPECT_DOUBLE_EQ(pair.left.conserved.hu, 1.6);
    EXPECT_DOUBLE_EQ(pair.left.conserved.hv, -0.8);
    // u_R = 0.15/0.3 = 0.5 -> hu_R* = 0.3*0.5 = 0.15 ; v_R = 0.25 -> hv_R* = 0.075.
    EXPECT_DOUBLE_EQ(pair.right.conserved.hu, 0.15);
    EXPECT_DOUBLE_EQ(pair.right.conserved.hv, 0.075);
}
```

- [ ] **Step 2: Run to confirm the new test fails under the current (non-Audusse) code.**
Run: `cmake --build --preset windows-msvc --target test_hydrostatic_reconstruction && ctest --preset windows-msvc -R "^test_hydrostatic_reconstruction$" -V`. Expected: FAIL (current code gives h_L* = max(0, eta_min − z_b_L) = max(0, 1.5 − 0) = 1.5, not 0.8).

- [ ] **Step 3: Implement Audusse §5.4.** Replace the body of `reconstruct_hydrostatic_pair` in `libs/surface2d/src/reconstruction/hydrostatic.cpp` with:

```cpp
HydrostaticPair reconstruct_hydrostatic_pair(const CellState& left, const CellState& right) {
    const core::Real z_b_left = left.eta - left.conserved.h;
    const core::Real z_b_right = right.eta - right.conserved.h;
    const core::Real z_b_star = std::max(z_b_left, z_b_right);
    const core::Real left_depth = std::max(0.0, left.eta - z_b_star);
    const core::Real right_depth = std::max(0.0, right.eta - z_b_star);
    return HydrostaticPair{
        .left = CellState{
            .conserved = {.h = left_depth, .hu = left_depth * left.u(), .hv = left_depth * left.v()},
            .eta = z_b_star + left_depth},
        .right = CellState{
            .conserved = {.h = right_depth, .hu = right_depth * right.u(), .hv = right_depth * right.v()},
            .eta = z_b_star + right_depth},
    };
}
```
(`left.u()`/`left.v()` already guard divide-by-zero for dry cells, returning 0 — verify in `state.hpp`. `<algorithm>` is already included for `std::max`.)

- [ ] **Step 4: Run the reconstruction test; confirm PASS** (both `EqualEta...` and the new `AudusseClips...`).

- [ ] **Step 5: Commit.**
```
git add libs/surface2d/src/reconstruction/hydrostatic.cpp tests/unit/surface2d/test_hydrostatic_reconstruction.cpp
git commit -m "fix(surface2d): hydrostatic reconstruction to main-spec 5.4 Audusse (z_b*=max)"
```

---

## Task 2: Full-suite regression + migrate any bed-step expectations

**Files:** Possibly `tests/unit/surface2d/test_dpm_edge_source_conservation.cpp` and other HLLC tests (only if needed).

- [ ] **Step 1: Build all and run the full surface2d suite.**
Run: kill stray procs; `cmake --build --preset windows-msvc 2>&1 | tee /tmp/aud.txt`; confirm `grep -cE "LNK1168|error C|: error" /tmp/aud.txt` == 0; `ctest --preset windows-msvc --timeout 120`.

- [ ] **Step 2: For each failure, classify and fix.**
The Audusse change only alters reconstruction when the bed differs across an edge (`z_b_L ≠ z_b_R`, i.e. `set_edge_aligned_state`-style bed steps). Flat-bed tests (G1/G2/G3/G4, lake-at-rest, wetting/drying, friction, rainfall) are unaffected. For any failing assertion:
- If it asserts a per-edge diagnostic recomputed via `hllc_normal_flux(...)` (M249 pattern), it follows the implementation and should still pass; if not, the test had a stale hard-coded constant — recompute the expected from the Audusse-reconstructed states and update, documenting why.
- Do NOT weaken any conservation/antisymmetry/mass-balance assertion. If a failure looks like a real regression (not a stale constant), STOP and escalate.

- [ ] **Step 3: Commit any test migrations** with a message explaining each changed expectation.

---

## Task 3: Enable G5

**Files:** Modify `tests/unit/surface2d/test_well_balanced_sloping_bed_at_rest.cpp`.

- [ ] **Step 1: Remove the `DISABLED_` prefix** from `DISABLED_SlopingBedKeepsMomentumZero` and delete the DISABLED explanation comment block (keep the physics comment).
- [ ] **Step 2: Build + run G5.** Expected: PASS, residual < 1e-12 (bit-wise, mesh-independent: the WB single-sided pairing reduces to the cell-center constant `−½ g phi_t_i h_i²` and the Audusse reconstruction makes the advective HLLC flux zero at rest).
- [ ] **Step 3: Full suite green; commit.**
```
git add tests/unit/surface2d/test_well_balanced_sloping_bed_at_rest.cpp
git commit -m "test(surface2d): enable G5 sloping-bed lake-at-rest (Audusse reconstruction)"
```

---

## Task 4: Evidence + records
- [ ] Update `superpowers/specs/2026-06-23-s-phi-t-momentum-coupling-evidence.md` §3 to "resolved" (or add a short follow-up evidence doc); note G5 now bit-wise; update `superpowers/INDEX.md`, `.wolf/anatomy.md`, `.wolf/memory.md`, `.wolf/cerebrum.md`. Commit.

## Self-Review
- Spec coverage: §5.4 Audusse formula → Task 1 Step 3; reconstruction tests → Task 1 Step 1; sloping-bed WB → Task 3; regression → Task 2.
- Placeholder scan: none — exact formula and exact test values given.
- Risk: the only judgment point is Task 2 Step 2 (classifying bed-step test failures). Flat-bed coverage is provably unaffected (z_b_L=z_b_R ⇒ z_b*=z_b_i ⇒ h_i*=h_i, identical to the old formula when eta_L=eta_R; and when eta differs on flat bed the old code clipped while Audusse does not — that is the intended spec correction, caught by Task 1's migrated test).

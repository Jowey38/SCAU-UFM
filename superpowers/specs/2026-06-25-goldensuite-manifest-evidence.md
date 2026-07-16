# GoldenSuite Manifest + CI Gate Evidence

**Date:** 2026-06-25
**Scope:** `tests/golden/`, `.github/workflows/ci.yml`, `superpowers/INDEX.md`
**Design:** `docs/superpowers/specs/2026-06-24-goldensuite-manifest-design.md`
**Plan:** `docs/superpowers/plans/2026-06-25-goldensuite-manifest.md`

## 1. Manifest completeness

`tests/golden/suite_manifest/goldensuite.json` is now the single source of truth for the full §10.2 matrix.

- **Implemented + active gate:** G1 `hydrostatic_step`, G2 `phi_t_jump_hydrostatic`, G3 `phi_c_edge_zero_velocity`, G4 `narrow_gap_blockage`, G5 `dpm_drag_decay`, G6 `phi_c_spd_reject`
- **Pending + inactive:** G7 `stcf_v4_to_v5_migration`, G8 `swmm_single_pipe_surcharge`, G9 `cpu_gpu_deterministic_match`, G10 `snapshot_replay_mass_deficit`, G11 `dflowfm_river_steady`, G12 `dual_engine_shared_cell`

Manifest-check result:

```text
OK goldensuite manifest completeness passes
```

## 2. First local gate run

Command:

```bash
ctest --preset windows-msvc -L golden --timeout 120
```

Result:

```text
100% tests passed, 0 tests failed out of 6
Label Time Summary:
golden    =   1.88 sec*proc (6 tests)
```

## 3. Criterion strategy landed

- **G1-G3** use `tests/golden/golden_tolerances.hpp` and the spec-derived `u_hydro_tol` / `eta_tol` constants (1e-12).
- **G4** uses exact-zero assertions (`EXPECT_EQ(..., 0.0)`) because hard-block returns literal zero flux.
- **G5** uses semantic split assertions: momentum must strictly decay versus a no-friction control run; recomposed mass aggregate uses `EXPECT_NEAR(..., conservation_near_tol)` with `1e-9` anti-flaky tolerance.
- **G6** is fail-closed (`EXPECT_THROW(..., std::invalid_argument)`).

## 4. CI gate wiring

Two new CI jobs are now declared in `.github/workflows/ci.yml`:

- `golden-suite-gate` — runs `ctest -L golden` on both linux-gcc and windows-msvc and is intended to block merge.
- `goldensuite-manifest-check` — runs the Python manifest checker and enforces full-matrix completeness + same-name/path invariants.

## 5. Boundaries

- Sloping-bed lake-at-rest remains a surface2d unit regression and is **not** a GoldenSuite G_n because §10.2 defines only G1-G12.
- The active gate covers only the currently implemented surface2d subset G1-G6.
- G7-G12 remain visible in the manifest so GoldenSuite incompleteness is explicit rather than hidden.

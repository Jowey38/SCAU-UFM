# M4 Surface2D CFL Rollback Conservation Evidence

**Date:** 2026-05-13
**Plan:** `superpowers/plans/2026-05-13-plan-m4-surface2d-cfl-rollback-conservation.md`
**M3 Basis:** `superpowers/specs/2026-05-12-plan-m3-surface2d-numerical-minimal-slice-evidence.md`

## Scope

M4 adds raw physical CFL diagnostics, `C_rollback` rollback comparison, internal-edge conservative mass residuals, and a minimal CPU reference depth update for `Surface2DCore`.

## Local Validation

- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc`: PASS
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "test_(surface_state|surface_step|dpm_fields|hydrostatic_reconstruction|hllc_flux|phi_t_source|surface2d_golden_minimal|cfl_rollback|conservative_update)$"`: PASS, 9/9 tests passed
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure`: PASS, 13/13 tests passed

## Coverage

- `test_cfl_rollback`: verifies `max_cell_cfl` remains raw physical CFL and rollback compares against `C_rollback` only.
- `test_conservative_update`: verifies isolated internal-edge mass residual antisymmetry and finite-volume depth update.
- `test_hydrostatic_reconstruction`: verifies clipped hydrostatic reconstruction preserves cell velocity for downstream flux computation.
- Existing M3 Surface2D tests remain passing for DPM fields, HLLC degeneracies, `phi_t` source pairing, CPU step diagnostics, and G1/G2/G3 minimal coverage.

## Boundaries

- `Surface2DCore` remains independent of SWMM, D-Flow FM, `CouplingLib`, and 1D engine ABIs.
- `max_cell_cfl` remains the raw physical CFL diagnostic; it is not scaled by `CFL_safety`.
- Rollback uses `C_rollback` separately from `CFL_safety`.
- This evidence does not claim GoldenSuite release readiness.

# M5 Surface2D Wetting Boundary Evidence

**Date:** 2026-05-14
**Plan:** `superpowers/plans/2026-05-13-plan-m5-surface2d-wetting-boundary.md`
**M4 Basis:** `superpowers/specs/2026-05-13-plan-m4-surface2d-cfl-rollback-conservation-evidence.md`

## Scope

M5 adds configurable wet/dry thresholding, dry-side HLLC flux blocking, non-negative finite-volume depth updates, and explicit boundary-edge behavior coverage for `Surface2DCore`.

## Local Validation

- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc`: PASS
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "test_(surface_state|surface_step|dpm_fields|hydrostatic_reconstruction|hllc_flux|phi_t_source|surface2d_golden_minimal|cfl_rollback|conservative_update|wetting_drying|boundary_flux)$"`: PASS, 11/11 tests passed
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure`: PASS, 15/15 tests passed

## Coverage

- `test_wetting_drying`: verifies dry-cell flux blocking and non-negative depth update under `StepConfig::h_min`.
- `test_boundary_flux`: documents current boundary-edge diagnostic behavior and confirms boundary residual updates remain out of scope.
- `test_surface_step`: extended with invalid `h_min` rejection.
- Existing M4/M3 Surface2D tests remain passing.

## Boundaries

- `Surface2DCore` remains independent of SWMM, D-Flow FM, `CouplingLib`, and 1D engine ABIs.
- Boundary-edge residual update semantics remain out of scope until explicit boundary state semantics are designed.
- This evidence does not claim GoldenSuite release readiness.

# M7 Surface2D Momentum Transport Evidence

**Date:** 2026-05-16
**Plan:** `superpowers/plans/2026-05-16-plan-m7-surface2d-momentum-transport.md`
**M6 Basis:** `superpowers/specs/2026-05-14-plan-m6-surface2d-boundary-conditions-evidence.md`

## Scope

M7 adds per-cell momentum residual accumulation and finite-volume `hu`/`hv` updates for `Surface2DCore`. Momentum transport uses the existing HLLC mass flux as the carrier; the upwind cell state is selected by the sign of `mass_flux`. The HLLC `momentum_n` formula remains zero in this slice; the pressure term `0.5 * g * h^2` and the full HLLC normal momentum flux are deferred.

## Local Validation

- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc`: PASS
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "test_(surface_state|surface_step|dpm_fields|hydrostatic_reconstruction|hllc_flux|phi_t_source|surface2d_golden_minimal|cfl_rollback|conservative_update|wetting_drying|boundary_flux|boundary_conditions|momentum_transport)$"`: PASS, 13/13 tests passed
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure`: PASS, 17/17 tests passed

## Coverage

- `test_momentum_transport`: internal-edge upwind antisymmetry, `hu`/`hv` advanced by internal mass flux, dry-cell `hu`/`hv` zeroing after depth clamp, Open boundary momentum contribution, Wall boundary zero contribution.
- Existing M3/M4/M5/M6 Surface2D tests remain passing.

## Boundaries

- `Surface2DCore` remains independent of SWMM, D-Flow FM, `CouplingLib`, and 1D engine ABIs.
- HLLC momentum flux pressure term (`0.5 * g * h^2`) and full HLLC normal momentum flux remain out of scope.
- This evidence does not claim GoldenSuite release readiness.

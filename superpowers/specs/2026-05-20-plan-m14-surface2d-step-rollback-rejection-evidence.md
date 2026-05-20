# M14 Surface2D Step Rollback/Rejection Evidence

**Date:** 2026-05-20
**Design:** `superpowers/specs/2026-05-20-m14-surface2d-step-rollback-rejection-design.md`
**Plan:** `superpowers/plans/2026-05-20-plan-m14-surface2d-step-rollback-rejection.md`

## Scope

M14 adds focused CPU reference `Surface2DCore` regression coverage proving that rejected steps report rollback and do not commit depth or momentum updates to `SurfaceState`.

## Local Validation

- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_step_rollback_rejection --config Debug && build/windows-msvc/tests/unit/surface2d/Debug/test_step_rollback_rejection.exe`: PASS, 2/2 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug -R "surface_state|surface_step|dpm_fields|hydrostatic_reconstruction|hllc_flux|phi_t_source|surface2d_golden_minimal|cfl_rollback|dpm_edge_source_conservation|conservative_update|wetting_drying|boundary_flux|boundary_conditions|momentum_transport|pressure_momentum|hllc_wave_momentum|hllc_wave_mass|step_rollback_rejection" --output-on-failure`: PASS, 18/18 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure`: PASS, 22/22 tests passed.

## Coverage

- `test_step_rollback_rejection`: rejected CPU step reports `rollback_required == true`, keeps raw `max_cell_cfl > C_rollback`, emits attempted-step cell/edge diagnostics, and preserves every cell's `h`, `hu`, `hv`, and `eta`.
- `test_step_rollback_rejection`: accepted CPU step with permissive `C_rollback` keeps the existing state mutation path active.
- Existing CFL rollback tests continue to prove raw `max_cell_cfl` is not scaled by `CFL_safety` and is compared directly against `C_rollback`.

## Boundaries

- M14 does not add `CouplingLib`, global `snapshot / rollback / replay` APIs, `mass_deficit_account`, adaptive timestep selection, CUDA behavior, SWMM, D-Flow FM, 1D engine interfaces, or release-level GoldenSuite claims.

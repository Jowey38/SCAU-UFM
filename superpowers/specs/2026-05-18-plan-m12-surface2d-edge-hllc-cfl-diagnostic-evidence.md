# M12 Surface2D Edge HLLC CFL Diagnostic Evidence

**Date:** 2026-05-18
**Plan:** `superpowers/plans/2026-05-18-plan-m12-surface2d-edge-hllc-cfl-diagnostic.md`
**M11 Basis:** `superpowers/specs/2026-05-18-plan-m11-surface2d-hllc-vector-momentum-projection-evidence.md`

## Scope

M12 replaces the old cell-speed-over-area CFL approximation with an edge-based raw HLLC CFL diagnostic in the CPU reference `Surface2DCore` time-integration path. It preserves `rollback_required = max_cell_cfl > C_rollback`, keeps `cfl_safety` out of the raw diagnostic, and does not introduce adaptive timestep selection.

## Local Validation

- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_cfl_rollback --config Debug && build/windows-msvc/tests/unit/surface2d/Debug/test_cfl_rollback.exe`: PASS, 4/4 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_cfl_rollback test_surface_step test_hllc_flux test_hllc_wave_mass test_hllc_wave_momentum --config Debug && PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug -R "^(test_cfl_rollback|test_surface_step|test_hllc_flux|test_hllc_wave_mass|test_hllc_wave_momentum)$" --output-on-failure`: PASS, 5/5 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --config Debug`: PASS.
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug -R "surface_state|surface_step|dpm_fields|hydrostatic_reconstruction|hllc_flux|phi_t_source|surface2d_golden_minimal|cfl_rollback|conservative_update|wetting_drying|boundary_flux|boundary_conditions|momentum_transport|pressure_momentum|hllc_wave_momentum|hllc_wave_mass" --output-on-failure`: PASS, 16/16 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure`: PASS, 20/20 tests passed.

## Coverage

- `test_cfl_rollback`: raw CFL remains unscaled by `cfl_safety`, rollback compares only against `C_rollback`, static water reports gravity-wave CFL, and edge length contributes to CFL.
- `test_surface_step`: step diagnostics report edge-HLLC CFL while preserving existing state and DPM edge diagnostics expectations.
- `test_surface2d_golden_minimal`: hydrostatic golden-minimal coverage preserves zero mass flux while expecting nonzero gravity-wave CFL.
- Existing HLLC flux, wave-speed, boundary, wetting/drying, conservative update, momentum, pressure, and golden-minimal tests remain passing.

## Boundaries

- `Surface2DCore` remains independent of SWMM, D-Flow FM, `CouplingLib`, and 1D engine ABIs.
- M12 remains limited to CPU reference Surface2D CFL diagnostics and rollback flag computation.
- M12 does not introduce adaptive timestep selection, CUDA backends, coupling ledger behavior, GoldenSuite release claims, or 1D engine interface changes.

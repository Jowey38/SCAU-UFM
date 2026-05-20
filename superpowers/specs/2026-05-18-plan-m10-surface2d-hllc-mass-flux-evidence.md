# M10 Surface2D HLLC Mass Flux Evidence

**Date:** 2026-05-18
**Plan:** `superpowers/plans/2026-05-18-plan-m10-surface2d-hllc-mass-flux.md`
**M9 Basis:** `superpowers/specs/2026-05-18-plan-m9-surface2d-hllc-wave-speed-momentum-evidence.md`

## Scope

M10 replaces the centered normal mass-flux diagnostic with an HLLC wave-speed branch-selected mass flux in the CPU reference `Surface2DCore` edge flux path. It preserves M9 HLLC normal momentum flux behavior, wet/dry blocking, DPM edge blocking, Wall/Open boundary behavior, CFL diagnostics, and momentum residual projection semantics.

## Local Validation

- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_hllc_flux test_hllc_wave_momentum test_hllc_wave_mass test_momentum_transport test_pressure_momentum --config Debug && PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug -R "^(test_hllc_flux|test_hllc_wave_momentum|test_hllc_wave_mass|test_momentum_transport|test_pressure_momentum)$" --output-on-failure`: PASS, 5/5 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --config Debug`: PASS.
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug -R "surface_state|surface_step|dpm_fields|hydrostatic_reconstruction|hllc_flux|phi_t_source|surface2d_golden_minimal|cfl_rollback|conservative_update|wetting_drying|boundary_flux|boundary_conditions|momentum_transport|pressure_momentum|hllc_wave_momentum|hllc_wave_mass" --output-on-failure`: PASS, 16/16 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure`: PASS, 20/20 tests passed.

## Coverage

- `test_hllc_flux`: HLLC branch-selected mass flux for right-going flow and `phi_e_n` scaling after branch selection.
- `test_hllc_wave_mass`: internal-edge mass sensitivity, dry internal-edge blocking, DPM edge blocking, Wall zero contribution, and Open boundary wave mass contribution.
- Existing Surface2D tests remain passing across the focused regression subset and full Surface2D CTest subset.

## Boundaries

- `Surface2DCore` remains independent of SWMM, D-Flow FM, `CouplingLib`, and 1D engine ABIs.
- M10 remains limited to the CPU reference HLLC edge flux path.
- M10 does not introduce CUDA backends, prescribed external boundary states, coupling ledger behavior, GoldenSuite release claims, or 1D engine interface changes.

# M9 Surface2D HLLC Wave-Speed Momentum Evidence

**Date:** 2026-05-18
**Plan:** `superpowers/plans/2026-05-17-plan-m9-surface2d-hllc-wave-speed-momentum.md`
**M8 Basis:** `superpowers/specs/2026-05-17-plan-m8-surface2d-pressure-momentum-evidence.md`

## Scope

M9 replaces the M8 pressure-only normal momentum diagnostic with a minimal HLLC wave-speed-based normal momentum flux in the CPU reference `Surface2DCore` edge flux path. It preserves existing mass flux, wet/dry blocking, DPM edge blocking, Wall/Open boundary behavior, and momentum residual projection semantics.

## Local Validation

- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_hllc_flux test_hllc_wave_momentum --config Debug`: PASS
- M9 target tests: PASS, `test_hllc_flux` and `test_hllc_wave_momentum` passed
- M7/M8/M9 regression subset: PASS, `test_hllc_flux`, `test_momentum_transport`, `test_pressure_momentum`, and `test_hllc_wave_momentum` passed
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --config Debug`: PASS
- Surface2D subset 15/15 PASS (`surface_state`, `surface_step`, `dpm_fields`, `hydrostatic_reconstruction`, `hllc_flux`, `phi_t_source`, `surface2d_golden_minimal`, `cfl_rollback`, `conservative_update`, `wetting_drying`, `boundary_flux`, `boundary_conditions`, `momentum_transport`, `pressure_momentum`, `hllc_wave_momentum`)
- Full CTest: PASS, 19/19 tests passed

## Coverage

- `test_hllc_flux`: normal velocity projection, hydrostatic pressure momentum at rest, HLLC wave speed bracketing, velocity-dependent normal momentum flux, and DPM hard/soft blocking.
- `test_hllc_wave_momentum`: internal-edge wave momentum sensitivity, dry internal-edge blocking, Wall zero contribution, and Open boundary wave momentum contribution.
- `test_surface2d_golden_minimal`: hydrostatic minimal golden now asserts zero mass flux while allowing pressure/HLLC normal momentum diagnostics introduced by M8/M9.
- Existing M3/M4/M5/M6/M7/M8 Surface2D tests remain passing.

## Boundaries

- `Surface2DCore` remains independent of SWMM, D-Flow FM, `CouplingLib`, and 1D engine ABIs.
- M9 remains limited to the CPU reference HLLC edge flux path.
- M9 does not introduce CUDA backends, prescribed external boundary states, coupling ledger behavior, GoldenSuite release claims, or 1D engine interface changes.

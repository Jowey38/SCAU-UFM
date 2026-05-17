# M8 Surface2D Pressure-Normal Momentum Evidence

**Date:** 2026-05-17
**Plan:** `superpowers/plans/2026-05-17-plan-m8-surface2d-pressure-momentum.md`
**M7 Basis:** `superpowers/specs/2026-05-16-plan-m7-surface2d-momentum-transport-evidence.md`

## Scope

M8 adds a scalar pressure-normal momentum contribution to `Surface2DCore` edge diagnostics and cell momentum residuals. It builds on the M7 upwind momentum transport without changing the HLLC mass flux path or introducing the full HLLC wave-speed solver.

## Local Validation

- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc`: PASS
- Surface2D subset 14/14 PASS (`surface_state`, `surface_step`, `dpm_fields`, `hydrostatic_reconstruction`, `hllc_flux`, `phi_t_source`, `surface2d_golden_minimal`, `cfl_rollback`, `conservative_update`, `wetting_drying`, `boundary_flux`, `boundary_conditions`, `momentum_transport`, `pressure_momentum`)
- Full CTest: PASS, 18/18 tests passed

## Coverage

- `test_pressure_momentum`: internal-edge pressure residual antisymmetry, dry-side blocking, Wall zero contribution, Open boundary contribution, and regression with M7 transport.
- Existing M3/M4/M5/M6/M7 Surface2D tests remain passing.

## Boundaries

- `Surface2DCore` remains independent of SWMM, D-Flow FM, `CouplingLib`, and 1D engine ABIs.
- Full HLLC normal momentum flux and wave-speed solving remain out of scope.
- This evidence does not claim GoldenSuite release readiness.

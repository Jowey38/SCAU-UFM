# M11 Surface2D HLLC Vector Momentum Projection Evidence

**Date:** 2026-05-18
**Plan:** `superpowers/plans/2026-05-18-plan-m11-surface2d-hllc-vector-momentum-projection.md`
**M10 Basis:** `superpowers/specs/2026-05-18-plan-m10-surface2d-hllc-mass-flux-evidence.md`

## Scope

M11 extends the CPU reference HLLC edge flux path with Cartesian vector momentum flux and uses that vector flux in the Surface2D momentum residual path. It preserves existing mass flux, normal momentum flux diagnostics, wet/dry blocking, DPM blocking, Wall/Open behavior, CFL diagnostics, and depth-update semantics.

## Local Validation

- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_hllc_flux test_hllc_wave_momentum test_hllc_wave_mass test_momentum_transport test_pressure_momentum --config Debug && PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug -R "^(test_hllc_flux|test_hllc_wave_momentum|test_hllc_wave_mass|test_momentum_transport|test_pressure_momentum)$" --output-on-failure`: PASS, 5/5 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --config Debug`: PASS.
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug -R "surface_state|surface_step|dpm_fields|hydrostatic_reconstruction|hllc_flux|phi_t_source|surface2d_golden_minimal|cfl_rollback|conservative_update|wetting_drying|boundary_flux|boundary_conditions|momentum_transport|pressure_momentum|hllc_wave_momentum|hllc_wave_mass" --output-on-failure`: PASS, 16/16 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure`: PASS, 20/20 tests passed.

## Coverage

- `test_hllc_flux`: vector momentum projection onto Cartesian axes and tangential momentum advection with HLLC mass flux.
- `test_momentum_transport`: step-level residuals consume HLLC vector momentum flux directly.
- `test_pressure_momentum`: pressure-related momentum regressions remain passing after replacing the old residual accumulation path.
- Existing mass, normal momentum, pressure, boundary, wetting/drying, conservative update, and golden-minimal tests remain passing.

## Boundaries

- `Surface2DCore` remains independent of SWMM, D-Flow FM, `CouplingLib`, and 1D engine ABIs.
- M11 remains limited to the CPU reference Surface2D edge-flux and momentum-residual path.
- M11 does not introduce CUDA backends, prescribed external boundary states, coupling ledger behavior, GoldenSuite release claims, or 1D engine interface changes.

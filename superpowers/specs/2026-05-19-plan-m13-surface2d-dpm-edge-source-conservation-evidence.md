# M13 Surface2D DPM Edge/Source Conservation Evidence

**Date:** 2026-05-19
**Design:** `superpowers/specs/2026-05-19-m13-surface2d-dpm-edge-source-conservation-design.md`
**Plan:** `superpowers/plans/2026-05-19-plan-m13-surface2d-dpm-edge-source-conservation.md`
**M12 Basis:** `superpowers/specs/2026-05-18-plan-m12-surface2d-edge-hllc-cfl-diagnostic-evidence.md`

## Scope

M13 adds focused CPU reference `Surface2DCore` regression coverage proving that isolated internal-edge HLLC transport diagnostics, DPM edge blocking fields, and `phi_t` source-pairing diagnostics participate coherently in one-step edge/source conservation checks.

## Local Validation

- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_dpm_edge_source_conservation --config Debug && build/windows-msvc/tests/unit/surface2d/Debug/test_dpm_edge_source_conservation.exe`: PASS, 3/3 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --config Debug`: PASS.
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug -R "surface_state|surface_step|dpm_fields|hydrostatic_reconstruction|hllc_flux|phi_t_source|surface2d_golden_minimal|cfl_rollback|dpm_edge_source_conservation|conservative_update|wetting_drying|boundary_flux|boundary_conditions|momentum_transport|pressure_momentum|hllc_wave_momentum|hllc_wave_mass" --output-on-failure`: PASS, 17/17 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure`: PASS, 21/21 tests passed.

## Coverage

- `test_dpm_edge_source_conservation`: active isolated internal edge has equal-and-opposite left/right mass residuals, equal-and-opposite left/right Cartesian momentum residual vectors, nonzero transport, and exact `pressure_pairing + s_phi_t == 0` closure.
- `test_dpm_edge_source_conservation`: `omega_edge == 0.0` hard block zeros edge mass and momentum transport diagnostics and leaves adjacent cell mass/momentum residuals at zero for the isolated edge.
- `test_dpm_edge_source_conservation`: `phi_e_n < PhiEdgeMin` soft block zeros edge mass and momentum transport diagnostics and leaves adjacent cell mass/momentum residuals at zero for the isolated edge.
- Existing `test_phi_t_source`, `test_pressure_momentum`, `test_surface_step`, `test_surface2d_golden_minimal`, DPM, HLLC, boundary, wetting/drying, conservative update, and CFL rollback regressions remain passing.

## Boundaries

- `Surface2DCore` remains independent of SWMM, D-Flow FM, `CouplingLib`, and 1D engine ABIs.
- M13 does not add new DPM field names or aliases; `phi_t`, `phi_e_n`, and `omega_edge` keep distinct semantics.
- M13 does not change M12 `max_cell_cfl` semantics.
- M13 does not add rollback, snapshot, replay, `mass_deficit_account`, global mass audit infrastructure, CUDA backends, adaptive timestep selection, coupling ledger behavior, GoldenSuite release claims, or 1D engine interfaces.

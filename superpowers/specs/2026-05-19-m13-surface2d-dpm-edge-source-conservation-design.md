# M13 Surface2D DPM Edge/Source Conservation Design

## Goal

M13 adds a minimal, testable Surface2D CPU reference slice proving that DPM edge fields and `phi_t` source-pairing diagnostics participate coherently in one-step edge/source conservation regressions.

## Scope

M13 stays inside `Surface2DCore` and uses the existing mixed mesh fixtures, HLLC edge flux path, `DpmFields`, `StepDiagnostics`, and CPU `advance_one_step_cpu(...)` overloads. It does not introduce rollback/snapshot behavior, adaptive timestep selection, CUDA backends, coupling ledger behavior, GoldenSuite release claims, or SWMM/D-Flow FM/1D engine interfaces.

## Architecture

The slice treats each internal edge as the smallest conservation unit. For an active internal edge, the step diagnostics should expose a coherent edge/source picture:

- HLLC mass flux is integrated into opposite-signed left/right cell mass residuals.
- HLLC Cartesian momentum flux is integrated into opposite-signed left/right cell momentum residuals.
- `phi_t` pressure pairing and centered `s_phi_t` source diagnostics cancel exactly for the edge-local hydrostatic pairing already implemented.
- DPM blocking through `omega_edge == 0` or `phi_e_n < PhiEdgeMin` zeros HLLC transport contributions and therefore leaves cell mass/momentum residuals unchanged for an isolated edge.

M13 should prefer diagnostics and regression coverage over adding new numerical machinery. If the existing implementation already satisfies a requirement, the implementation task should be limited to tests and evidence.

## Test Design

Create or extend focused unit coverage for DPM edge/source conservation, preferably under `tests/unit/surface2d/` as a dedicated `test_dpm_edge_source_conservation.cpp` target if CMake wiring is straightforward, or inside an existing Surface2D test file if that is the smaller change.

The minimum tests are:

1. Active internal edge conservation closure.
   - Use `build_mixed_minimal_mesh()` and isolate one internal edge by setting all DPM edge fields closed except the selected edge.
   - Give the left/right cells a nontrivial but stable state so mass and momentum fluxes are nonzero.
   - Set different left/right `phi_t` values.
   - Assert `pressure_pairing + s_phi_t == 0` for that edge.
   - Assert left/right mass residuals are equal and opposite.
   - Assert left/right momentum residual vectors are equal and opposite.

2. `omega_edge` hard block conservation closure.
   - Use the same isolated internal edge setup but set `omega_edge = 0.0`.
   - Assert edge mass and momentum flux diagnostics are zero.
   - Assert left/right mass and momentum residuals remain zero.

3. `phi_e_n` soft block conservation closure.
   - Use the same isolated internal edge setup but set `phi_e_n < PhiEdgeMin`.
   - Assert edge mass and momentum flux diagnostics are zero.
   - Assert left/right mass and momentum residuals remain zero.

4. Regression guard for existing pressure/source pairing.
   - Preserve current expectations from `test_phi_t_source`, `test_pressure_momentum`, `test_surface_step`, and `test_surface2d_golden_minimal`.

## Implementation Boundaries

- Do not add new DPM field names or aliases.
- Preserve `phi_t`, `phi_e_n`, and `omega_edge` as distinct semantics.
- Do not change `max_cell_cfl` semantics from M12.
- Do not add global mass audit infrastructure in M13.
- Do not add rollback, snapshot, replay, or `mass_deficit_account` behavior in M13.
- Do not add dependencies from `Surface2DCore` to SWMM, D-Flow FM, `CouplingLib`, or any 1D engine ABI.

## Validation

M13 validation should include:

- Focused DPM edge/source conservation test target.
- Existing Surface2D step, DPM fields, HLLC flux, pressure momentum, boundary, wetting/drying, conservative update, CFL rollback, and golden-minimal tests.
- Full Debug build.
- Full CTest.

Evidence should be recorded in `superpowers/specs/2026-05-19-plan-m13-surface2d-dpm-edge-source-conservation-evidence.md` after implementation.

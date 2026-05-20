# M16 CouplingLib Core Q_limit Evidence

**Date:** 2026-05-20
**Design:** `superpowers/specs/2026-05-20-m16-coupling-core-q-limit-design.md`
**Plan:** `superpowers/plans/2026-05-20-plan-m16-coupling-core-q-limit.md`

## Scope

M16 adds the canonical pure core `V_limit = 0.9 * phi_t * h * A` and `Q_limit = V_limit / dt_sub` calculation for exchange-cell hard-gate flow limits.

## Local Validation

- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_flow_limit --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_flow_limit.exe`: PASS, 4/4 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_core_state --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_core_state.exe`: PASS, 4/4 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure`: PASS, full CTest passed 24/24.

## Coverage

- `test_coupling_flow_limit`: computes `V_limit = 0.9 * phi_t * h * A` and `Q_limit = V_limit / dt_sub`.
- `test_coupling_flow_limit`: zero `phi_t`, `h`, or `area` produces zero limits.
- `test_coupling_flow_limit`: non-positive `dt_sub` is rejected.
- `test_coupling_flow_limit`: negative `phi_t`, `h`, or `area` is rejected.
- `test_coupling_core_state`: existing snapshot, rollback, and replay behavior remains intact after extending `ExchangeCellState`.

## Boundaries

- M16 does not connect to `Surface2DCore`, SWMM, D-Flow FM, or any 1D engine ABI.
- M16 does not introduce `Q_max_safe` or any alias for `Q_limit`.
- M16 does not implement `mass_deficit_account`, shared-cell arbitration, adaptive timestep selection, fault-state orchestration, or GoldenSuite release evidence.

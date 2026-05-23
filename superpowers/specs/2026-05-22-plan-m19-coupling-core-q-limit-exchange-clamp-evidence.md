# M19 CouplingLib Core Q_limit Exchange Clamp Evidence

**Date:** 2026-05-22
**Design:** `superpowers/specs/2026-05-22-m19-coupling-core-q-limit-exchange-clamp-design.md`
**Plan:** `superpowers/plans/2026-05-22-plan-m19-coupling-core-q-limit-exchange-clamp.md`

## Scope

M19 adds the pure `CouplingLib` core `evaluate_exchange(...)` decision that clamps a single exchange request against `Q_limit` and accrues unmet volume into the cell's `mass_deficit_account`.

## Local Validation

- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_exchange_decision --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_exchange_decision.exe`: PASS, 5/5 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_core_state --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_core_state.exe`: PASS, 7/7 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_mass_deficit_account --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_mass_deficit_account.exe`: PASS, 5/5 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_flow_limit --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_flow_limit.exe`: PASS, 4/4 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure`: PASS, full CTest passed 26/26.

## Coverage

- `test_coupling_exchange_decision`: request below `Q_limit` with no deficit grants the full request.
- `test_coupling_exchange_decision`: request above `Q_limit` with no deficit clamps to `Q_limit` and accrues unmet volume.
- `test_coupling_exchange_decision`: deficit below the hard gate is fully repaid alongside the new request.
- `test_coupling_exchange_decision`: deficit at or above the hard gate consumes the full `Q_limit` and blocks the new request.
- `test_coupling_exchange_decision`: rejects negative request volume and negative deficit volume.
- `test_coupling_core_state`, `test_coupling_mass_deficit_account`, `test_coupling_flow_limit`: existing behavior remains intact.

## Boundaries

- M19 does not introduce arbitration, `priority_weight`, or multi-engine shared-cell splitting.
- M19 does not introduce `drain_split`, `epsilon_deficit`, scheduler, fault, or write-off logic.
- M19 does not connect `evaluate_exchange(...)` to `Surface2DCore`, SWMM, D-Flow FM, or any 1D engine ABI.
- M19 does not mutate `CouplingState`; downstream callers must feed the decision through `CouplingEvent` and the existing snapshot/rollback/replay path.

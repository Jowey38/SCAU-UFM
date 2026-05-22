# M18 CouplingLib Core Deficit Snapshot Replay Evidence

**Date:** 2026-05-22
**Design:** `superpowers/specs/2026-05-22-m18-coupling-core-deficit-snapshot-replay-design.md`
**Plan:** `superpowers/plans/2026-05-22-plan-m18-coupling-core-deficit-snapshot-replay.md`

## Scope

M18 integrates `mass_deficit_account` into the pure `CouplingState` snapshot, rollback, and replay path.

## Local Validation

- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_core_state --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_core_state.exe`: PASS, 7/7 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_mass_deficit_account --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_mass_deficit_account.exe`: PASS, 5/5 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_flow_limit --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_flow_limit.exe`: PASS, 4/4 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure`: PASS, full CTest passed 25/25.

## Coverage

- `test_coupling_core_state`: snapshots capture deficit account values and remain immutable after replay.
- `test_coupling_core_state`: rollback restores deficit account values from the snapshot.
- `test_coupling_core_state`: rollback preserves pending deficit events for replay.
- `test_coupling_core_state`: existing snapshot/rollback/replay behavior for cell volume remains intact.
- `test_coupling_mass_deficit_account`: existing roll-forward and repayment semantics remain intact.
- `test_coupling_flow_limit`: existing `V_limit` / `Q_limit` behavior remains intact.

## Boundaries

- M18 does not connect to `Surface2DCore`, SWMM, D-Flow FM, or any 1D engine ABI.
- M18 does not add shared-cell arbitration, `priority_weight`, scheduler behavior, fault orchestration, or write-off logic.
- M18 does not redefine `Q_limit`, `V_limit`, or `epsilon_deficit` semantics.

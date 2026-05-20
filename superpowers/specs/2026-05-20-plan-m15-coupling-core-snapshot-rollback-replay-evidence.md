# M15 CouplingLib Core Snapshot/Rollback/Replay Evidence

**Date:** 2026-05-20
**Design:** `superpowers/specs/2026-05-20-m15-coupling-core-snapshot-rollback-replay-design.md`
**Plan:** `superpowers/plans/2026-05-20-plan-m15-coupling-core-snapshot-rollback-replay.md`

## Scope

M15 adds a minimal pure `CouplingLib` core state container with snapshot, rollback, and replay semantics for exchange-cell volume events.

## Local Validation

- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_core_state --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_core_state.exe`: PASS, 4/4 tests passed.
- `build/windows-msvc/tests/unit/surface2d/Debug/test_step_rollback_rejection.exe && PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure`: PASS, rollback executable rerun passed 2/2 and full CTest passed 23/23.

## Coverage

- `test_coupling_core_state`: snapshots capture exchange-cell volumes and remain immutable after later replayed events.
- `test_coupling_core_state`: rollback restores exchange-cell volumes from a saved snapshot.
- `test_coupling_core_state`: rollback preserves pending events and replay reapplies them in enqueue order.
- `test_coupling_core_state`: invalid exchange-cell event targets are rejected at enqueue time.

## Boundaries

- M15 does not connect to `Surface2DCore`, SWMM, D-Flow FM, or any 1D engine ABI.
- M15 does not implement `Q_limit`, `V_limit`, `mass_deficit_account`, shared-cell arbitration, adaptive timestep selection, fault-state orchestration, or GoldenSuite release evidence.

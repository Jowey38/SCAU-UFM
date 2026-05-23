# M20 CouplingLib Core Drain Split Evidence

**Date:** 2026-05-23
**Design:** `superpowers/specs/2026-05-23-m20-coupling-core-drain-split-design.md`
**Plan:** `superpowers/plans/2026-05-23-plan-m20-coupling-core-drain-split.md`

## Scope

M20 adds the pure `CouplingLib` core `split_drain(...)` decision that splits a single arbitrated exchange decision into micro-steps following spec §6.1 rule 4 (20% smoothing rule), with the micro-step count capped at 5.

## Local Validation

- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_drain_split --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_drain_split.exe`: PASS, 6/6 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_exchange_decision --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_exchange_decision.exe`: PASS, 5/5 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_core_state --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_core_state.exe`: PASS, 7/7 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_mass_deficit_account --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_mass_deficit_account.exe`: PASS, 5/5 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_flow_limit --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_flow_limit.exe`: PASS, 4/4 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure`: PASS, full CTest passed 27/27.

## Coverage

- `test_coupling_drain_split`: granted volume well below the threshold stays a single micro-step.
- `test_coupling_drain_split`: granted volume exactly at the threshold stays a single micro-step.
- `test_coupling_drain_split`: granted volume slightly above the threshold splits into two equal micro-steps.
- `test_coupling_drain_split`: granted volume requiring more than five micro-steps is capped at five.
- `test_coupling_drain_split`: zero granted volume reports a single trivial micro-step.
- `test_coupling_drain_split`: rejects non-positive `dt_sub`, negative geometry, negative `v_granted`, and zero storage with positive `v_granted`.
- `test_coupling_exchange_decision`, `test_coupling_core_state`, `test_coupling_mass_deficit_account`, `test_coupling_flow_limit`: existing behavior remains intact.

## Boundaries

- M20 does not introduce `count_drain_split` runtime counter or any counter plumbing.
- M20 does not introduce arbitration, `priority_weight`, or multi-engine shared-cell splitting.
- M20 does not introduce `epsilon_deficit`, scheduler, fault, or write-off logic.
- M20 does not connect `split_drain(...)` to `Surface2DCore`, SWMM, D-Flow FM, or any 1D engine ABI.
- M20 does not mutate `CouplingState`; downstream callers must still feed micro-step decisions through `CouplingEvent` and the existing snapshot/rollback/replay path.

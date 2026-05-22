# M17 CouplingLib Core Mass Deficit Account Evidence

**Date:** 2026-05-21
**Design:** `superpowers/specs/2026-05-21-m17-coupling-core-mass-deficit-account-design.md`
**Plan:** `superpowers/plans/2026-05-21-plan-m17-coupling-core-mass-deficit-account.md`

## Scope

M17 adds the minimal pure core `mass_deficit_account` ledger semantics for rolling unmet exchange volume forward and applying repayment volume.

## Local Validation

- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_mass_deficit_account --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_mass_deficit_account.exe`: PASS, 5/5 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_core_state --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_core_state.exe`: PASS, 4/4 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_flow_limit --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_flow_limit.exe`: PASS, 4/4 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure`: PASS, full CTest passed 25/25.

## Coverage

- `test_coupling_mass_deficit_account`: rolls a zero deficit forward with unmet volume.
- `test_coupling_mass_deficit_account`: accumulates multiple unmet volume increments.
- `test_coupling_mass_deficit_account`: repays part of an existing deficit.
- `test_coupling_mass_deficit_account`: clamps over-repayment at zero deficit.
- `test_coupling_mass_deficit_account`: rejects negative unmet or applied volume.
- `test_coupling_core_state`: existing snapshot, rollback, and replay behavior remains intact.
- `test_coupling_flow_limit`: existing `V_limit` / `Q_limit` behavior remains intact.

## Boundaries

- M17 does not connect to `Surface2DCore`, SWMM, D-Flow FM, or any 1D engine ABI.
- M17 does not add shared-cell arbitration, `priority_weight`, scheduler behavior, fault orchestration, or write-off logic.
- M17 does not redefine `Q_limit`, `V_limit`, or `epsilon_deficit` semantics.

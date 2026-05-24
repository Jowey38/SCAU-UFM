# M21 CouplingLib Core Nonnegative Storage Backoff Evidence

**Date:** 2026-05-24
**Design:** `superpowers/specs/2026-05-24-m21-coupling-core-nonnegative-storage-backoff-design.md`
**Plan:** `superpowers/plans/2026-05-24-plan-m21-coupling-core-nonnegative-storage-backoff.md`

## Scope

M21 adds the pure `CouplingLib` core `enforce_nonnegative_storage(...)` decision that clamps a single donor exchange decision so `v_granted` cannot exceed donor-side physical storage `phi_t * h * A`.

## Local Validation

- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_nonnegative_storage_backoff --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_nonnegative_storage_backoff.exe`: PASS, 5/5 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_drain_split --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_drain_split.exe`: PASS, 6/6 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_exchange_decision --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_exchange_decision.exe`: PASS, 5/5 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_core_state --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_core_state.exe`: PASS, 7/7 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_mass_deficit_account --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_mass_deficit_account.exe`: PASS, 5/5 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_flow_limit --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_flow_limit.exe`: PASS, 4/4 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure`: PASS, full CTest passed 28/28.

## Coverage

- `test_coupling_nonnegative_storage_backoff`: leaves decisions below available storage unchanged.
- `test_coupling_nonnegative_storage_backoff`: leaves decisions exactly at available storage unchanged.
- `test_coupling_nonnegative_storage_backoff`: clamps grants above available storage and accrues additional unmet volume.
- `test_coupling_nonnegative_storage_backoff`: zero available storage moves all granted volume to unmet.
- `test_coupling_nonnegative_storage_backoff`: rejects invalid `dt_sub`, negative geometry, and negative decision fields.
- `test_coupling_drain_split`, `test_coupling_exchange_decision`, `test_coupling_core_state`, `test_coupling_mass_deficit_account`, `test_coupling_flow_limit`: existing behavior remains intact.

## Boundaries

- M21 does not implement the multi-donor linear programming form of spec §6.1 rule 5.
- M21 does not introduce arbitration, `priority_weight`, shared-cell splitting, or `V_limit_k`.
- M21 does not introduce `count_negative_depth_fix` or any runtime counter plumbing.
- M21 does not introduce `epsilon_deficit`, scheduler, fault, or write-off logic.
- M21 does not connect `enforce_nonnegative_storage(...)` to `Surface2DCore`, SWMM, D-Flow FM, or any 1D engine ABI.
- M21 does not mutate `CouplingState`.

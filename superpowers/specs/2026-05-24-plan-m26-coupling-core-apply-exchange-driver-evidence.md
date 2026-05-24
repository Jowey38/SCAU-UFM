# M26 CouplingLib Core Apply Exchange Single Donor Driver Evidence

**Date:** 2026-05-24
**Design:** `superpowers/specs/2026-05-24-m26-coupling-core-apply-exchange-driver-design.md`
**Plan:** `superpowers/plans/2026-05-24-plan-m26-coupling-core-apply-exchange-driver.md`

## Scope

M26 adds `CouplingState::apply_exchange(cell_index, request)` that atomically composes pipeline evaluation + event enqueue + counter record into a single entry point for adapter callers.

## Local Validation

- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_apply_exchange --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_apply_exchange.exe`: PASS, 5/5 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_core_state --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_core_state.exe`: PASS, 18/18 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_exchange_pipeline --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_exchange_pipeline.exe`: PASS, 6/6 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_nonnegative_storage_backoff --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_nonnegative_storage_backoff.exe`: PASS, 5/5 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_drain_split --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_drain_split.exe`: PASS, 6/6 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_exchange_decision --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_exchange_decision.exe`: PASS, 5/5 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_mass_deficit_account --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_mass_deficit_account.exe`: PASS, 5/5 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_flow_limit --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_flow_limit.exe`: PASS, 4/4 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure`: PASS, full CTest passed 30/30 (10.14s).

## Coverage

- `test_coupling_apply_exchange`: returned decision matches `evaluate_exchange_pipeline(...)` for the same cell state and request.
- `test_coupling_apply_exchange`: after apply, `cells()[i].volume` is unchanged (deferred to replay); after `replay_pending()`, volume changes by `v_granted`.
- `test_coupling_apply_exchange`: after apply, runtime counters are updated according to M23 engagement flags.
- `test_coupling_apply_exchange`: after apply + replay, `mass_deficit_account` is updated by `v_unmet` and `v_repay`.
- `test_coupling_apply_exchange`: out-of-range `cell_index` throws `std::out_of_range`.

## Boundaries

- `apply_exchange(...)` does not call `replay_pending()`.
- M26 does not introduce multi-donor arbitration, `priority_weight`, `V_limit_k`, or vector-form DTOs.
- M26 does not introduce FaultController, scheduler, or mapping logic.
- M26 does not connect to SWMM, D-Flow FM, `Surface2DCore`, or any 1D engine ABI.
- M26 does not introduce snapshot persistence or IO.

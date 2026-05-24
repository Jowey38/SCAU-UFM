# M24 CouplingLib Core Runtime Counters Snapshot Evidence

**Date:** 2026-05-24
**Design:** `superpowers/specs/2026-05-24-m24-coupling-core-runtime-counters-snapshot-design.md`
**Plan:** `superpowers/plans/2026-05-24-plan-m24-coupling-core-runtime-counters-snapshot.md`

## Scope

M24 lifts the M23 per-invocation engagement flags into a `RuntimeCounters` aggregate owned by `CouplingState`, brings the counters into `snapshot` / `rollback` coverage, and locks the invariant that `replay_pending()` does not mutate counters.

## Local Validation

- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_core_state --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_core_state.exe`: PASS, 15/15 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_exchange_pipeline --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_exchange_pipeline.exe`: PASS, 6/6 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_nonnegative_storage_backoff --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_nonnegative_storage_backoff.exe`: PASS, 5/5 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_drain_split --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_drain_split.exe`: PASS, 6/6 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_exchange_decision --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_exchange_decision.exe`: PASS, 5/5 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_mass_deficit_account --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_mass_deficit_account.exe`: PASS, 5/5 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_flow_limit --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_flow_limit.exe`: PASS, 4/4 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure`: PASS, full CTest passed 29/29 (6.15s).

## Coverage

- `test_coupling_core_state`: fresh state has both counters at 0.
- `test_coupling_core_state`: `record_pipeline_decision(...)` only increments `count_drain_split` when the M23 flag is true.
- `test_coupling_core_state`: `record_pipeline_decision(...)` only increments `count_negative_depth_fix` when the M23 flag is true.
- `test_coupling_core_state`: both flags true increment both counters; repeated calls accumulate.
- `test_coupling_core_state`: both flags false leave counters unchanged.
- `test_coupling_core_state`: snapshot captures counter values at snapshot time and remains immutable across subsequent state mutations.
- `test_coupling_core_state`: rollback restores counter values from snapshot.
- `test_coupling_core_state`: `replay_pending()` updates only cells and the deficit ledger, not counters.

## §6.5 / §8.3 Alignment

- `RuntimeCounters` are integer accumulators; the §8.3 strict zero-diff replay invariant is satisfied trivially whenever the same sequence of `record_pipeline_decision(...)` calls is applied with the same flag values.
- §6.5 snapshot coverage on `RuntimeCounters` is satisfied in-memory; persistent schema serialization is out of scope for M24.

## Boundaries

- M24 does not introduce a stateful `apply_exchange(...)` driver; callers still invoke `evaluate_exchange_pipeline(...)` and pass the result to `record_pipeline_decision(...)` explicitly.
- M24 does not introduce additional §14.1 counters (`count_velocity_clamp`, `count_flux_cutoff`, `count_drag_matrix_degenerate`, `count_ai_prior_conflict`, `count_mem_pressure_events`, `max_cell_cfl`).
- M24 does not introduce multi-donor arbitration, `priority_weight`, `V_limit_k`, or DTO refactor.
- M24 does not introduce snapshot persistence, schema serialization, or IO.
- M24 does not connect to `Surface2DCore`, SWMM, D-Flow FM, or any 1D engine ABI.

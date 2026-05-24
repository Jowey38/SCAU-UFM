# M23 CouplingLib Core Pipeline Diagnostic Flags Evidence

**Date:** 2026-05-24
**Design:** `superpowers/specs/2026-05-24-m23-coupling-core-pipeline-diagnostic-flags-design.md`
**Plan:** `superpowers/plans/2026-05-24-plan-m23-coupling-core-pipeline-diagnostic-flags.md`

## Scope

M23 extends `ExchangePipelineDecision` with two diagnostic booleans:

- `drain_split_engaged` — whether M20 returned `micro_steps > 1`.
- `negative_depth_fix_engaged` — whether M21 actually lowered the post-M19 `v_granted` (strict `<`).

The flags are pure observations on the M22 composition; no state, no new physics, no DTO refactor.

## Local Validation

- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_exchange_pipeline --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_exchange_pipeline.exe`: PASS, 6/6 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_nonnegative_storage_backoff --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_nonnegative_storage_backoff.exe`: PASS, 5/5 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_drain_split --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_drain_split.exe`: PASS, 6/6 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_exchange_decision --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_exchange_decision.exe`: PASS, 5/5 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_core_state --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_core_state.exe`: PASS, 7/7 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_mass_deficit_account --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_mass_deficit_account.exe`: PASS, 5/5 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_flow_limit --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_flow_limit.exe`: PASS, 4/4 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure`: PASS, full CTest passed 29/29 (8.06s).

## Coverage

- `test_coupling_exchange_pipeline`: safe request (`v_granted = 16.0 > 0.2·phi_t·h·A = 8.0`) engages `drain_split_engaged`; `negative_depth_fix_engaged` stays off.
- `test_coupling_exchange_pipeline`: request above `Q_limit` engages `drain_split_engaged`; `negative_depth_fix_engaged` stays off (M19 clamp keeps `v_granted = V_limit < phi_t·h·A`).
- `test_coupling_exchange_pipeline`: nonnegative-storage scenario from M22 keeps `negative_depth_fix_engaged` off — turning the M22 evidence observation into a machine-asserted invariant under canonical `V_limit = 0.9·phi_t·h·A`.
- `test_coupling_exchange_pipeline`: deficit repayment scenario engages `drain_split_engaged`; `negative_depth_fix_engaged` stays off.
- `test_coupling_exchange_pipeline`: new `EngagementFlagsRemainOffForSafeRequestAtSplitThreshold` case asserts both flags `false` when `v_granted = 8.0` exactly at the M20 split threshold (single micro-step).
- `test_coupling_exchange_pipeline`: invalid inputs are still rejected through composed helpers.
- M16-M22 focused tests remain intact.

## §14.1 Counter Surface

`drain_split_engaged` and `negative_depth_fix_engaged` are the per-invocation observations required to populate the §14.1 runtime counters `count_drain_split` and `count_negative_depth_fix`. M23 wires the observation primitives only; aggregation into stateful counters and inclusion in snapshot / rollback / replay coverage are reserved for M24.

## Boundaries

- M23 does not introduce stateful counters or modify `CouplingState`.
- M23 does not change snapshot / rollback / replay coverage; those updates are reserved for M24.
- M23 does not introduce multi-donor arbitration, `priority_weight`, `V_limit_k`, or any new DTO.
- M23 does not change `evaluate_exchange(...)`, `enforce_nonnegative_storage(...)`, or `split_drain(...)`.
- M23 does not connect the pipeline to `Surface2DCore`, SWMM, D-Flow FM, or any 1D engine ABI.

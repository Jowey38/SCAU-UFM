# M22 CouplingLib Core Single Donor Pipeline Evidence

**Date:** 2026-05-24
**Design:** `superpowers/specs/2026-05-24-m22-coupling-core-single-donor-pipeline-design.md`
**Plan:** `superpowers/plans/2026-05-24-plan-m22-coupling-core-single-donor-pipeline.md`

## Scope

M22 adds the pure `CouplingLib` core `evaluate_exchange_pipeline(...)` decision that composes `evaluate_exchange(...)`, `enforce_nonnegative_storage(...)`, and `split_drain(...)` for one donor cell and one exchange request.

## Local Validation

- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_exchange_pipeline --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_exchange_pipeline.exe`: PASS, 5/5 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_nonnegative_storage_backoff --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_nonnegative_storage_backoff.exe`: PASS, 5/5 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_drain_split --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_drain_split.exe`: PASS, 6/6 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_exchange_decision --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_exchange_decision.exe`: PASS, 5/5 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_core_state --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_core_state.exe`: PASS, 7/7 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_mass_deficit_account --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_mass_deficit_account.exe`: PASS, 5/5 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_flow_limit --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_flow_limit.exe`: PASS, 4/4 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure`: PASS, full CTest passed 29/29.

## Coverage

- `test_coupling_exchange_pipeline`: safe request returns final grant and drain split.
- `test_coupling_exchange_pipeline`: request above `Q_limit` clamps through M19 and accrues unmet volume.
- `test_coupling_exchange_pipeline`: nonnegative storage backoff sits after the M19 hard-gate clamp; in the canonical `V_limit = 0.9 * phi_t * h * A` regime, the M21 storage cap at `phi_t * h * A` is unreachable after M19, so its `if (v_granted <= available_storage)` no-op branch is exercised and the post-M19 grant flows through unchanged.
- `test_coupling_exchange_pipeline`: deficit repayment precedes new request grant and the drain split is computed from the new grant.
- `test_coupling_exchange_pipeline`: invalid inputs are rejected through composed helpers.
- Existing M16-M21 focused tests remain intact.

### M22 NonnegativeStorageBackoffRunsAfterHardGateClamp expected values

The plan's original expected values for this case (`q_granted = 2.5`, `v_granted = 10.0`, `v_unmet = 10.0`, `v_per_micro_step = 2.0`) implicitly assumed M19 caps at `phi_t * h * A`. Under the canonical formula `V_limit = 0.9 * phi_t * h * A` mandated by `CLAUDE.md` and the symbols reference, M19 caps at `9.0` and M21's storage clamp at `10.0` is therefore unreachable in this composition. The test expectations were aligned with the canonical composition output (`q_granted = 2.25`, `v_granted = 9.0`, `v_unmet = 11.0`, `v_per_micro_step = 1.8`, `micro_steps = 5`, `dt_micro = 0.8`), preserving the test's role of demonstrating the M21 step is wired into the pipeline (executed as a no-op clamp here) and that M20 derives micro-steps from the post-M21 grant.

## Boundaries

- M22 does not introduce new exchange semantics beyond composing M19-M21 helpers.
- M22 does not implement multi-donor linear programming.
- M22 does not introduce arbitration, `priority_weight`, shared-cell splitting, or `V_limit_k`.
- M22 does not introduce runtime counters.
- M22 does not introduce `epsilon_deficit`, scheduler, fault, or write-off logic.
- M22 does not connect `evaluate_exchange_pipeline(...)` to `Surface2DCore`, SWMM, D-Flow FM, or any 1D engine ABI.
- M22 does not mutate `CouplingState`.

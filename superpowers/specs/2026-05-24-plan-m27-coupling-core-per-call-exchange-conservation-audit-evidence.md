# M27 CouplingLib Core Per-Call Exchange Conservation Audit Evidence

**Date:** 2026-05-24
**Design:** `superpowers/specs/2026-05-24-m27-coupling-core-per-call-exchange-conservation-audit-design.md`
**Plan:** `superpowers/plans/2026-05-24-plan-m27-coupling-core-per-call-exchange-conservation-audit.md`

## Scope

M27 lifts the implicit §6.3 deficit-ledger volume identity `v_granted + v_unmet == q_request * dt_sub` into a pure machine-asserted audit helper with strict zero residual on the correctness path.

## Local Validation

- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_exchange_audit --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_exchange_audit.exe`: PASS, 6/6 tests passed.
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure`: PASS, full CTest passed 31/31 (9.32s).

## Coverage

- `test_coupling_exchange_audit.SafeRequestIsBalancedWithZeroResidual`: full grant case (`request=4.0, dt_sub=4.0`): `request_volume = 16.0, accounted = 16.0, residual = 0.0, balanced = true`.
- `test_coupling_exchange_audit.HardGateClampScenarioIsBalanced`: `Q_limit` clamp case (`request=20.0, dt_sub=4.0`): `request_volume = 80.0, accounted = 36.0 + 44.0 = 80.0, residual = 0.0, balanced = true`.
- `test_coupling_exchange_audit.DeficitRepaymentScenarioIsBalanced`: deficit repayment does not affect request-side balance (`deficit=8.0, request=5.0, dt_sub=4.0`): `request_volume = 20.0, accounted = 20.0, residual = 0.0, balanced = true`.
- `test_coupling_exchange_audit.NonnegativeStorageBackoffScenarioIsBalanced`: M22 evidence scenario (`phi_t=0.2, h=1.0, area=50.0, request=5.0, dt_sub=4.0`): `request_volume = 20.0, accounted = 9.0 + 11.0 = 20.0, residual = 0.0, balanced = true`.
- `test_coupling_exchange_audit.ManuallyConstructedImbalancedDecisionFlagsResidual`: hand-crafted imbalanced decision (`v_granted=4.0, v_unmet=8.0, request=4.0, dt_sub=4.0`): `request_volume = 16.0, accounted = 12.0, residual = 4.0, balanced = false`.
- `test_coupling_exchange_audit.RejectsNegativeRequestOrDecisionVolumes`: each of `q_request < 0`, `dt_sub <= 0`, `v_granted < 0`, `v_unmet < 0` throws `std::invalid_argument`.

## §6.3 / §8.3 Alignment

- §6.3 deficit-ledger identity `Q_target - Q_applied == V_unmet / dt_sub` is captured directly as `v_granted + v_unmet == q_request * dt_sub`.
- §8.3 correctness path uses strict `EXPECT_DOUBLE_EQ` equality; no tolerance. §11.4's `epsilon_mass` performance-path tolerance is explicitly out of scope.

## Boundaries

- M27 is a pure helper; no state mutation, no IO, no schema.
- M27 does not introduce system-wide `M_ref` accumulation.
- M27 does not introduce multi-donor arbitration, `priority_weight`, `V_limit_k`, or DTO refactor.
- M27 does not audit the repayment-side identity (out of §6.3 request-side scope).
- M27 does not connect to `Surface2DCore`, SWMM, D-Flow FM, or any 1D engine ABI.

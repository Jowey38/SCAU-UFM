# M130 Passive Action Outcome DTO Boundary Evidence

## Scope

M130 implements the next safest step after M129 design-only executable action outcome requirements: a passive action-outcome DTO/helper boundary.

The current passive chain can now emit an outcome record without scheduler control:

```text
EngineReport
→ classify_engine_health(...)
→ aggregate_engine_health(...)
→ make_fault_controller_diagnostic(...)
→ propose_fault_controller_action(...)
→ observe_mock_scheduler_fault_action(...)
→ consume_mock_scheduler_fault_action(...)
→ classify_fault_controller_passive_state(...)
→ make_fault_controller_passive_transition(...)
→ make_fault_controller_passive_action_audit_record(...)
→ make_fault_controller_passive_action_outcome(...)
```

This stage records passive outcome metadata only. It does not execute isolation, reconnect, abort, scheduler process control, release-gate behavior, or real adapter behavior.

## Implemented Core Boundary

`CouplingLib core` owns:

- `FaultControllerPassiveActionOutcomeKind`
- `FaultControllerPassiveActionOutcomeReason`
- `FaultControllerPassiveActionOutcome`
- `make_fault_controller_passive_action_outcome(...)`

Outcome behavior:

- preserves the input `FaultControllerPassiveActionAuditRecord`;
- maps no-action audit records to `outcome_kind = not_requested`;
- maps operator-review audit records to `outcome_kind = operator_review_required`;
- maps no-action audit records to `reason = nominal_no_action`;
- maps operator-review audit records to `reason = review_required_only`;
- sets `operator_review_required = true` only for operator-review audit records;
- always keeps non-execution fields false:
  - `scheduler_control_available = false`
  - `scheduler_control_used = false`
  - `adapter_boundary_available = false`
  - `adapter_call_attempted = false`
  - `adapter_call_succeeded = false`
  - `adapter_call_failed = false`
  - `runtime_action_requested = false`
  - `runtime_action_attempted = false`
  - `runtime_action_executed = false`
  - `runtime_action_skipped = false`
  - `runtime_action_failed = false`
  - `rollback_required = false`
  - `replay_required = false`
  - `mass_audit_required = false`
  - `release_gate_action_executed = false`

## Tests

Focused tests were added under:

- `tests/unit/coupling/test_coupling_fault_controller_passive_action_outcome.cpp`

Registered target:

- `test_coupling_fault_controller_passive_action_outcome`

Asserted invariants:

- nominal audits produce not-requested outcome records;
- review-required audits produce operator-review-required outcome records;
- outcome records preserve the audit record;
- outcome creation does not promote audit execution flags into scheduler control, adapter boundary availability, adapter calls, runtime action requests, runtime attempts, runtime execution, rollback, replay, mass-audit requirement, or release-gate execution.

## Confirmed Negative Boundary

M130 does not add:

- executable FaultController state machine;
- full transition-table evaluator;
- threshold counters;
- timeout, recovery, or persistence thresholds;
- runtime isolation execution;
- runtime reconnect execution;
- runtime abort execution;
- scheduler process control;
- scheduler lifecycle control;
- skipped exchange scheduling;
- reordered request/arbitration/replay/audit behavior;
- mutation of coupling state from passive outcome records;
- native SWMM process, handle, or C API calls;
- native D-Flow FM / BMI process, handle, or API calls;
- protocol release-gate actions such as `BLOCK_MERGE` or `BLOCK_RELEASE`;
- runtime evidence writing;
- adapter-owned fault policy.

The outcome record is still a passive DTO seam only. It is not executable scheduler control or runtime evidence writing.

## Local Evidence

Focused build and CTest:

```text
cmake --build --preset windows-msvc --target test_coupling_fault_controller_passive_action_outcome
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_fault_controller_passive_action_outcome$"
```

Result:

```text
Test project H:/githubcode/SCAU-UFM/build/windows-msvc
    Start 42: test_coupling_fault_controller_passive_action_outcome
1/1 Test #42: test_coupling_fault_controller_passive_action_outcome ...   Passed    0.36 sec

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   0.52 sec
```

## CI Evidence

Pending. Record the push commit and GitHub Actions run after the M130 implementation/evidence changes are pushed.

## Remaining Gaps Before Executable FaultController Work

Before moving from passive outcome records into executable FaultController or scheduler behavior, remaining gaps include:

1. Add ordering evidence that passive outcome creation before mock scheduling does not alter scheduling, replay, or audit behavior.
2. Define and implement a full transition-table evaluator only after thresholds and audit fields are specified.
3. Define threshold and temporal policy for transient versus persistent faults.
4. Define separate execution DTOs for isolation, reconnect, abort, and review-required outcomes.
5. Define scheduler ordering evidence for any future executable action before it controls runtime behavior.
6. Keep real adapter health integration separate until mock-only passive outcome boundaries remain stable.
7. Add CI evidence and avoid GoldenSuite release/merge claims until required gate evidence exists.

## Decision

M130 establishes a passive action-outcome DTO/helper boundary and proves it does not enable scheduler control, runtime action requests, runtime execution, adapter calls, rollback, replay, mass-audit requirements, release gates, isolation, reconnect, or abort behavior. The next safe step is ordering evidence that passive outcome creation before mock scheduling still leaves scheduling, replay, and audit unchanged.

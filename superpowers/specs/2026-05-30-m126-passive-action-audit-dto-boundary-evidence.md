# M126 Passive Action Audit DTO Boundary Evidence

## Scope

M126 implements the next safest step after M125 design-only executable action audit schema requirements: a passive action-audit DTO/helper boundary.

The current passive chain can now emit an audit record without scheduler control:

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
```

This stage records passive audit metadata only. It does not execute isolation, reconnect, abort, scheduler process control, release-gate behavior, or real adapter behavior.

## Implemented Core Boundary

`CouplingLib core` owns:

- `FaultControllerPassiveActionAuditKind`
- `FaultControllerPassiveActionAuditStage`
- `FaultControllerPassiveActionAuditReason`
- `FaultControllerPassiveActionAuditRecord`
- `make_fault_controller_passive_action_audit_record(...)`

Audit behavior:

- preserves the input `FaultControllerPassiveTransition`;
- maps `running` transition records to `action_kind = none`;
- maps `review_required` transition records to `action_kind = operator_review`;
- sets `action_stage = transition_recorded`;
- maps running transitions to `reason = nominal_no_action`;
- maps review-required transitions to `reason = review_required_only`;
- always keeps non-execution fields false:
  - `scheduler_control_enabled = false`
  - `runtime_action_requested = false`
  - `runtime_action_executed = false`
  - `isolation_executed = false`
  - `reconnect_executed = false`
  - `abort_executed = false`
  - `adapter_call_executed = false`
  - `release_gate_action_executed = false`

## Tests

Focused tests were added under:

- `tests/unit/coupling/test_coupling_fault_controller_passive_action_audit.cpp`

Registered target:

- `test_coupling_fault_controller_passive_action_audit`

Asserted invariants:

- nominal transitions produce no-action audit records;
- review-required transitions produce operator-review audit records;
- audit records preserve the transition record;
- audit creation does not promote transition control flags into scheduler control, runtime action requests, runtime execution, isolation, reconnect, abort, adapter calls, or release-gate actions.

## Confirmed Negative Boundary

M126 does not add:

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
- mutation of coupling state from passive audit records;
- native SWMM process, handle, or C API calls;
- native D-Flow FM / BMI process, handle, or API calls;
- protocol release-gate actions such as `BLOCK_MERGE` or `BLOCK_RELEASE`;
- runtime evidence writing;
- adapter-owned fault policy.

The audit record is still a passive DTO seam only. It is not executable scheduler control or runtime evidence writing.

## Local Evidence

Focused build and CTest:

```text
cmake --build --preset windows-msvc --target test_coupling_fault_controller_passive_action_audit
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_fault_controller_passive_action_audit$"
```

Result:

```text
Test project H:/githubcode/SCAU-UFM/build/windows-msvc
    Start 40: test_coupling_fault_controller_passive_action_audit
1/1 Test #40: test_coupling_fault_controller_passive_action_audit ...   Passed    0.35 sec

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   0.59 sec
```

## CI Evidence

Pending. Record the push commit and GitHub Actions run after the M126 implementation/evidence changes are pushed.

## Remaining Gaps Before Executable FaultController Work

Before moving from passive audit records into executable FaultController or scheduler behavior, remaining gaps include:

1. Add ordering evidence that passive audit creation before mock scheduling does not alter scheduling, replay, or audit behavior.
2. Define and implement a full transition-table evaluator only after thresholds and audit fields are specified.
3. Define threshold and temporal policy for transient versus persistent faults.
4. Define separate execution DTOs for isolation, reconnect, abort, and review-required outcomes.
5. Define scheduler ordering evidence for any future executable action before it controls runtime behavior.
6. Keep real adapter health integration separate until mock-only passive audit boundaries remain stable.
7. Add CI evidence and avoid GoldenSuite release/merge claims until required gate evidence exists.

## Decision

M126 establishes a passive action-audit DTO/helper boundary and proves it does not enable scheduler control, runtime action requests, adapter calls, release gates, isolation, reconnect, or abort behavior. The next safe step is ordering evidence that passive audit creation before mock scheduling still leaves scheduling, replay, and audit unchanged.

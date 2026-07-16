# M128 Passive Action Audit Stage Rollup

## Scope

M128 rolls up the passive action-audit design and evidence completed in M125 through M127.

The current passive mock scheduler fault chain is:

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
→ run_mock_coupling_scheduler_step(...)
```

M128 is a rollup only. It does not add new production behavior, scheduler control, runtime isolation, reconnect, abort, real adapter integration, CI evidence, GoldenSuite evidence, or release/merge readiness claims.

## Covered Milestones

- M125: documented design-only requirements for a future executable action audit schema.
- M126: implemented passive action-audit DTO/helper records through `FaultControllerPassiveActionAuditKind`, `FaultControllerPassiveActionAuditStage`, `FaultControllerPassiveActionAuditReason`, `FaultControllerPassiveActionAuditRecord`, and `make_fault_controller_passive_action_audit_record(...)`.
- M127: added ordering evidence that passive action-audit record creation before mock scheduling does not change scheduling, replay, or audit results.

## Current Passive Action-Audit Boundary

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

## Ordering Evidence Summary

M127 verifies a control path against an audited path:

1. Control path:
   - starts from a coupling state snapshot;
   - runs `run_mock_coupling_scheduler_step(...)` directly.

2. Audited path:
   - starts from an equivalent coupling state snapshot;
   - derives a `review_required` proposed FaultController action;
   - observes it through `observe_mock_scheduler_fault_action(...)`;
   - consumes it through `consume_mock_scheduler_fault_action(...)`;
   - classifies it through `classify_fault_controller_passive_state(...)`;
   - creates a passive transition through `make_fault_controller_passive_transition(...)`;
   - creates a passive action-audit record through `make_fault_controller_passive_action_audit_record(...)`;
   - runs the same `run_mock_coupling_scheduler_step(...)`.

The audited path is required to match the control path for:

- shared exchange decision count;
- endpoint engine and node metadata;
- granted flow and volume;
- repayment flow and volume;
- unmet volume;
- allocated `Q_limit` and `V_limit` shares;
- priority weights;
- replayed cell volume;
- replayed cell deficit account;
- replayed cell depth;
- system-mass audit baseline total;
- system-mass audit current total;
- system-mass audit residual;
- system-mass audit conservation flag.

The audited path also confirms action-audit creation remains non-controlling:

- `action_kind = operator_review`;
- `reason = review_required_only`;
- `scheduler_control_enabled = false`;
- `runtime_action_requested = false`;
- `runtime_action_executed = false`;
- `isolation_executed = false`;
- `reconnect_executed = false`;
- `abort_executed = false`;
- `adapter_call_executed = false`;
- `release_gate_action_executed = false`.

## Confirmed Negative Boundary

M125-M127 do not add:

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

This stage is evidence for passive action-audit records and ordering compatibility only, not evidence that scheduler control, runtime evidence writing, or executable fault handling exists.

## Evidence Summary

### M126 focused passive action-audit evidence

Focused target:

```text
cmake --build --preset windows-msvc --target test_coupling_fault_controller_passive_action_audit
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_fault_controller_passive_action_audit$"
```

Recorded result in M126:

```text
Test project H:/githubcode/SCAU-UFM/build/windows-msvc
    Start 40: test_coupling_fault_controller_passive_action_audit
1/1 Test #40: test_coupling_fault_controller_passive_action_audit ...   Passed    0.35 sec

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   0.59 sec
```

### M127 focused ordering evidence

Focused target:

```text
cmake --build --preset windows-msvc --target test_coupling_fault_controller_passive_action_audit_ordering
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_fault_controller_passive_action_audit_ordering$"
```

Recorded result in M127:

```text
Test project H:/githubcode/SCAU-UFM/build/windows-msvc
    Start 41: test_coupling_fault_controller_passive_action_audit_ordering
1/1 Test #41: test_coupling_fault_controller_passive_action_audit_ordering ...   Passed    0.27 sec

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   0.49 sec
```

### Latest full local Debug evidence

Latest full local validation recorded in M127:

```text
cmake --build --preset windows-msvc
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
100% tests passed, 0 tests failed out of 50

Total Test time (real) =   8.89 sec
```

## Review Evidence

Manual review for the M125-M127 chain confirms:

- `make_fault_controller_passive_action_audit_record(...)` is side-effect free;
- passive action-audit records preserve transition context;
- passive action-audit records provide action kind, stage, reason, and non-execution flags only;
- passive action-audit records do not promote transition, classification, consumption, observation, or proposal data into scheduler control, runtime action requests, runtime execution, adapter calls, release gates, isolation, reconnect, or abort behavior;
- creating a passive action-audit record before the mock scheduler step does not alter exchange decisions, replay state, or mass audit results;
- no scheduler control, adapter control, native engine control, release gate, or runtime evidence writer is introduced.

## CI Evidence

Pending. Record the push commit and GitHub Actions run after the M125-M128 implementation/evidence changes are pushed.

## Remaining Gaps Before Executable FaultController Work

Before moving from passive action-audit records into executable FaultController or scheduler behavior, remaining gaps include:

1. Define executable action outcome requirements before any action execution DTO implementation.
2. Define and implement a full transition-table evaluator only after thresholds and audit fields are specified.
3. Define threshold and temporal policy for transient versus persistent faults.
4. Define separate execution DTOs for isolation, reconnect, abort, and review-required outcomes.
5. Define scheduler ordering evidence for any future executable action before it controls runtime behavior.
6. Keep real adapter health integration separate until mock-only passive audit boundaries remain stable.
7. Add CI evidence and avoid GoldenSuite release/merge claims until required gate evidence exists.

## Decision

M125-M127 form a coherent passive action-audit stage. The next safe step is a design-only executable action outcome requirements document before any scheduler control implementation.

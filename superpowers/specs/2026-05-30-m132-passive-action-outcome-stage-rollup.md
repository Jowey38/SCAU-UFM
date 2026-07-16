# M132 Passive Action Outcome Stage Rollup

## Scope

M132 rolls up the passive action-outcome design and evidence completed in M129 through M131.

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
→ make_fault_controller_passive_action_outcome(...)
→ run_mock_coupling_scheduler_step(...)
```

M132 is a rollup only. It does not add new production behavior, scheduler control, runtime isolation, reconnect, abort, real adapter integration, CI evidence, GoldenSuite evidence, or release/merge readiness claims.

## Covered Milestones

- M129: documented design-only requirements for future executable action outcomes.
- M130: implemented passive action-outcome DTO/helper records through `FaultControllerPassiveActionOutcomeKind`, `FaultControllerPassiveActionOutcomeReason`, `FaultControllerPassiveActionOutcome`, and `make_fault_controller_passive_action_outcome(...)`.
- M131: added ordering evidence that passive action-outcome record creation before mock scheduling does not change scheduling, replay, or audit results.

## Current Passive Action-Outcome Boundary

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

## Ordering Evidence Summary

M131 verifies a control path against an outcome-recorded path:

1. Control path:
   - starts from a coupling state snapshot;
   - runs `run_mock_coupling_scheduler_step(...)` directly.

2. Outcome-recorded path:
   - starts from an equivalent coupling state snapshot;
   - derives a `review_required` proposed FaultController action;
   - observes it through `observe_mock_scheduler_fault_action(...)`;
   - consumes it through `consume_mock_scheduler_fault_action(...)`;
   - classifies it through `classify_fault_controller_passive_state(...)`;
   - creates a passive transition through `make_fault_controller_passive_transition(...)`;
   - creates a passive action-audit record through `make_fault_controller_passive_action_audit_record(...)`;
   - creates a passive action-outcome record through `make_fault_controller_passive_action_outcome(...)`;
   - runs the same `run_mock_coupling_scheduler_step(...)`.

The outcome-recorded path is required to match the control path for:

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

The outcome-recorded path also confirms action-outcome creation remains non-controlling:

- `outcome_kind = operator_review_required`;
- `reason = review_required_only`;
- `operator_review_required = true`;
- `scheduler_control_available = false`;
- `scheduler_control_used = false`;
- `adapter_boundary_available = false`;
- `adapter_call_attempted = false`;
- `adapter_call_succeeded = false`;
- `adapter_call_failed = false`;
- `runtime_action_requested = false`;
- `runtime_action_attempted = false`;
- `runtime_action_executed = false`;
- `runtime_action_skipped = false`;
- `runtime_action_failed = false`;
- `rollback_required = false`;
- `replay_required = false`;
- `mass_audit_required = false`;
- `release_gate_action_executed = false`.

## Confirmed Negative Boundary

M129-M131 do not add:

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

This stage is evidence for passive action-outcome records and ordering compatibility only, not evidence that scheduler control, runtime evidence writing, or executable fault handling exists.

## Evidence Summary

### M130 focused passive action-outcome evidence

Focused target:

```text
cmake --build --preset windows-msvc --target test_coupling_fault_controller_passive_action_outcome
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_fault_controller_passive_action_outcome$"
```

Recorded result in M130:

```text
Test project H:/githubcode/SCAU-UFM/build/windows-msvc
    Start 42: test_coupling_fault_controller_passive_action_outcome
1/1 Test #42: test_coupling_fault_controller_passive_action_outcome ...   Passed    0.36 sec

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   0.52 sec
```

### M131 focused ordering evidence

Focused target:

```text
cmake --build --preset windows-msvc --target test_coupling_fault_controller_passive_action_outcome_ordering
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_fault_controller_passive_action_outcome_ordering$"
```

Recorded result in M131:

```text
Test project H:/githubcode/SCAU-UFM/build/windows-msvc
    Start 43: test_coupling_fault_controller_passive_action_outcome_ordering
1/1 Test #43: test_coupling_fault_controller_passive_action_outcome_ordering ...   Passed    0.33 sec

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   0.61 sec
```

### Latest full local Debug evidence

Latest full local validation recorded in M131:

```text
cmake --build --preset windows-msvc
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
100% tests passed, 0 tests failed out of 52

Total Test time (real) =  12.70 sec
```

## Review Evidence

Manual review for the M129-M131 chain confirms:

- `make_fault_controller_passive_action_outcome(...)` is side-effect free;
- passive action-outcome records preserve action-audit context;
- passive action-outcome records provide outcome kind, reason, operator-review marker, and non-execution flags only;
- passive action-outcome records do not promote audit, transition, classification, consumption, observation, or proposal data into scheduler control, adapter calls, runtime action requests, runtime attempts, runtime execution, rollback, replay, mass-audit requirements, release gates, isolation, reconnect, or abort behavior;
- creating a passive action-outcome record before the mock scheduler step does not alter exchange decisions, replay state, or mass audit results;
- no scheduler control, adapter control, native engine control, release gate, or runtime evidence writer is introduced.

## CI Evidence

Pending. Record the push commit and GitHub Actions run after the M129-M132 implementation/evidence changes are pushed.

## Remaining Gaps Before Executable FaultController Work

Before moving from passive action-outcome records into executable FaultController or scheduler behavior, remaining gaps include:

1. Define executable action boundary requirements before any execution DTO implementation.
2. Define and implement a full transition-table evaluator only after thresholds and audit fields are specified.
3. Define threshold and temporal policy for transient versus persistent faults.
4. Define separate execution DTOs for isolation, reconnect, abort, and review-required outcomes.
5. Define scheduler ordering evidence for any future executable action before it controls runtime behavior.
6. Keep real adapter health integration separate until mock-only passive outcome boundaries remain stable.
7. Add CI evidence and avoid GoldenSuite release/merge claims until required gate evidence exists.

## Decision

M129-M131 form a coherent passive action-outcome stage. The next safe step is a design-only executable action boundary requirements document before any scheduler control implementation.

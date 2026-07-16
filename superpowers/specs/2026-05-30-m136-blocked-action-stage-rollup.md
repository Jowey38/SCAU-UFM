# M136 Blocked Action Stage Rollup

## Scope

M136 rolls up the blocked-action design and evidence completed in M133 through M135.

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
→ make_fault_controller_blocked_action(...)
→ run_mock_coupling_scheduler_step(...)
```

M136 is a rollup only. It does not add new production behavior, scheduler control, runtime isolation, reconnect, abort, real adapter integration, CI evidence, GoldenSuite evidence, or release/merge readiness claims.

## Covered Milestones

- M133: documented design-only requirements for future executable action boundaries.
- M134: implemented blocked-action DTO/helper records through `FaultControllerBlockedActionReason`, `FaultControllerBlockedAction`, and `make_fault_controller_blocked_action(...)`.
- M135: added ordering evidence that blocked-action record creation before mock scheduling does not change scheduling, replay, or audit results.

## Current Blocked-Action Boundary

`CouplingLib core` owns:

- `FaultControllerBlockedActionReason`
- `FaultControllerBlockedAction`
- `make_fault_controller_blocked_action(...)`

Blocked-action behavior:

- preserves the input `FaultControllerPassiveActionOutcome`;
- maps not-requested outcomes to `reason = no_action_requested`;
- maps operator-review-required outcomes to `reason = operator_review_required`;
- always keeps allow flags false:
  - `execution_allowed = false`
  - `scheduler_control_allowed = false`
  - `adapter_call_allowed = false`
  - `isolation_allowed = false`
  - `reconnect_allowed = false`
  - `abort_allowed = false`
  - `release_gate_action_allowed = false`

## Ordering Evidence Summary

M135 verifies a control path against a blocked-recorded path:

1. Control path:
   - starts from a coupling state snapshot;
   - runs `run_mock_coupling_scheduler_step(...)` directly.

2. Blocked-recorded path:
   - starts from an equivalent coupling state snapshot;
   - derives a `review_required` proposed FaultController action;
   - observes it through `observe_mock_scheduler_fault_action(...)`;
   - consumes it through `consume_mock_scheduler_fault_action(...)`;
   - classifies it through `classify_fault_controller_passive_state(...)`;
   - creates a passive transition through `make_fault_controller_passive_transition(...)`;
   - creates a passive action-audit record through `make_fault_controller_passive_action_audit_record(...)`;
   - creates a passive action-outcome record through `make_fault_controller_passive_action_outcome(...)`;
   - creates a blocked-action record through `make_fault_controller_blocked_action(...)`;
   - runs the same `run_mock_coupling_scheduler_step(...)`.

The blocked-recorded path is required to match the control path for:

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

The blocked-recorded path also confirms blocked-action creation remains non-controlling:

- `reason = operator_review_required`;
- `execution_allowed = false`;
- `scheduler_control_allowed = false`;
- `adapter_call_allowed = false`;
- `isolation_allowed = false`;
- `reconnect_allowed = false`;
- `abort_allowed = false`;
- `release_gate_action_allowed = false`.

## Confirmed Negative Boundary

M133-M135 do not add:

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
- mutation of coupling state from blocked-action records;
- native SWMM process, handle, or C API calls;
- native D-Flow FM / BMI process, handle, or API calls;
- protocol release-gate actions such as `BLOCK_MERGE` or `BLOCK_RELEASE`;
- runtime evidence writing;
- adapter-owned fault policy.

This stage is evidence for blocked-action records and ordering compatibility only, not evidence that scheduler control, runtime evidence writing, or executable fault handling exists.

## Evidence Summary

### M134 focused blocked-action evidence

Focused target:

```text
cmake --build --preset windows-msvc --target test_coupling_fault_controller_blocked_action
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_fault_controller_blocked_action$"
```

Recorded result in M134:

```text
Test project H:/githubcode/SCAU-UFM/build/windows-msvc
    Start 44: test_coupling_fault_controller_blocked_action
1/1 Test #44: test_coupling_fault_controller_blocked_action ...   Passed    0.57 sec

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   0.77 sec
```

### M135 focused ordering evidence

Focused target:

```text
cmake --build --preset windows-msvc --target test_coupling_fault_controller_blocked_action_ordering
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_fault_controller_blocked_action_ordering$"
```

Recorded result in M135:

```text
Test project H:/githubcode/SCAU-UFM/build/windows-msvc
    Start 45: test_coupling_fault_controller_blocked_action_ordering
1/1 Test #45: test_coupling_fault_controller_blocked_action_ordering ...   Passed    0.24 sec

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   0.47 sec
```

### Latest full local Debug evidence

Latest full local validation recorded in M135:

```text
cmake --build --preset windows-msvc
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
100% tests passed, 0 tests failed out of 54

Total Test time (real) =  10.37 sec
```

## Review Evidence

Manual review for the M133-M135 chain confirms:

- `make_fault_controller_blocked_action(...)` is side-effect free;
- blocked-action records preserve action-outcome context;
- blocked-action records provide block reason and allow flags only;
- blocked-action records do not promote outcome, audit, transition, classification, consumption, observation, or proposal data into scheduler control, adapter calls, runtime execution, release gates, isolation, reconnect, or abort behavior;
- creating a blocked-action record before the mock scheduler step does not alter exchange decisions, replay state, or mass audit results;
- no scheduler control, adapter control, native engine control, release gate, or runtime evidence writer is introduced.

## CI Evidence

Pending. Record the push commit and GitHub Actions run after the M133-M136 implementation/evidence changes are pushed.

## Remaining Gaps Before Executable FaultController Work

Before moving from blocked-action records into executable FaultController or scheduler behavior, remaining gaps include:

1. Define executable scheduler-control requirements before any scheduler control implementation.
2. Define and implement a full transition-table evaluator only after thresholds and audit fields are specified.
3. Define threshold and temporal policy for transient versus persistent faults.
4. Define separate execution DTOs for isolation, reconnect, abort, and review-required outcomes.
5. Define scheduler ordering evidence for any future executable action before it controls runtime behavior.
6. Keep real adapter health integration separate until mock-only blocked-action boundaries remain stable.
7. Add CI evidence and avoid GoldenSuite release/merge claims until required gate evidence exists.

## Decision

M133-M135 form a coherent blocked-action stage. The next safe step is a design-only executable scheduler-control requirements document before any scheduler control implementation.

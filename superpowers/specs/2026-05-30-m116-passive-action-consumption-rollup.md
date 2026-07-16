# M116 Passive Action Consumption Rollup

## Scope

M116 rolls up the passive action-consumption design and implementation completed in M114 and M115.

The current passive mock scheduler fault chain is:

```text
EngineReport
→ classify_engine_health(...)
→ aggregate_engine_health(...)
→ make_fault_controller_diagnostic(...)
→ propose_fault_controller_action(...)
→ observe_mock_scheduler_fault_action(...)
→ consume_mock_scheduler_fault_action(...)
→ run_mock_coupling_scheduler_step(...)
```

M116 is a rollup only. It does not add new production behavior, scheduler control, runtime isolation, reconnect, abort, real adapter integration, CI evidence, GoldenSuite evidence, or release/merge readiness claims.

## Covered Milestones

- M114: designed a passive action-consumption DTO/helper boundary and explicitly separated it from scheduler control.
- M115: implemented `MockCouplingSchedulerFaultConsumption` and `consume_mock_scheduler_fault_action(...)`, with focused and ordering tests.

## Current Passive Consumption Boundary

`CouplingLib core` owns:

- `MockCouplingSchedulerFaultConsumption`
- `consume_mock_scheduler_fault_action(...)`

Consumption behavior:

- preserves the input `MockCouplingSchedulerFaultObservation`;
- copies the observed action state into `consumed_state`;
- reports `review_required_consumed = true` only when the observed state is `review_required`;
- keeps passive stage permissions true:
  - `exchange_scheduling_allowed = true`
  - `replay_allowed = true`
  - `audit_allowed = true`
- always keeps execution result flags false:
  - `executed_isolation = false`
  - `executed_reconnect = false`
  - `executed_abort = false`

## Ordering Evidence Summary

M115 verifies a control path against a consumed path:

1. Control path:
   - starts from a coupling state snapshot;
   - runs `run_mock_coupling_scheduler_step(...)` directly.

2. Consumed path:
   - starts from an equivalent coupling state snapshot;
   - derives a `review_required` proposed FaultController action;
   - observes it through `observe_mock_scheduler_fault_action(...)`;
   - consumes it through `consume_mock_scheduler_fault_action(...)`;
   - runs the same `run_mock_coupling_scheduler_step(...)`.

The consumed path is required to match the control path for:

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

The consumed path also confirms consumption remains non-executing:

- `review_required_consumed = true`;
- `exchange_scheduling_allowed = true`;
- `replay_allowed = true`;
- `audit_allowed = true`;
- `executed_isolation = false`;
- `executed_reconnect = false`;
- `executed_abort = false`.

## Confirmed Negative Boundary

M114-M115 do not add:

- FaultController state machine;
- running, isolated, reconnecting, recovered, or failed states;
- state transition table;
- consecutive-failure counters;
- timeout, recovery, or persistence thresholds;
- runtime isolation execution;
- runtime reconnect execution;
- runtime abort execution;
- scheduler process control;
- scheduler lifecycle control;
- skipped exchange scheduling;
- reordered request/arbitration/replay/audit behavior;
- mutation of coupling state from fault consumption;
- native SWMM process, handle, or C API calls;
- native D-Flow FM / BMI process, handle, or API calls;
- protocol release-gate actions such as `BLOCK_MERGE` or `BLOCK_RELEASE`;
- runtime evidence writing;
- adapter-owned fault policy.

This stage is evidence for passive consumption and ordering compatibility only, not evidence that scheduler control or executable fault handling exists.

## Evidence Summary

### M115 focused consumption evidence

Focused target:

```text
cmake --build --preset windows-msvc --target test_coupling_mock_scheduler_fault_consumption test_coupling_mock_scheduler_fault_consumption_ordering
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^(test_coupling_mock_scheduler_fault_consumption|test_coupling_mock_scheduler_fault_consumption_ordering)$"
```

Recorded result in M115:

```text
Test project H:/githubcode/SCAU-UFM/build/windows-msvc
    Start 34: test_coupling_mock_scheduler_fault_consumption
1/2 Test #34: test_coupling_mock_scheduler_fault_consumption ............   Passed    0.26 sec
    Start 35: test_coupling_mock_scheduler_fault_consumption_ordering
2/2 Test #35: test_coupling_mock_scheduler_fault_consumption_ordering ...   Passed    0.38 sec

100% tests passed, 0 tests failed out of 2

Total Test time (real) =   0.87 sec
```

### Latest full local Debug evidence

Latest full local validation recorded in M115:

```text
cmake --build --preset windows-msvc
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
100% tests passed, 0 tests failed out of 44

Total Test time (real) =   9.96 sec
```

## Review Evidence

Manual review for the M114-M115 chain confirms:

- `consume_mock_scheduler_fault_action(...)` is side-effect free;
- the consumption record preserves the observation and records consumption state only;
- consumption does not promote proposed or observed execution flags into executed isolation, reconnect, or abort results;
- consuming a proposed fault action before the mock scheduler step does not alter exchange decisions, replay state, or mass audit results;
- no scheduler control, adapter control, native engine control, release gate, or runtime evidence writer is introduced.

## CI Evidence

Pending. Record the push commit and GitHub Actions run after the M114-M116 implementation/evidence changes are pushed.

## Remaining Gaps Before Executable FaultController Work

Before moving from passive consumption into executable FaultController or scheduler behavior, remaining gaps include:

1. Define an explicit FaultController state machine and transition table.
2. Define threshold and temporal policy for transient versus persistent faults.
3. Define separate execution DTOs for isolation, reconnect, abort, and review-required outcomes.
4. Define audit records for any future executable action.
5. Define scheduler ordering evidence for any future executable action before it controls runtime behavior.
6. Keep real adapter health integration separate until mock-only passive consumption boundaries remain stable.
7. Add CI evidence and avoid GoldenSuite release/merge claims until required gate evidence exists.

## Decision

M114-M115 form a coherent passive action-consumption stage. The next safe step is a design-only executable FaultController state-machine requirements document before any scheduler control implementation.

# M113 Mock Scheduler Passive Observation Rollup

## Scope

M113 rolls up the mock scheduler passive fault-observation work completed in M111 and M112.

The current passive observation chain is:

```text
EngineReport
→ classify_engine_health(...)
→ aggregate_engine_health(...)
→ make_fault_controller_diagnostic(...)
→ propose_fault_controller_action(...)
→ observe_mock_scheduler_fault_action(...)
→ run_mock_coupling_scheduler_step(...)
```

M113 is a rollup only. It does not add new production behavior, scheduler control, runtime isolation, reconnect, abort, real adapter integration, CI evidence, GoldenSuite evidence, or release/merge readiness claims.

## Covered Milestones

- M111: added a non-executing mock scheduler observation boundary for proposed FaultController actions.
- M112: added ordering evidence that observing a proposed fault action before mock exchange scheduling does not alter scheduling, replay, or audit results.

## Current Passive Scheduler Observation Boundary

`CouplingLib core` owns:

- `MockCouplingSchedulerFaultObservation`
- `observe_mock_scheduler_fault_action(...)`

Observation behavior:

- preserves the input `FaultControllerProposedAction`;
- copies the proposed action state into `observed_state`;
- reports `observed_review_required = true` only when the proposed action state is `review_required`;
- always keeps execution result flags false:
  - `executed_isolation = false`
  - `executed_reconnect = false`
  - `executed_abort = false`

## Ordering Evidence Summary

M112 verifies a control path against an observed path:

1. Control path:
   - starts from a coupling state snapshot;
   - runs `run_mock_coupling_scheduler_step(...)` directly.

2. Observed path:
   - starts from an equivalent coupling state snapshot;
   - derives a `review_required` proposed FaultController action;
   - observes it through `observe_mock_scheduler_fault_action(...)`;
   - runs the same `run_mock_coupling_scheduler_step(...)`.

The observed path is required to match the control path for:

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

The observed path also confirms the observation remains non-executing:

- `observed_review_required = true`;
- `executed_isolation = false`;
- `executed_reconnect = false`;
- `executed_abort = false`.

## Confirmed Negative Boundary

M111-M112 do not add:

- FaultController state machine;
- running, isolated, reconnecting, recovered, or failed states;
- state transition table;
- consecutive-failure counters;
- timeout, recovery, or persistence thresholds;
- action-consumption semantics;
- runtime isolation execution;
- runtime reconnect execution;
- runtime abort execution;
- scheduler process control;
- scheduler lifecycle control;
- skipped exchange scheduling;
- reordered request/arbitration/replay/audit behavior;
- mutation of coupling state from fault observation;
- native SWMM process, handle, or C API calls;
- native D-Flow FM / BMI process, handle, or API calls;
- protocol release-gate actions such as `BLOCK_MERGE` or `BLOCK_RELEASE`;
- runtime evidence writing;
- adapter-owned fault policy.

This stage is evidence for passive observation and ordering compatibility only, not evidence that scheduler control or executable fault handling exists.

## Evidence Summary

### M111 focused observation evidence

Focused target:

```text
cmake --build --preset windows-msvc --target test_coupling_mock_scheduler_fault_observation
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_mock_scheduler_fault_observation$"
```

Recorded result in M111:

```text
Test project H:/githubcode/SCAU-UFM/build/windows-msvc
    Start 32: test_coupling_mock_scheduler_fault_observation
1/1 Test #32: test_coupling_mock_scheduler_fault_observation ...   Passed    0.26 sec

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   0.45 sec
```

### M112 focused ordering evidence

Focused target:

```text
cmake --build --preset windows-msvc --target test_coupling_mock_scheduler_fault_observation_ordering
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_mock_scheduler_fault_observation_ordering$"
```

Recorded result in M112:

```text
Test project H:/githubcode/SCAU-UFM/build/windows-msvc
    Start 33: test_coupling_mock_scheduler_fault_observation_ordering
1/1 Test #33: test_coupling_mock_scheduler_fault_observation_ordering ...   Passed    0.26 sec

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   0.42 sec
```

### Latest full local Debug evidence

Latest full local validation recorded in M112:

```text
cmake --build --preset windows-msvc
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
100% tests passed, 0 tests failed out of 42

Total Test time (real) =   7.72 sec
```

## Review Evidence

Manual review for the M111-M112 chain confirms:

- `observe_mock_scheduler_fault_action(...)` is side-effect free;
- the observation record preserves the proposed action and records observation state only;
- observation does not promote proposed or input execution flags into executed isolation, reconnect, or abort results;
- observing a proposed fault action before the mock scheduler step does not alter exchange decisions, replay state, or mass audit results;
- no scheduler control, adapter control, native engine control, release gate, or runtime evidence writer is introduced.

## CI Evidence

Pending. Record the push commit and GitHub Actions run after the M111-M113 implementation/evidence changes are pushed.

## Remaining Gaps Before Executable FaultController Work

Before moving from passive observation into executable FaultController or scheduler behavior, remaining gaps include:

1. Define an explicit FaultController state machine and transition table.
2. Define threshold and temporal policy for transient versus persistent faults.
3. Define passive action-consumption DTOs separately from observation DTOs.
4. Define how isolation, reconnect, abort, and review-required outcomes are represented, consumed, audited, and tested.
5. Define scheduler ordering evidence for any future action-consumption boundary before it can control runtime behavior.
6. Keep real adapter health integration separate until mock-only observation and ordering boundaries remain stable.
7. Add CI evidence and avoid GoldenSuite release/merge claims until required gate evidence exists.

## Decision

M111-M112 form a coherent mock scheduler passive observation stage. The next safe step is a passive action-consumption design document that defines DTOs and negative boundaries before any implementation of scheduler control, isolation, reconnect, or abort behavior.

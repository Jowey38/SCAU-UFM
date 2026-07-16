# M124 Passive Transition Stage Rollup

## Scope

M124 rolls up the passive transition design and evidence completed in M121 through M123.

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
→ run_mock_coupling_scheduler_step(...)
```

M124 is a rollup only. It does not add new production behavior, scheduler control, runtime isolation, reconnect, abort, real adapter integration, CI evidence, GoldenSuite evidence, or release/merge readiness claims.

## Covered Milestones

- M121: documented design-only requirements for a future executable transition table.
- M122: implemented passive transition DTO/helper records through `FaultControllerPassiveTransitionReason`, `FaultControllerPassiveTransition`, and `make_fault_controller_passive_transition(...)`.
- M123: added ordering evidence that passive transition creation before mock scheduling does not change scheduling, replay, or audit results.

## Current Passive Transition Boundary

`CouplingLib core` owns:

- `FaultControllerPassiveTransitionReason`
- `FaultControllerPassiveTransition`
- `make_fault_controller_passive_transition(...)`

Transition behavior:

- preserves the input `FaultControllerPassiveStateClassification`;
- preserves the caller-provided previous passive state label;
- copies the classification state into `next_state`;
- maps `running` to `FaultControllerPassiveTransitionReason::nominal_health`;
- maps `review_required` to `FaultControllerPassiveTransitionReason::fault_detected_review_required`;
- always keeps action/control fields false:
  - `isolation_requested = false`
  - `reconnect_requested = false`
  - `abort_requested = false`
  - `runtime_action_executed = false`
  - `scheduler_control_enabled = false`

## Ordering Evidence Summary

M123 verifies a control path against a transitioned path:

1. Control path:
   - starts from a coupling state snapshot;
   - runs `run_mock_coupling_scheduler_step(...)` directly.

2. Transitioned path:
   - starts from an equivalent coupling state snapshot;
   - derives a `review_required` proposed FaultController action;
   - observes it through `observe_mock_scheduler_fault_action(...)`;
   - consumes it through `consume_mock_scheduler_fault_action(...)`;
   - classifies it through `classify_fault_controller_passive_state(...)`;
   - creates a passive transition through `make_fault_controller_passive_transition(...)`;
   - runs the same `run_mock_coupling_scheduler_step(...)`.

The transitioned path is required to match the control path for:

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

The transitioned path also confirms transition creation remains non-controlling:

- `previous_state = running`;
- `next_state = review_required`;
- `reason = fault_detected_review_required`;
- `isolation_requested = false`;
- `reconnect_requested = false`;
- `abort_requested = false`;
- `runtime_action_executed = false`;
- `scheduler_control_enabled = false`.

## Confirmed Negative Boundary

M121-M123 do not add:

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
- mutation of coupling state from passive transition records;
- native SWMM process, handle, or C API calls;
- native D-Flow FM / BMI process, handle, or API calls;
- protocol release-gate actions such as `BLOCK_MERGE` or `BLOCK_RELEASE`;
- runtime evidence writing;
- adapter-owned fault policy.

This stage is evidence for passive transition records and ordering compatibility only, not evidence that scheduler control or executable fault handling exists.

## Evidence Summary

### M122 focused passive transition evidence

Focused target:

```text
cmake --build --preset windows-msvc --target test_coupling_fault_controller_passive_transition
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_fault_controller_passive_transition$"
```

Recorded result in M122:

```text
Test project H:/githubcode/SCAU-UFM/build/windows-msvc
    Start 38: test_coupling_fault_controller_passive_transition
1/1 Test #38: test_coupling_fault_controller_passive_transition ...   Passed    0.29 sec

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   0.47 sec
```

### M123 focused ordering evidence

Focused target:

```text
cmake --build --preset windows-msvc --target test_coupling_fault_controller_passive_transition_ordering
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_fault_controller_passive_transition_ordering$"
```

Recorded result in M123:

```text
Test project H:/githubcode/SCAU-UFM/build/windows-msvc
    Start 39: test_coupling_fault_controller_passive_transition_ordering
1/1 Test #39: test_coupling_fault_controller_passive_transition_ordering ...   Passed    0.24 sec

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   0.42 sec
```

### Latest full local Debug evidence

Latest full local validation recorded in M123:

```text
cmake --build --preset windows-msvc
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
100% tests passed, 0 tests failed out of 48

Total Test time (real) =   8.74 sec
```

## Review Evidence

Manual review for the M121-M123 chain confirms:

- `make_fault_controller_passive_transition(...)` is side-effect free;
- transition records preserve passive classification context;
- transition records provide previous state, next state, and reason only;
- transition records do not promote classification, consumption, observation, or proposal data into scheduler control, isolation, reconnect, abort, or runtime execution;
- creating a passive transition before the mock scheduler step does not alter exchange decisions, replay state, or mass audit results;
- no scheduler control, adapter control, native engine control, release gate, or runtime evidence writer is introduced.

## CI Evidence

Pending. Record the push commit and GitHub Actions run after the M121-M124 implementation/evidence changes are pushed.

## Remaining Gaps Before Executable FaultController Work

Before moving from passive transition records into executable FaultController or scheduler behavior, remaining gaps include:

1. Define audit schema requirements for any future executable action.
2. Define and implement a full transition-table evaluator only after thresholds and audit fields are specified.
3. Define threshold and temporal policy for transient versus persistent faults.
4. Define separate execution DTOs for isolation, reconnect, abort, and review-required outcomes.
5. Define scheduler ordering evidence for any future executable action before it controls runtime behavior.
6. Keep real adapter health integration separate until mock-only passive transition boundaries remain stable.
7. Add CI evidence and avoid GoldenSuite release/merge claims until required gate evidence exists.

## Decision

M121-M123 form a coherent passive transition stage. The next safe step is a design-only executable action audit schema requirements document before any scheduler control implementation.

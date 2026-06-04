# M120 Passive State Classification Rollup

## Scope

M120 rolls up the passive state-classification design and evidence completed in M117 through M119.

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
→ run_mock_coupling_scheduler_step(...)
```

M120 is a rollup only. It does not add new production behavior, scheduler control, runtime isolation, reconnect, abort, real adapter integration, CI evidence, GoldenSuite evidence, or release/merge readiness claims.

## Covered Milestones

- M117: documented design-only requirements for a future executable FaultController state machine.
- M118: implemented passive state-label classification through `FaultControllerPassiveStateLabel`, `FaultControllerPassiveStateClassification`, and `classify_fault_controller_passive_state(...)`.
- M119: added ordering evidence that passive state classification before mock scheduling does not change scheduling, replay, or audit results.

## Current Passive State Classification Boundary

`CouplingLib core` owns:

- `FaultControllerPassiveStateLabel`
- `FaultControllerPassiveStateClassification`
- `classify_fault_controller_passive_state(...)`

Classification behavior:

- preserves the input `MockCouplingSchedulerFaultConsumption`;
- maps `FaultControllerProposedActionState::continue_run` to `FaultControllerPassiveStateLabel::running`;
- maps `FaultControllerProposedActionState::review_required` to `FaultControllerPassiveStateLabel::review_required`;
- always keeps control flags false:
  - `scheduler_control_enabled = false`
  - `isolation_enabled = false`
  - `reconnect_enabled = false`
  - `abort_enabled = false`

## Ordering Evidence Summary

M119 verifies a control path against a classified path:

1. Control path:
   - starts from a coupling state snapshot;
   - runs `run_mock_coupling_scheduler_step(...)` directly.

2. Classified path:
   - starts from an equivalent coupling state snapshot;
   - derives a `review_required` proposed FaultController action;
   - observes it through `observe_mock_scheduler_fault_action(...)`;
   - consumes it through `consume_mock_scheduler_fault_action(...)`;
   - classifies it through `classify_fault_controller_passive_state(...)`;
   - runs the same `run_mock_coupling_scheduler_step(...)`.

The classified path is required to match the control path for:

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

The classified path also confirms classification remains non-controlling:

- `state = review_required`;
- `scheduler_control_enabled = false`;
- `isolation_enabled = false`;
- `reconnect_enabled = false`;
- `abort_enabled = false`.

## Confirmed Negative Boundary

M117-M119 do not add:

- executable FaultController state machine;
- running/isolated/reconnecting/recovered/failed transition table;
- consecutive-failure counters;
- timeout, recovery, or persistence thresholds;
- runtime isolation execution;
- runtime reconnect execution;
- runtime abort execution;
- scheduler process control;
- scheduler lifecycle control;
- skipped exchange scheduling;
- reordered request/arbitration/replay/audit behavior;
- mutation of coupling state from passive state classification;
- native SWMM process, handle, or C API calls;
- native D-Flow FM / BMI process, handle, or API calls;
- protocol release-gate actions such as `BLOCK_MERGE` or `BLOCK_RELEASE`;
- runtime evidence writing;
- adapter-owned fault policy.

This stage is evidence for passive state labeling and ordering compatibility only, not evidence that scheduler control or executable fault handling exists.

## Evidence Summary

### M118 focused passive classification evidence

Focused target:

```text
cmake --build --preset windows-msvc --target test_coupling_fault_controller_passive_state
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_fault_controller_passive_state$"
```

Recorded result in M118:

```text
Test project H:/githubcode/SCAU-UFM/build/windows-msvc
    Start 36: test_coupling_fault_controller_passive_state
1/1 Test #36: test_coupling_fault_controller_passive_state ...   Passed    0.58 sec

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   0.83 sec
```

### M119 focused ordering evidence

Focused target:

```text
cmake --build --preset windows-msvc --target test_coupling_fault_controller_passive_state_ordering
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_fault_controller_passive_state_ordering$"
```

Recorded result in M119:

```text
Test project H:/githubcode/SCAU-UFM/build/windows-msvc
    Start 37: test_coupling_fault_controller_passive_state_ordering
1/1 Test #37: test_coupling_fault_controller_passive_state_ordering ...   Passed    0.24 sec

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   0.44 sec
```

### Latest full local Debug evidence

Latest full local validation recorded in M119:

```text
cmake --build --preset windows-msvc
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
100% tests passed, 0 tests failed out of 46

Total Test time (real) =   8.19 sec
```

## Review Evidence

Manual review for the M117-M119 chain confirms:

- `classify_fault_controller_passive_state(...)` is side-effect free;
- classification preserves the passive consumption record;
- classification labels future state data only;
- classification does not promote consumed, observed, or proposed flags into scheduler control, isolation, reconnect, or abort capability;
- classifying a proposed fault state before the mock scheduler step does not alter exchange decisions, replay state, or mass audit results;
- no scheduler control, adapter control, native engine control, release gate, or runtime evidence writer is introduced.

## CI Evidence

Pending. Record the push commit and GitHub Actions run after the M117-M120 implementation/evidence changes are pushed.

## Remaining Gaps Before Executable FaultController Work

Before moving from passive state classification into executable FaultController or scheduler behavior, remaining gaps include:

1. Define an explicit executable transition table.
2. Define threshold and temporal policy for transient versus persistent faults.
3. Define separate execution DTOs for isolation, reconnect, abort, and review-required outcomes.
4. Define audit records for any future executable action.
5. Define scheduler ordering evidence for any future executable action before it controls runtime behavior.
6. Keep real adapter health integration separate until mock-only passive classification boundaries remain stable.
7. Add CI evidence and avoid GoldenSuite release/merge claims until required gate evidence exists.

## Decision

M117-M119 form a coherent passive state-classification stage. The next safe step is a design-only executable transition table requirements document before any scheduler control implementation.

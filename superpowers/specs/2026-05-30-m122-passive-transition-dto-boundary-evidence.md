# M122 Passive Transition DTO Boundary Evidence

## Scope

M122 implements the next safest step after the M121 design-only transition-table requirements: a passive transition DTO/helper boundary.

The current passive chain can now emit a transition record without scheduler control:

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
```

This stage records previous state, next state, and reason only. It does not execute isolation, reconnect, abort, scheduler process control, release-gate behavior, or real adapter behavior.

## Implemented Core Boundary

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

## Tests

Focused tests were added under:

- `tests/unit/coupling/test_coupling_fault_controller_passive_transition.cpp`

Registered target:

- `test_coupling_fault_controller_passive_transition`

Asserted invariants:

- continue-run classification produces a nominal running transition;
- review-required classification produces a review-required transition with fault-detected reason;
- transition preserves the classification record;
- transition does not promote classification control flags into requested isolation, reconnect, abort, runtime execution, or scheduler control.

## Confirmed Negative Boundary

M122 does not add:

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

The transition record is still a passive DTO seam only. It is not executable scheduler control.

## Local Evidence

Focused build and CTest:

```text
cmake --build --preset windows-msvc --target test_coupling_fault_controller_passive_transition
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_fault_controller_passive_transition$"
```

Result:

```text
Test project H:/githubcode/SCAU-UFM/build/windows-msvc
    Start 38: test_coupling_fault_controller_passive_transition
1/1 Test #38: test_coupling_fault_controller_passive_transition ...   Passed    0.29 sec

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   0.47 sec
```

## CI Evidence

Pending. Record the push commit and GitHub Actions run after the M122 implementation/evidence changes are pushed.

## Remaining Gaps Before Executable FaultController Work

Before moving from passive transition records into executable FaultController or scheduler behavior, remaining gaps include:

1. Define and implement a full transition-table evaluator only after thresholds and audit fields are specified.
2. Define threshold and temporal policy for transient versus persistent faults.
3. Define separate execution DTOs for isolation, reconnect, abort, and review-required outcomes.
4. Define audit records for any future executable action.
5. Define scheduler ordering evidence for any future executable action before it controls runtime behavior.
6. Keep real adapter health integration separate until mock-only passive transition boundaries remain stable.
7. Add CI evidence and avoid GoldenSuite release/merge claims until required gate evidence exists.

## Decision

M122 establishes a passive transition DTO/helper boundary and proves it does not enable scheduler control, isolation, reconnect, or abort behavior. The next safe step is ordering evidence that passive transition creation before mock scheduling still leaves scheduling, replay, and audit unchanged.

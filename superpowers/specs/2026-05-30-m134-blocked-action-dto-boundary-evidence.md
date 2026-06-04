# M134 Blocked Action DTO Boundary Evidence

## Scope

M134 implements the next safest step after M133 design-only executable action boundary requirements: a blocked-action DTO/helper boundary.

The current passive chain can now emit an explicit blocked-action record without scheduler control:

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
```

This stage records why execution is not allowed. It does not execute isolation, reconnect, abort, scheduler process control, release-gate behavior, or real adapter behavior.

## Implemented Core Boundary

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

## Tests

Focused tests were added under:

- `tests/unit/coupling/test_coupling_fault_controller_blocked_action.cpp`

Registered target:

- `test_coupling_fault_controller_blocked_action`

Asserted invariants:

- nominal outcomes produce no-action blocked records;
- review-required outcomes produce operator-review blocked records;
- blocked-action records preserve the outcome record;
- blocked-action creation does not promote outcome control/execution flags into execution allowance, scheduler control allowance, adapter call allowance, isolation allowance, reconnect allowance, abort allowance, or release-gate allowance.

## Confirmed Negative Boundary

M134 does not add:

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

The blocked-action record is still a passive DTO seam only. It is not executable scheduler control or runtime evidence writing.

## Local Evidence

Focused build and CTest:

```text
cmake --build --preset windows-msvc --target test_coupling_fault_controller_blocked_action
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_fault_controller_blocked_action$"
```

Result:

```text
Test project H:/githubcode/SCAU-UFM/build/windows-msvc
    Start 44: test_coupling_fault_controller_blocked_action
1/1 Test #44: test_coupling_fault_controller_blocked_action ...   Passed    0.57 sec

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   0.77 sec
```

## CI Evidence

Pending. Record the push commit and GitHub Actions run after the M134 implementation/evidence changes are pushed.

## Remaining Gaps Before Executable FaultController Work

Before moving from blocked-action records into executable FaultController or scheduler behavior, remaining gaps include:

1. Add ordering evidence that blocked-action creation before mock scheduling does not alter scheduling, replay, or audit behavior.
2. Define and implement a full transition-table evaluator only after thresholds and audit fields are specified.
3. Define threshold and temporal policy for transient versus persistent faults.
4. Define separate execution DTOs for isolation, reconnect, abort, and review-required outcomes.
5. Define scheduler ordering evidence for any future executable action before it controls runtime behavior.
6. Keep real adapter health integration separate until mock-only blocked-action boundaries remain stable.
7. Add CI evidence and avoid GoldenSuite release/merge claims until required gate evidence exists.

## Decision

M134 establishes a blocked-action DTO/helper boundary and proves it does not allow scheduler control, runtime execution, adapter calls, release gates, isolation, reconnect, or abort behavior. The next safe step is ordering evidence that blocked-action creation before mock scheduling still leaves scheduling, replay, and audit unchanged.

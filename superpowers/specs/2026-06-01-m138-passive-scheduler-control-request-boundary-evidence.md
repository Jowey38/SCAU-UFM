# M138 Passive Scheduler-Control Request Boundary Evidence

## Scope

M138 implements a passive scheduler-control request DTO skeleton in `CouplingLib core`.

This milestone does not add scheduler-control execution, scheduler phase mutation, skipped exchange scheduling, replay holds, forced mass audit, adapter calls, isolation/reconnect/abort execution, runtime evidence writers, CI claims, GoldenSuite claims, or release-gate actions.

## Implemented Boundary

`CouplingLib core` owns:

- `FaultControllerSchedulerControlKind`
- `FaultControllerSchedulerControlStatus`
- `FaultControllerSchedulerControlReason`
- `FaultControllerSchedulerControlRequest`
- `make_fault_controller_scheduler_control_request(...)`

The passive helper:

- preserves the input `FaultControllerPassiveActionOutcome`;
- preserves the input `FaultControllerBlockedAction`;
- maps no-action blocked records to:
  - `requested_kind = none`
  - `status = not_requested`
  - `reason = no_scheduler_control_requested`
- maps operator-review blocked records to:
  - `requested_kind = observe_only`
  - `status = blocked_boundary_absent`
  - `reason = scheduler_control_boundary_absent`
- records that scheduler-control boundary support, threshold evidence, operator approval, rollback context, replay policy, and mass-audit policy are unavailable;
- keeps all scheduler-control, adapter-call, replay-hold, mass-audit-forcing, scheduler-mutation, and release-gate execution flags false.

## Focused Evidence

### Passive request DTO/helper

Command:

```text
cmake --build --preset windows-msvc --target test_coupling_fault_controller_scheduler_control_request
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_fault_controller_scheduler_control_request$"
```

Result:

```text
Test project H:/githubcode/SCAU-UFM/build/windows-msvc
    Start 46: test_coupling_fault_controller_scheduler_control_request
1/1 Test #46: test_coupling_fault_controller_scheduler_control_request ...   Passed    0.47 sec

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   0.73 sec
```

### Ordering evidence

Command:

```text
cmake --build --preset windows-msvc --target test_coupling_fault_controller_scheduler_control_request_ordering
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_fault_controller_scheduler_control_request_ordering$"
```

Result:

```text
Test project H:/githubcode/SCAU-UFM/build/windows-msvc
    Start 47: test_coupling_fault_controller_scheduler_control_request_ordering
1/1 Test #47: test_coupling_fault_controller_scheduler_control_request_ordering ...   Passed    0.39 sec

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   0.49 sec
```

## Ordering Evidence Summary

The ordering test compares a baseline mock scheduler path with a path that creates a passive scheduler-control request before running `run_mock_coupling_scheduler_step(...)`.

The request-recorded path matches the baseline for:

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

## Confirmed Negative Boundary

M138 does not add:

- executable FaultController state machine;
- transition-table evaluator;
- threshold counters;
- runtime isolation execution;
- runtime reconnect execution;
- runtime abort execution;
- scheduler process control;
- scheduler lifecycle control;
- skipped exchange scheduling;
- reordered request/arbitration/replay/audit behavior;
- replay holds;
- forced mass audit;
- mutation of coupling state from scheduler-control requests;
- native SWMM process, handle, or C API calls;
- native D-Flow FM / BMI process, handle, or API calls;
- protocol release-gate actions such as `BLOCK_MERGE` or `BLOCK_RELEASE`;
- runtime evidence writing;
- adapter-owned fault policy.

This stage is evidence for a passive request skeleton and ordering compatibility only, not evidence that scheduler control, runtime evidence writing, or executable fault handling exists.

## Full Local Debug Evidence

Command:

```text
cmake --build --preset windows-msvc
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
100% tests passed, 0 tests failed out of 56

Total Test time (real) =  15.23 sec
```

## CI Evidence

Pending. Record the push commit and GitHub Actions run after the M138 implementation/evidence changes are pushed.

## Decision

M138 establishes a passive scheduler-control request boundary that fails closed when scheduler-control execution is unavailable. The next safe step is either a rollup of M137-M138 or another design-only milestone for explicit scheduler-control result requirements before any executable scheduler control is implemented.

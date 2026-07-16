# M140 Passive Scheduler-Control Result Boundary Evidence

## Scope

M140 implements a passive scheduler-control result DTO skeleton in `CouplingLib core`.

This milestone does not add scheduler-control execution, scheduler phase mutation, skipped exchange scheduling, replay holds, forced mass audit, adapter calls, isolation/reconnect/abort execution, runtime evidence writers, CI claims, GoldenSuite claims, or release-gate actions.

## Implemented Boundary

`CouplingLib core` owns:

- `FaultControllerSchedulerControlResultStatus`
- `FaultControllerSchedulerControlResultReason`
- `FaultControllerSchedulerControlResult`
- `make_fault_controller_scheduler_control_result(...)`

The passive helper:

- preserves the input `FaultControllerSchedulerControlRequest`;
- maps request `requested_kind = none` to:
  - `attempted_kind = none`
  - `status = not_requested`
  - `reason = no_scheduler_control_requested`
- maps request `requested_kind = observe_only` to:
  - `attempted_kind = observe_only`
  - `status = blocked_boundary_absent`
  - `reason = scheduler_control_boundary_absent`
- records passive phase labels only:
  - `phase_before_control = passive_scheduler_control_request_recorded`
  - `phase_after_control = passive_scheduler_control_result_recorded`
- preserves scheduler-control boundary availability, threshold evidence, operator approval, rollback context, replay policy, and mass-audit policy as descriptive request provenance;
- keeps all scheduler-control, exchange-pause, target-skip, replay-hold, mass-audit-forcing, scheduler-mutation, adapter-call, rollback/replay/mass-audit requirement, and release-gate execution flags false.

## Focused Evidence

### Passive result DTO/helper

Command:

```text
cmake --build --preset windows-msvc --target test_coupling_fault_controller_scheduler_control_result
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_fault_controller_scheduler_control_result$"
```

Result:

```text
Test project H:/githubcode/SCAU-UFM/build/windows-msvc
    Start 48: test_coupling_fault_controller_scheduler_control_result
1/1 Test #48: test_coupling_fault_controller_scheduler_control_result ...   Passed    0.36 sec

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   0.54 sec
```

### Ordering evidence

Command:

```text
cmake --build --preset windows-msvc --target test_coupling_fault_controller_scheduler_control_result_ordering
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_fault_controller_scheduler_control_result_ordering$"
```

Result:

```text
Test project H:/githubcode/SCAU-UFM/build/windows-msvc
    Start 49: test_coupling_fault_controller_scheduler_control_result_ordering
1/1 Test #49: test_coupling_fault_controller_scheduler_control_result_ordering ...   Passed    0.40 sec

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   0.50 sec
```

## Ordering Evidence Summary

The ordering test compares a baseline mock scheduler path with a path that creates a passive scheduler-control result before running `run_mock_coupling_scheduler_step(...)`.

The result-recorded path matches the baseline for:

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

M140 does not add:

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
- mutation of coupling state from scheduler-control results;
- native SWMM process, handle, or C API calls;
- native D-Flow FM / BMI process, handle, or API calls;
- protocol release-gate actions such as `BLOCK_MERGE` or `BLOCK_RELEASE`;
- runtime evidence writing;
- adapter-owned fault policy.

This stage is evidence for a passive result skeleton and ordering compatibility only, not evidence that scheduler control, runtime evidence writing, or executable fault handling exists.

## Full Local Debug Evidence

Command:

```text
cmake --build --preset windows-msvc
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
100% tests passed, 0 tests failed out of 58

Total Test time (real) =  14.76 sec
```

## CI Evidence

Pending. Record the push commit and GitHub Actions run after the M140 implementation/evidence changes are pushed.

## Decision

M140 establishes a passive scheduler-control result boundary that preserves request provenance and fails closed when scheduler-control execution is unavailable. The next safe step is either a rollup of M137-M140 or another design-only milestone for explicit executable scheduler-control prerequisites before any scheduler behavior is changed.

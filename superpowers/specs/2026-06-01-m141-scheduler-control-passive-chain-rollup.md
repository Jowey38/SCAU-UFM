# M141 Scheduler-Control Passive Chain Rollup

## Scope

M141 rolls up the scheduler-control design and passive evidence completed in M137 through M140.

This rollup does not add new production behavior, scheduler-control execution, scheduler phase mutation, skipped exchange scheduling, replay holds, forced mass audit, adapter calls, isolation/reconnect/abort execution, runtime evidence writing, CI evidence, GoldenSuite evidence, or release/merge readiness claims.

## Covered Milestones

- M137: documented design-only requirements for a future executable scheduler-control boundary.
- M138: implemented passive scheduler-control request DTO/helper records through `FaultControllerSchedulerControlKind`, `FaultControllerSchedulerControlStatus`, `FaultControllerSchedulerControlReason`, `FaultControllerSchedulerControlRequest`, and `make_fault_controller_scheduler_control_request(...)`.
- M139: documented design-only requirements for a future scheduler-control result boundary.
- M140: implemented passive scheduler-control result DTO/helper records through `FaultControllerSchedulerControlResultStatus`, `FaultControllerSchedulerControlResultReason`, `FaultControllerSchedulerControlResult`, and `make_fault_controller_scheduler_control_result(...)`.

## Current Passive Chain

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
→ make_fault_controller_scheduler_control_request(...)
→ make_fault_controller_scheduler_control_result(...)
→ run_mock_coupling_scheduler_step(...)
```

All records in this chain remain descriptive. None of them execute scheduler control, adapter actions, runtime isolation, reconnect, abort, replay holds, mass-audit forcing, or release-gate actions.

## Current Scheduler-Control Request Boundary

`CouplingLib core` owns:

- `FaultControllerSchedulerControlKind`
- `FaultControllerSchedulerControlStatus`
- `FaultControllerSchedulerControlReason`
- `FaultControllerSchedulerControlRequest`
- `make_fault_controller_scheduler_control_request(...)`

Passive request behavior:

- preserves `FaultControllerPassiveActionOutcome`;
- preserves `FaultControllerBlockedAction`;
- maps not-requested blocked records to:
  - `requested_kind = none`
  - `status = not_requested`
  - `reason = no_scheduler_control_requested`
- maps operator-review blocked records to:
  - `requested_kind = observe_only`
  - `status = blocked_boundary_absent`
  - `reason = scheduler_control_boundary_absent`
- records scheduler-control boundary, threshold evidence, operator approval, rollback context, replay policy, and mass-audit policy availability as unavailable by default;
- keeps scheduler-control, adapter-call, replay-hold, mass-audit-forcing, scheduler-mutation, and release-gate execution flags false.

## Current Scheduler-Control Result Boundary

`CouplingLib core` owns:

- `FaultControllerSchedulerControlResultStatus`
- `FaultControllerSchedulerControlResultReason`
- `FaultControllerSchedulerControlResult`
- `make_fault_controller_scheduler_control_result(...)`

Passive result behavior:

- preserves `FaultControllerSchedulerControlRequest`;
- maps `requested_kind = none` to:
  - `attempted_kind = none`
  - `status = not_requested`
  - `reason = no_scheduler_control_requested`
- maps `requested_kind = observe_only` to:
  - `attempted_kind = observe_only`
  - `status = blocked_boundary_absent`
  - `reason = scheduler_control_boundary_absent`
- records passive phase labels only:
  - `phase_before_control = passive_scheduler_control_request_recorded`
  - `phase_after_control = passive_scheduler_control_result_recorded`
- preserves boundary availability, threshold evidence, operator approval, rollback context, replay policy, and mass-audit policy as descriptive request provenance;
- keeps scheduler-control, exchange-pause, target-skip, replay-hold, mass-audit-forcing, scheduler-mutation, adapter-call, rollback/replay/mass-audit requirement, and release-gate execution flags false.

## Ordering Evidence Summary

M138 verifies a control path against a passive request-recorded path.

M140 verifies a control path against a passive result-recorded path.

Both recorded paths are required to match the control path for:

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

M137-M140 do not add:

- executable FaultController state machine;
- transition-table evaluator;
- threshold counters;
- runtime isolation execution;
- runtime reconnect execution;
- runtime abort execution;
- scheduler process control;
- scheduler lifecycle control;
- scheduler phase mutation;
- skipped exchange scheduling;
- altered shared-cell arbitration;
- reordered request/arbitration/replay/audit behavior;
- replay holds;
- replay skipping;
- forced mass audit;
- mutation of coupling state from scheduler-control request/result records;
- native SWMM process, handle, or C API calls;
- native D-Flow FM / BMI process, handle, or API calls;
- drainage and river native object sharing;
- protocol release-gate actions such as `BLOCK_MERGE` or `BLOCK_RELEASE`;
- runtime evidence writing;
- adapter-owned fault policy.

This stage is evidence for passive scheduler-control request/result records and ordering compatibility only, not evidence that scheduler control, runtime evidence writing, or executable fault handling exists.

## Evidence Summary

### M138 passive request focused evidence

Focused target:

```text
cmake --build --preset windows-msvc --target test_coupling_fault_controller_scheduler_control_request
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_fault_controller_scheduler_control_request$"
```

Recorded result in M138:

```text
Test project H:/githubcode/SCAU-UFM/build/windows-msvc
    Start 46: test_coupling_fault_controller_scheduler_control_request
1/1 Test #46: test_coupling_fault_controller_scheduler_control_request ...   Passed    0.47 sec

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   0.73 sec
```

### M138 passive request ordering evidence

Focused target:

```text
cmake --build --preset windows-msvc --target test_coupling_fault_controller_scheduler_control_request_ordering
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_fault_controller_scheduler_control_request_ordering$"
```

Recorded result in M138:

```text
Test project H:/githubcode/SCAU-UFM/build/windows-msvc
    Start 47: test_coupling_fault_controller_scheduler_control_request_ordering
1/1 Test #47: test_coupling_fault_controller_scheduler_control_request_ordering ...   Passed    0.39 sec

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   0.49 sec
```

### M140 passive result focused evidence

Focused target:

```text
cmake --build --preset windows-msvc --target test_coupling_fault_controller_scheduler_control_result
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_fault_controller_scheduler_control_result$"
```

Recorded result in M140:

```text
Test project H:/githubcode/SCAU-UFM/build/windows-msvc
    Start 48: test_coupling_fault_controller_scheduler_control_result
1/1 Test #48: test_coupling_fault_controller_scheduler_control_result ...   Passed    0.36 sec

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   0.54 sec
```

### M140 passive result ordering evidence

Focused target:

```text
cmake --build --preset windows-msvc --target test_coupling_fault_controller_scheduler_control_result_ordering
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_fault_controller_scheduler_control_result_ordering$"
```

Recorded result in M140:

```text
Test project H:/githubcode/SCAU-UFM/build/windows-msvc
    Start 49: test_coupling_fault_controller_scheduler_control_result_ordering
1/1 Test #49: test_coupling_fault_controller_scheduler_control_result_ordering ...   Passed    0.40 sec

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   0.50 sec
```

### Latest full local Debug evidence

Latest full local validation recorded in M140:

```text
cmake --build --preset windows-msvc
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
100% tests passed, 0 tests failed out of 58

Total Test time (real) =  14.76 sec
```

## Review Evidence

Manual review for the M137-M140 chain confirms:

- scheduler-control request records are side-effect free;
- scheduler-control result records are side-effect free;
- request/result records preserve passive provenance;
- request/result records fail closed when executable scheduler-control boundary support is absent;
- creating request/result records before the mock scheduler step does not alter exchange decisions, replay state, or mass audit results;
- no scheduler control, adapter control, native engine control, release gate, replay hold, force-audit behavior, or runtime evidence writer is introduced.

## CI Evidence

Pending. Record the push commit and GitHub Actions run after the M137-M141 implementation/evidence changes are pushed.

## Remaining Gaps Before Executable Scheduler-Control Work

Before moving from passive scheduler-control records into executable scheduler behavior, remaining gaps include:

1. Define exact scheduler phases as code-facing stable identifiers.
2. Define threshold and temporal policy for transient versus persistent faults.
3. Define evidence-completeness validation for threshold, rollback, replay, and mass-audit context.
4. Define operator-approval policy and review state transitions.
5. Define a future executable scheduler-control boundary separate from passive request/result DTOs.
6. Add ordering evidence for every future executable control kind before it changes scheduling, replay, or audit behavior.
7. Keep adapter calls, isolation, reconnect, abort, and release-gate behavior separate until their own explicit execution DTOs and evidence exist.
8. Add CI evidence and avoid GoldenSuite release/merge claims until required gate evidence exists.

## Decision

M137-M140 form a coherent passive scheduler-control request/result stage. The next safe step is a design-only milestone for stable scheduler phase identifiers and evidence-completeness validation requirements before any scheduler-control execution is implemented.

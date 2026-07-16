# M135 Blocked Action Ordering Evidence

## Scope

M135 adds ordering evidence that blocked-action record creation can occur before a mock scheduler exchange step without changing exchange scheduling, replay, or audit results.

The passive chain remains non-executing and non-controlling:

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

M135 is not an executable FaultController, scheduler control boundary, adapter boundary, release-gate action, or runtime evidence writer. The mock scheduler step does not consume the blocked-action record to isolate, reconnect, abort, skip, reorder, or alter exchange behavior.

## Implemented Test Boundary

Focused test target:

- `test_coupling_fault_controller_blocked_action_ordering`

Test file:

- `tests/unit/coupling/test_coupling_fault_controller_blocked_action_ordering.cpp`

The test constructs two identical coupling states:

1. A control path that runs `run_mock_coupling_scheduler_step(...)` directly.
2. A blocked-recorded path that first derives a `review_required` blocked-action record, then runs the same `run_mock_coupling_scheduler_step(...)`.

The comparison asserts that blocked-action record creation does not change:

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

It also confirms the blocked-action record itself remains non-controlling:

- `reason = operator_review_required`;
- `execution_allowed = false`;
- `scheduler_control_allowed = false`;
- `adapter_call_allowed = false`;
- `isolation_allowed = false`;
- `reconnect_allowed = false`;
- `abort_allowed = false`;
- `release_gate_action_allowed = false`.

## Confirmed Negative Boundary

M135 does not add:

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

The blocked-action record remains ordering evidence for a passive DTO seam, not executable scheduler control or runtime evidence writing.

## Local Evidence

Focused build and CTest:

```text
cmake --build --preset windows-msvc --target test_coupling_fault_controller_blocked_action_ordering
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_fault_controller_blocked_action_ordering$"
```

Result:

```text
Test project H:/githubcode/SCAU-UFM/build/windows-msvc
    Start 45: test_coupling_fault_controller_blocked_action_ordering
1/1 Test #45: test_coupling_fault_controller_blocked_action_ordering ...   Passed    0.24 sec

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   0.47 sec
```

Full local Debug validation:

```text
cmake --build --preset windows-msvc
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
100% tests passed, 0 tests failed out of 54

Total Test time (real) =  10.37 sec
```

## CI Evidence

Pending. Record the push commit and GitHub Actions run after the M135 implementation/evidence changes are pushed.

## Remaining Gaps Before Executable FaultController Work

Before moving from blocked-action ordering into executable FaultController or scheduler behavior, remaining gaps include:

1. Roll up M133-M135 blocked-action work.
2. Define and implement a full transition-table evaluator only after thresholds and audit fields are specified.
3. Define threshold and temporal policy for transient versus persistent faults.
4. Define separate execution DTOs for isolation, reconnect, abort, and review-required outcomes.
5. Define scheduler ordering evidence for any future executable action before it controls runtime behavior.
6. Keep real adapter health integration separate until mock-only blocked-action boundaries remain stable.
7. Add CI evidence and avoid GoldenSuite release/merge claims until required gate evidence exists.

## Decision

M135 confirms that blocked-action record creation can occur before mock exchange scheduling without altering scheduling, replay, or audit results. The next safe step is to roll up M133-M135 or continue with design-only executable scheduler-control requirements before any scheduler control implementation.

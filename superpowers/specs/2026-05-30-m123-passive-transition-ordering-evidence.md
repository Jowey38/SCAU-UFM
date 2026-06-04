# M123 Passive Transition Ordering Evidence

## Scope

M123 adds ordering evidence that passive FaultController transition creation can occur before a mock scheduler exchange step without changing exchange scheduling, replay, or audit results.

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
→ run_mock_coupling_scheduler_step(...)
```

M123 is not an executable FaultController or scheduler control boundary. The mock scheduler step does not consume the transition to isolate, reconnect, abort, skip, reorder, or alter exchange behavior.

## Implemented Test Boundary

Focused test target:

- `test_coupling_fault_controller_passive_transition_ordering`

Test file:

- `tests/unit/coupling/test_coupling_fault_controller_passive_transition_ordering.cpp`

The test constructs two identical coupling states:

1. A control path that runs `run_mock_coupling_scheduler_step(...)` directly.
2. A transitioned path that first derives a `review_required` passive transition record, then runs the same `run_mock_coupling_scheduler_step(...)`.

The comparison asserts that transition creation does not change:

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

It also confirms the transition itself remains non-controlling:

- `previous_state = running`;
- `next_state = review_required`;
- `reason = fault_detected_review_required`;
- `isolation_requested = false`;
- `reconnect_requested = false`;
- `abort_requested = false`;
- `runtime_action_executed = false`;
- `scheduler_control_enabled = false`.

## Confirmed Negative Boundary

M123 does not add:

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

The transition remains ordering evidence for a passive DTO seam, not executable scheduler control.

## Local Evidence

Focused build and CTest:

```text
cmake --build --preset windows-msvc --target test_coupling_fault_controller_passive_transition_ordering
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_fault_controller_passive_transition_ordering$"
```

Result:

```text
Test project H:/githubcode/SCAU-UFM/build/windows-msvc
    Start 39: test_coupling_fault_controller_passive_transition_ordering
1/1 Test #39: test_coupling_fault_controller_passive_transition_ordering ...   Passed    0.24 sec

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   0.42 sec
```

Full local Debug validation:

```text
cmake --build --preset windows-msvc
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
100% tests passed, 0 tests failed out of 48

Total Test time (real) =   8.74 sec
```

## CI Evidence

Pending. Record the push commit and GitHub Actions run after the M123 implementation/evidence changes are pushed.

## Remaining Gaps Before Executable FaultController Work

Before moving from passive transition ordering into executable FaultController or scheduler behavior, remaining gaps include:

1. Define and implement a full transition-table evaluator only after thresholds and audit fields are specified.
2. Define threshold and temporal policy for transient versus persistent faults.
3. Define separate execution DTOs for isolation, reconnect, abort, and review-required outcomes.
4. Define audit records for any future executable action.
5. Define scheduler ordering evidence for any future executable action before it controls runtime behavior.
6. Keep real adapter health integration separate until mock-only passive transition boundaries remain stable.
7. Add CI evidence and avoid GoldenSuite release/merge claims until required gate evidence exists.

## Decision

M123 confirms that passive transition creation can occur before mock exchange scheduling without altering scheduling, replay, or audit results. The next safe step is to roll up M121-M123 or continue with design-only executable action audit schema requirements before any scheduler control implementation.

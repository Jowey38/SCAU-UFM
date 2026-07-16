# M127 Passive Action Audit Ordering Evidence

## Scope

M127 adds ordering evidence that passive FaultController action-audit record creation can occur before a mock scheduler exchange step without changing exchange scheduling, replay, or audit results.

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
→ run_mock_coupling_scheduler_step(...)
```

M127 is not an executable FaultController, scheduler control boundary, adapter boundary, release-gate action, or runtime evidence writer. The mock scheduler step does not consume the audit record to isolate, reconnect, abort, skip, reorder, or alter exchange behavior.

## Implemented Test Boundary

Focused test target:

- `test_coupling_fault_controller_passive_action_audit_ordering`

Test file:

- `tests/unit/coupling/test_coupling_fault_controller_passive_action_audit_ordering.cpp`

The test constructs two identical coupling states:

1. A control path that runs `run_mock_coupling_scheduler_step(...)` directly.
2. An audited path that first derives a `review_required` passive action-audit record, then runs the same `run_mock_coupling_scheduler_step(...)`.

The comparison asserts that audit record creation does not change:

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

It also confirms the audit record itself remains non-controlling:

- `action_kind = operator_review`;
- `reason = review_required_only`;
- `scheduler_control_enabled = false`;
- `runtime_action_requested = false`;
- `runtime_action_executed = false`;
- `isolation_executed = false`;
- `reconnect_executed = false`;
- `abort_executed = false`;
- `adapter_call_executed = false`;
- `release_gate_action_executed = false`.

## Confirmed Negative Boundary

M127 does not add:

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
- mutation of coupling state from passive audit records;
- native SWMM process, handle, or C API calls;
- native D-Flow FM / BMI process, handle, or API calls;
- protocol release-gate actions such as `BLOCK_MERGE` or `BLOCK_RELEASE`;
- runtime evidence writing;
- adapter-owned fault policy.

The audit record remains ordering evidence for a passive DTO seam, not executable scheduler control or runtime evidence writing.

## Local Evidence

Focused build and CTest:

```text
cmake --build --preset windows-msvc --target test_coupling_fault_controller_passive_action_audit_ordering
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_fault_controller_passive_action_audit_ordering$"
```

Result:

```text
Test project H:/githubcode/SCAU-UFM/build/windows-msvc
    Start 41: test_coupling_fault_controller_passive_action_audit_ordering
1/1 Test #41: test_coupling_fault_controller_passive_action_audit_ordering ...   Passed    0.27 sec

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   0.49 sec
```

Full local Debug validation:

```text
cmake --build --preset windows-msvc
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
100% tests passed, 0 tests failed out of 50

Total Test time (real) =   8.89 sec
```

## CI Evidence

Pending. Record the push commit and GitHub Actions run after the M127 implementation/evidence changes are pushed.

## Remaining Gaps Before Executable FaultController Work

Before moving from passive audit ordering into executable FaultController or scheduler behavior, remaining gaps include:

1. Roll up M125-M127 passive action-audit work.
2. Define and implement a full transition-table evaluator only after thresholds and audit fields are specified.
3. Define threshold and temporal policy for transient versus persistent faults.
4. Define separate execution DTOs for isolation, reconnect, abort, and review-required outcomes.
5. Define scheduler ordering evidence for any future executable action before it controls runtime behavior.
6. Keep real adapter health integration separate until mock-only passive audit boundaries remain stable.
7. Add CI evidence and avoid GoldenSuite release/merge claims until required gate evidence exists.

## Decision

M127 confirms that passive action-audit record creation can occur before mock exchange scheduling without altering scheduling, replay, or audit results. The next safe step is to roll up M125-M127 or continue with design-only executable action outcome requirements before any scheduler control implementation.

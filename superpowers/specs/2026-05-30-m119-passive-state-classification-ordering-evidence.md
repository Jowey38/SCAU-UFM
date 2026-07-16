# M119 Passive State Classification Ordering Evidence

## Scope

M119 adds ordering evidence that passive FaultController state classification can occur before a mock scheduler exchange step without changing exchange scheduling, replay, or audit results.

The passive chain remains label-only and non-controlling:

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

M119 is not an executable FaultController or scheduler control boundary. The mock scheduler step does not consume the classification to isolate, reconnect, abort, skip, reorder, or alter exchange behavior.

## Implemented Test Boundary

Focused test target:

- `test_coupling_fault_controller_passive_state_ordering`

Test file:

- `tests/unit/coupling/test_coupling_fault_controller_passive_state_ordering.cpp`

The test constructs two identical coupling states:

1. A control path that runs `run_mock_coupling_scheduler_step(...)` directly.
2. A classified path that first derives a `review_required` passive state classification, then runs the same `run_mock_coupling_scheduler_step(...)`.

The comparison asserts that classification does not change:

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

It also confirms the classification itself remains non-controlling:

- `state = review_required`;
- `scheduler_control_enabled = false`;
- `isolation_enabled = false`;
- `reconnect_enabled = false`;
- `abort_enabled = false`.

## Confirmed Negative Boundary

M119 does not add:

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

The classification remains ordering evidence for a label-only seam, not executable scheduler control.

## Local Evidence

Focused build:

```text
cmake --build --preset windows-msvc --target test_coupling_fault_controller_passive_state_ordering
```

Result:

```text
scau_coupling_core.vcxproj -> H:\githubcode\SCAU-UFM\build\windows-msvc\libs\coupling\core\Debug\scau_coupling_core.lib
Building Custom Rule H:/githubcode/SCAU-UFM/tests/unit/coupling/CMakeLists.txt
test_coupling_fault_controller_passive_state_ordering.cpp
test_coupling_fault_controller_passive_state_ordering.vcxproj -> H:\githubcode\SCAU-UFM\build\windows-msvc\tests\unit\coupling\Debug\test_coupling_fault_controller_passive_state_ordering.exe
```

Focused CTest:

```text
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_fault_controller_passive_state_ordering$"
```

Result:

```text
Test project H:/githubcode/SCAU-UFM/build/windows-msvc
    Start 37: test_coupling_fault_controller_passive_state_ordering
1/1 Test #37: test_coupling_fault_controller_passive_state_ordering ...   Passed    0.24 sec

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   0.44 sec
```

Full local Debug validation:

```text
cmake --build --preset windows-msvc
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
100% tests passed, 0 tests failed out of 46

Total Test time (real) =   8.19 sec
```

## CI Evidence

Pending. Record the push commit and GitHub Actions run after the M119 implementation/evidence changes are pushed.

## Remaining Gaps Before Executable FaultController Work

Before moving from passive state classification ordering into executable FaultController or scheduler behavior, remaining gaps include:

1. Define an explicit executable transition table.
2. Define threshold and temporal policy for transient versus persistent faults.
3. Define separate execution DTOs for isolation, reconnect, abort, and review-required outcomes.
4. Define audit records for any future executable action.
5. Define scheduler ordering evidence for any future executable action before it controls runtime behavior.
6. Keep real adapter health integration separate until mock-only passive classification boundaries remain stable.
7. Add CI evidence and avoid GoldenSuite release/merge claims until required gate evidence exists.

## Decision

M119 confirms that passive state classification can occur before mock exchange scheduling without altering scheduling, replay, or audit results. The next safe step is to roll up M117-M119 or continue with design-only executable transition requirements before any scheduler control implementation.

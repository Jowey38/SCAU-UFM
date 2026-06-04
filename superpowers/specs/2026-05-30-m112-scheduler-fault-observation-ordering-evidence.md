# M112 Scheduler Fault Observation Ordering Evidence

## Scope

M112 adds local ordering evidence that a proposed FaultController action can be observed before a mock scheduler exchange step without changing exchange scheduling, replay, or audit results.

The observed chain remains passive:

```text
EngineReport
→ classify_engine_health(...)
→ aggregate_engine_health(...)
→ make_fault_controller_diagnostic(...)
→ propose_fault_controller_action(...)
→ observe_mock_scheduler_fault_action(...)
→ run_mock_coupling_scheduler_step(...)
```

M112 is still not an executable FaultController or scheduler action boundary. The scheduler step does not consume the observation to isolate, reconnect, abort, skip, reorder, or alter exchange behavior.

## Implemented Test Boundary

Focused test target:

- `test_coupling_mock_scheduler_fault_observation_ordering`

Test file:

- `tests/unit/coupling/test_coupling_mock_scheduler_fault_observation_ordering.cpp`

The test constructs two identical coupling states:

1. A control path that runs the mock coupling scheduler step directly.
2. An observed path that first derives a `review_required` proposed FaultController action, observes it through `observe_mock_scheduler_fault_action(...)`, then runs the same mock coupling scheduler step.

The comparison asserts that observation does not change:

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

It also confirms the observation itself remains non-executing:

- `observed_review_required = true`
- `executed_isolation = false`
- `executed_reconnect = false`
- `executed_abort = false`

## Confirmed Negative Boundary

M112 does not add:

- FaultController state machine;
- scheduler consumption of fault observations as control actions;
- runtime isolation execution;
- runtime reconnect execution;
- runtime abort execution;
- scheduler process control;
- skipped exchange scheduling;
- reordered request/arbitration/replay/audit behavior;
- mutation of coupling state from fault observation;
- real SWMM C API integration;
- real D-Flow FM / BMI integration;
- protocol release-gate actions such as `BLOCK_MERGE` or `BLOCK_RELEASE`;
- runtime evidence writing.

The observation is evidence for ordering compatibility only, not evidence for executable fault handling.

## Local Evidence

Focused build:

```text
cmake --build --preset windows-msvc --target test_coupling_mock_scheduler_fault_observation_ordering
```

Result:

```text
scau_coupling_core.vcxproj -> H:\githubcode\SCAU-UFM\build\windows-msvc\libs\coupling\core\Debug\scau_coupling_core.lib
Building Custom Rule H:/githubcode/SCAU-UFM/tests/unit/coupling/CMakeLists.txt
test_coupling_mock_scheduler_fault_observation_ordering.cpp
test_coupling_mock_scheduler_fault_observation_ordering.vcxproj -> H:\githubcode\SCAU-UFM\build\windows-msvc\tests\unit\coupling\Debug\test_coupling_mock_scheduler_fault_observation_ordering.exe
```

Focused CTest:

```text
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_mock_scheduler_fault_observation_ordering$"
```

Result:

```text
Test project H:/githubcode/SCAU-UFM/build/windows-msvc
    Start 33: test_coupling_mock_scheduler_fault_observation_ordering
1/1 Test #33: test_coupling_mock_scheduler_fault_observation_ordering ...   Passed    0.26 sec

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
100% tests passed, 0 tests failed out of 42

Total Test time (real) =   7.72 sec
```

## CI Evidence

Pending. Record the push commit and GitHub Actions run after the M112 implementation/evidence changes are pushed.

## Remaining Gaps Before Executable FaultController Work

Before moving from observation ordering evidence into executable FaultController or scheduler behavior, remaining gaps include:

1. Define an explicit FaultController state machine and transition table.
2. Define threshold and temporal policy for transient versus persistent faults.
3. Define action-consumption DTOs and tests separately from observation DTOs.
4. Define how isolation, reconnect, abort, and review-required outcomes are audited.
5. Keep real adapter health integration separate until mock-only ordering boundaries remain stable.
6. Add CI evidence and avoid GoldenSuite release/merge claims until required gate evidence exists.

## Decision

M112 confirms that observing a proposed fault action can occur before mock exchange scheduling without altering scheduling, replay, or audit results. The next safe step should remain non-executing, such as a rollup for M111-M112 or a passive action-consumption design document before any implementation of scheduler control.

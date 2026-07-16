# M111 Mock Scheduler Fault Observation Boundary Evidence

## Scope

M111 adds a non-executing mock scheduler observation boundary for proposed FaultController actions.

The passive chain is now observable by the mock scheduler layer as data only:

```text
EngineReport
→ classify_engine_health(...)
→ aggregate_engine_health(...)
→ make_fault_controller_diagnostic(...)
→ propose_fault_controller_action(...)
→ observe_mock_scheduler_fault_action(...)
```

This stage does not execute isolation, reconnect, abort, process control, release-gate behavior, or real adapter behavior.

## Implemented Core Boundary

`CouplingLib core` owns:

- `MockCouplingSchedulerFaultObservation`
- `observe_mock_scheduler_fault_action(...)`

Observation behavior:

- preserves the input `FaultControllerProposedAction`;
- copies the proposed action state into `observed_state`;
- reports `observed_review_required = true` only when the proposed action state is `review_required`;
- always keeps execution result flags false:
  - `executed_isolation = false`
  - `executed_reconnect = false`
  - `executed_abort = false`

## Confirmed Negative Boundary

M111 does not add:

- FaultController state machine;
- threshold or temporal policy;
- runtime isolation execution;
- runtime reconnect execution;
- runtime abort execution;
- scheduler process control;
- scheduler lifecycle control;
- mutation of coupling state from fault observations;
- native SWMM process, handle, or C API calls;
- native D-Flow FM / BMI process, handle, or API calls;
- protocol release-gate actions such as `BLOCK_MERGE` or `BLOCK_RELEASE`;
- runtime evidence writing;
- adapter-owned fault policy.

The observation boundary is a DTO seam only. It allows later scheduler ordering tests to observe proposed fault actions without claiming that the scheduler can act on them.

## Tests

Focused unit tests were added under `tests/unit/coupling/test_coupling_mock_scheduler_fault_observation.cpp`.

Asserted invariants:

- `continue_run` actions are observed without execution;
- `review_required` actions are observed without execution;
- diagnostic and health fields are preserved through the observation record;
- even if an input action contains execution flags, the observation does not promote them into executed isolation, reconnect, or abort results.

The test target is registered in `tests/unit/coupling/CMakeLists.txt` as `test_coupling_mock_scheduler_fault_observation`.

## Local Evidence

Focused build:

```text
cmake --build --preset windows-msvc --target test_coupling_mock_scheduler_fault_observation
```

Result:

```text
state.cpp
scau_coupling_core.vcxproj -> H:\githubcode\SCAU-UFM\build\windows-msvc\libs\coupling\core\Debug\scau_coupling_core.lib
Building Custom Rule H:/githubcode/SCAU-UFM/tests/unit/coupling/CMakeLists.txt
test_coupling_mock_scheduler_fault_observation.cpp
test_coupling_mock_scheduler_fault_observation.vcxproj -> H:\githubcode\SCAU-UFM\build\windows-msvc\tests\unit\coupling\Debug\test_coupling_mock_scheduler_fault_observation.exe
```

Focused CTest:

```text
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_mock_scheduler_fault_observation$"
```

Result:

```text
Test project H:/githubcode/SCAU-UFM/build/windows-msvc
    Start 32: test_coupling_mock_scheduler_fault_observation
1/1 Test #32: test_coupling_mock_scheduler_fault_observation ...   Passed    0.26 sec

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   0.45 sec
```

Full local Debug validation:

```text
cmake --build --preset windows-msvc
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
100% tests passed, 0 tests failed out of 41

Total Test time (real) =   8.68 sec
```

## CI Evidence

Pending. Record the push commit and GitHub Actions run after the M111 implementation/evidence changes are pushed.

## Remaining Gaps Before Executable FaultController Work

Before moving from scheduler observation into executable FaultController or scheduler behavior, remaining gaps include:

1. Define an explicit FaultController state machine and transition table.
2. Define threshold and temporal policy for transient versus persistent faults.
3. Define scheduler ordering evidence for health collection, diagnostic derivation, policy proposal, observation, request, arbitration, accept, replay, and audit phases.
4. Define separate action-consumption boundaries for isolation, reconnect, abort, and review-required behavior.
5. Keep real adapter health integration separate until mock-only observation and ordering boundaries are stable.
6. Add CI evidence and avoid GoldenSuite release/merge claims until required gate evidence exists.

## Decision

M111 establishes a safe non-executing scheduler observation boundary. The next safe step is to add ordering evidence that the mock scheduler can observe proposed fault actions without changing exchange scheduling, replay, audit, isolation, reconnect, or abort behavior.

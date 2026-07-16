# M115 Passive Action Consumption Boundary Evidence

## Scope

M115 implements the passive action-consumption boundary designed in M114.

The passive mock scheduler fault chain is now:

```text
EngineReport
→ classify_engine_health(...)
→ aggregate_engine_health(...)
→ make_fault_controller_diagnostic(...)
→ propose_fault_controller_action(...)
→ observe_mock_scheduler_fault_action(...)
→ consume_mock_scheduler_fault_action(...)
→ run_mock_coupling_scheduler_step(...)
```

This stage consumes proposed fault actions as records only. It does not execute isolation, reconnect, abort, scheduler process control, release-gate behavior, or real adapter behavior.

## Implemented Core Boundary

`CouplingLib core` owns:

- `MockCouplingSchedulerFaultConsumption`
- `consume_mock_scheduler_fault_action(...)`

Consumption behavior:

- preserves the input `MockCouplingSchedulerFaultObservation`;
- copies the observed action state into `consumed_state`;
- reports `review_required_consumed = true` only when the observed state is `review_required`;
- keeps passive stage permissions true:
  - `exchange_scheduling_allowed = true`
  - `replay_allowed = true`
  - `audit_allowed = true`
- always keeps execution result flags false:
  - `executed_isolation = false`
  - `executed_reconnect = false`
  - `executed_abort = false`

## Tests

Focused tests were added under:

- `tests/unit/coupling/test_coupling_mock_scheduler_fault_consumption.cpp`
- `tests/unit/coupling/test_coupling_mock_scheduler_fault_consumption_ordering.cpp`

Registered targets:

- `test_coupling_mock_scheduler_fault_consumption`
- `test_coupling_mock_scheduler_fault_consumption_ordering`

Asserted consumption invariants:

- `continue_run` observations are consumed without execution;
- `review_required` observations are consumed without execution;
- diagnostic, health, action, and observation fields are preserved;
- consumption does not promote proposed or observed execution flags into executed isolation, reconnect, or abort results;
- passive scheduling, replay, and audit permissions remain true.

Asserted ordering invariants:

- consuming a `review_required` fault action before the mock scheduler step does not change shared exchange decisions;
- consumption does not change replayed cell volume;
- consumption does not change replayed cell deficit account;
- consumption does not change replayed cell depth;
- consumption does not change system-mass audit baseline total;
- consumption does not change system-mass audit current total;
- consumption does not change system-mass audit residual;
- consumption does not change system-mass audit conservation flag.

## Confirmed Negative Boundary

M115 does not add:

- FaultController state machine;
- running, isolated, reconnecting, recovered, or failed states;
- state transition table;
- consecutive-failure counters;
- timeout, recovery, or persistence thresholds;
- runtime isolation execution;
- runtime reconnect execution;
- runtime abort execution;
- scheduler process control;
- scheduler lifecycle control;
- skipped exchange scheduling;
- reordered request/arbitration/replay/audit behavior;
- mutation of coupling state from fault consumption;
- native SWMM process, handle, or C API calls;
- native D-Flow FM / BMI process, handle, or API calls;
- protocol release-gate actions such as `BLOCK_MERGE` or `BLOCK_RELEASE`;
- runtime evidence writing;
- adapter-owned fault policy.

The consumption boundary is still a DTO/reporting seam only. It is not executable scheduler control.

## Local Evidence

Focused build:

```text
cmake --build --preset windows-msvc --target test_coupling_mock_scheduler_fault_consumption test_coupling_mock_scheduler_fault_consumption_ordering
```

Result:

```text
state.cpp
scau_coupling_core.vcxproj -> H:\githubcode\SCAU-UFM\build\windows-msvc\libs\coupling\core\Debug\scau_coupling_core.lib
Building Custom Rule H:/githubcode/SCAU-UFM/tests/unit/coupling/CMakeLists.txt
test_coupling_mock_scheduler_fault_consumption.cpp
test_coupling_mock_scheduler_fault_consumption.vcxproj -> H:\githubcode\SCAU-UFM\build\windows-msvc\tests\unit\coupling\Debug\test_coupling_mock_scheduler_fault_consumption.exe
scau_coupling_core.vcxproj -> H:\githubcode\SCAU-UFM\build\windows-msvc\libs\coupling\core\Debug\scau_coupling_core.lib
Building Custom Rule H:/githubcode/SCAU-UFM/tests/unit/coupling/CMakeLists.txt
test_coupling_mock_scheduler_fault_consumption_ordering.cpp
test_coupling_mock_scheduler_fault_consumption_ordering.vcxproj -> H:\githubcode\SCAU-UFM\build\windows-msvc\tests\unit\coupling\Debug\test_coupling_mock_scheduler_fault_consumption_ordering.exe
```

Focused CTest:

```text
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^(test_coupling_mock_scheduler_fault_consumption|test_coupling_mock_scheduler_fault_consumption_ordering)$"
```

Result:

```text
Test project H:/githubcode/SCAU-UFM/build/windows-msvc
    Start 34: test_coupling_mock_scheduler_fault_consumption
1/2 Test #34: test_coupling_mock_scheduler_fault_consumption ............   Passed    0.26 sec
    Start 35: test_coupling_mock_scheduler_fault_consumption_ordering
2/2 Test #35: test_coupling_mock_scheduler_fault_consumption_ordering ...   Passed    0.38 sec

100% tests passed, 0 tests failed out of 2

Total Test time (real) =   0.87 sec
```

Full local Debug validation:

```text
cmake --build --preset windows-msvc
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
100% tests passed, 0 tests failed out of 44

Total Test time (real) =   9.96 sec
```

## CI Evidence

Pending. Record the push commit and GitHub Actions run after the M115 implementation/evidence changes are pushed.

## Remaining Gaps Before Executable FaultController Work

Before moving from passive consumption into executable FaultController or scheduler behavior, remaining gaps include:

1. Define an explicit FaultController state machine and transition table.
2. Define threshold and temporal policy for transient versus persistent faults.
3. Define separate execution DTOs for isolation, reconnect, abort, and review-required outcomes.
4. Define audit records for any future executable action.
5. Define scheduler ordering evidence for any future executable action before it controls runtime behavior.
6. Keep real adapter health integration separate until mock-only passive consumption boundaries remain stable.
7. Add CI evidence and avoid GoldenSuite release/merge claims until required gate evidence exists.

## Decision

M115 establishes passive action consumption as a reporting seam and proves it does not alter scheduling, replay, or audit behavior. The next safe step is a rollup for M114-M115 or a design document for executable FaultController state-machine requirements before any scheduler control implementation.

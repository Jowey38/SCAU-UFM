# M118 Passive State Classification Boundary Evidence

## Scope

M118 implements a passive state-classification helper for future FaultController state labels.

The passive mock scheduler fault chain can now be labeled as state data:

```text
EngineReport
→ classify_engine_health(...)
→ aggregate_engine_health(...)
→ make_fault_controller_diagnostic(...)
→ propose_fault_controller_action(...)
→ observe_mock_scheduler_fault_action(...)
→ consume_mock_scheduler_fault_action(...)
→ classify_fault_controller_passive_state(...)
```

This stage labels passive state only. It does not control the scheduler, execute isolation, reconnect, abort, process control, release-gate behavior, or real adapter behavior.

## Implemented Core Boundary

`CouplingLib core` owns:

- `FaultControllerPassiveStateLabel`
- `FaultControllerPassiveStateClassification`
- `classify_fault_controller_passive_state(...)`

Classification behavior:

- preserves the input `MockCouplingSchedulerFaultConsumption`;
- maps `FaultControllerProposedActionState::continue_run` to `FaultControllerPassiveStateLabel::running`;
- maps `FaultControllerProposedActionState::review_required` to `FaultControllerPassiveStateLabel::review_required`;
- always keeps control flags false:
  - `scheduler_control_enabled = false`
  - `isolation_enabled = false`
  - `reconnect_enabled = false`
  - `abort_enabled = false`

## Tests

Focused tests were added under:

- `tests/unit/coupling/test_coupling_fault_controller_passive_state.cpp`

Registered target:

- `test_coupling_fault_controller_passive_state`

Asserted invariants:

- continue-run consumption classifies as `running` without control;
- review-required consumption classifies as `review_required` without control;
- classification preserves the consumption record;
- classification does not promote consumed execution flags into scheduler control, isolation, reconnect, or abort capability.

## Confirmed Negative Boundary

M118 does not add:

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

The classifier is a state-labeling seam only. It is not executable scheduler control.

## Local Evidence

Focused build:

```text
cmake --build --preset windows-msvc --target test_coupling_fault_controller_passive_state
```

Result:

```text
state.cpp
scau_coupling_core.vcxproj -> H:\githubcode\SCAU-UFM\build\windows-msvc\libs\coupling\core\Debug\scau_coupling_core.lib
Building Custom Rule H:/githubcode/SCAU-UFM/tests/unit/coupling/CMakeLists.txt
test_coupling_fault_controller_passive_state.cpp
test_coupling_fault_controller_passive_state.vcxproj -> H:\githubcode\SCAU-UFM\build\windows-msvc\tests\unit\coupling\Debug\test_coupling_fault_controller_passive_state.exe
```

Focused CTest:

```text
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_fault_controller_passive_state$"
```

Result:

```text
Test project H:/githubcode/SCAU-UFM/build/windows-msvc
    Start 36: test_coupling_fault_controller_passive_state
1/1 Test #36: test_coupling_fault_controller_passive_state ...   Passed    0.58 sec

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   0.83 sec
```

Full local Debug validation:

```text
cmake --build --preset windows-msvc
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
100% tests passed, 0 tests failed out of 45

Total Test time (real) =  11.45 sec
```

## CI Evidence

Pending. Record the push commit and GitHub Actions run after the M118 implementation/evidence changes are pushed.

## Remaining Gaps Before Executable FaultController Work

Before moving from passive state classification into executable FaultController or scheduler behavior, remaining gaps include:

1. Define an explicit executable transition table.
2. Define threshold and temporal policy for transient versus persistent faults.
3. Define separate execution DTOs for isolation, reconnect, abort, and review-required outcomes.
4. Define audit records for any future executable action.
5. Define scheduler ordering evidence for any future executable action before it controls runtime behavior.
6. Keep real adapter health integration separate until mock-only passive classification boundaries remain stable.
7. Add CI evidence and avoid GoldenSuite release/merge claims until required gate evidence exists.

## Decision

M118 establishes passive state classification as a label-only seam and proves it does not enable scheduler control, isolation, reconnect, or abort behavior. The next safe step is to add ordering evidence that passive state classification before mock scheduling still leaves scheduling, replay, and audit unchanged, or to roll up M117-M118.

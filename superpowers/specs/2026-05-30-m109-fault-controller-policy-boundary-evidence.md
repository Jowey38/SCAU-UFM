# M109 FaultController Policy Boundary Evidence

## Scope

M109 adds the first minimal core FaultController policy boundary after the passive health-to-diagnostic chain.

The implemented chain is now:

```text
EngineReport
→ classify_engine_health(...)
→ aggregate_engine_health(...)
→ make_fault_controller_diagnostic(...)
→ propose_fault_controller_action(...)
```

This stage is intentionally non-executing. It proposes a policy-facing action record only; it does not execute isolation, reconnect, abort, scheduler control, release-gate actions, or real adapter behavior.

## Implemented Core Boundary

`CouplingLib core` owns:

- `FaultControllerProposedActionState`
- `FaultControllerProposedAction`
- `propose_fault_controller_action(...)`

Policy mapping behavior:

- `FaultControllerDiagnosticStatus::nominal` maps to `FaultControllerProposedActionState::continue_run`.
- `FaultControllerDiagnosticStatus::fault_detected` maps to `FaultControllerProposedActionState::review_required`.
- The input `FaultControllerDiagnostic` is preserved on the proposed action record.
- Execution flags remain false:
  - `execute_isolation = false`
  - `execute_reconnect = false`
  - `execute_abort = false`

## Confirmed Negative Boundary

M109 does not add:

- FaultController state machine;
- consecutive-failure counters, thresholds, timeout policy, or recovery policy;
- runtime isolation execution;
- runtime reconnect execution;
- runtime abort execution;
- scheduler process control;
- scheduler consumption of `FaultControllerProposedAction`;
- protocol release-gate actions such as `BLOCK_MERGE` or `BLOCK_RELEASE`;
- runtime evidence writing;
- real SWMM C API integration;
- real D-Flow FM / BMI integration;
- third-party SWMM, D-Flow FM, or BMI headers/libraries;
- adapter-owned fault policy.

This remains a project-owned, same-process, mock-stage core policy DTO boundary.

## Local Evidence

Focused build:

```text
cmake --build --preset windows-msvc --target test_coupling_fault_controller_policy
```

Result:

```text
scau_coupling_core.vcxproj -> H:\githubcode\SCAU-UFM\build\windows-msvc\libs\coupling\core\Debug\scau_coupling_core.lib
test_coupling_fault_controller_policy.cpp
test_coupling_fault_controller_policy.vcxproj -> H:\githubcode\SCAU-UFM\build\windows-msvc\tests\unit\coupling\Debug\test_coupling_fault_controller_policy.exe
```

Focused CTest:

```text
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_fault_controller_policy$"
```

Result:

```text
Test project H:/githubcode/SCAU-UFM/build/windows-msvc
    Start 31: test_coupling_fault_controller_policy
1/1 Test #31: test_coupling_fault_controller_policy ...   Passed    0.28 sec

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   0.47 sec
```

Additional regression after fixing pre-existing replay fixture direction omissions exposed by full CTest:

```text
cmake --build --preset windows-msvc --target test_coupling_core_state test_coupling_shared_replay
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^(test_coupling_core_state|test_coupling_shared_replay)$"
```

Result:

```text
Test project H:/githubcode/SCAU-UFM/build/windows-msvc
    Start 23: test_coupling_core_state
1/2 Test #23: test_coupling_core_state .........   Passed    0.25 sec
    Start 26: test_coupling_shared_replay
2/2 Test #26: test_coupling_shared_replay ......   Passed    0.19 sec

100% tests passed, 0 tests failed out of 2

Total Test time (real) =   0.60 sec
```

Full local Debug validation:

```text
cmake --build --preset windows-msvc
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
100% tests passed, 0 tests failed out of 40

Total Test time (real) =   7.05 sec
```

## CI Evidence

Pending. Record the push commit and GitHub Actions run after the M109 implementation/evidence changes are pushed.

## Remaining Gaps Before Executable FaultController Work

Before moving from proposed non-executing policy records into executable FaultController behavior, remaining gaps include:

1. No FaultController state machine exists.
2. No threshold or temporal policy exists.
3. No runtime isolation, reconnect, or abort action exists.
4. No scheduler integration exists.
5. No real adapter health integration exists.
6. CI and GoldenSuite evidence remain incomplete.

## Decision

M109 establishes the minimal policy boundary recommended by M108 while preserving the negative execution boundary. The next safe step can either roll up M107-M109 into a passive diagnostic-to-policy stage or add a similarly non-executing scheduler observation boundary, but it should still avoid runtime execution until a state machine, transition policy, and evidence path are explicitly designed and tested.

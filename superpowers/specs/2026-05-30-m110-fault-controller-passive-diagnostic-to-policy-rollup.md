# M110 FaultController Passive Diagnostic To Policy Rollup

## Scope

M110 rolls up the passive FaultController diagnostic-to-policy work completed from M107 through M109.

The current project-owned passive chain is:

```text
EngineReport
→ classify_engine_health(...)
→ aggregate_engine_health(...)
→ make_fault_controller_diagnostic(...)
→ propose_fault_controller_action(...)
```

M110 is a rollup only. It does not add new production behavior, scheduler behavior, runtime execution, real adapter integration, CI evidence, GoldenSuite evidence, or release/merge readiness claims.

## Covered Milestones

- M107: added passive core FaultController diagnostic mapping from `EngineHealthAggregate` to `FaultControllerDiagnostic`.
- M108: consolidated the M103-M107 health-to-diagnostic chain and documented the remaining gaps before executable FaultController work.
- M109: added a minimal non-executing core policy helper from `FaultControllerDiagnostic` to `FaultControllerProposedAction`.

## Current Passive Diagnostic-To-Policy Chain

### 1. Health aggregate to passive diagnostic

`CouplingLib core` owns:

- `FaultControllerDiagnosticStatus`
- `FaultControllerDiagnostic`
- `make_fault_controller_diagnostic(...)`

Diagnostic mapping behavior:

- `EngineHealthAggregateStatus::all_healthy` maps to `FaultControllerDiagnosticStatus::nominal`.
- `EngineHealthAggregateStatus::any_unhealthy` maps to `FaultControllerDiagnosticStatus::fault_detected`.
- The input `EngineHealthAggregate` is preserved.
- Current diagnostic action flags remain passive:
  - `should_isolate = false`
  - `should_reconnect = false`
  - `should_abort = false`

### 2. Passive diagnostic to proposed policy action

`CouplingLib core` owns:

- `FaultControllerProposedActionState`
- `FaultControllerProposedAction`
- `propose_fault_controller_action(...)`

Policy proposal behavior:

- `FaultControllerDiagnosticStatus::nominal` maps to `FaultControllerProposedActionState::continue_run`.
- `FaultControllerDiagnosticStatus::fault_detected` maps to `FaultControllerProposedActionState::review_required`.
- The input `FaultControllerDiagnostic` is preserved.
- Execution flags remain false:
  - `execute_isolation = false`
  - `execute_reconnect = false`
  - `execute_abort = false`

## Confirmed Negative Boundary

M107-M109 do not add:

- FaultController state machine;
- running, isolated, reconnecting, recovered, or failed states;
- state transition table;
- consecutive-failure counters;
- timeout, recovery, or persistence thresholds;
- runtime isolation execution;
- runtime reconnect execution;
- runtime abort execution;
- scheduler process control;
- scheduler consumption of `FaultControllerDiagnostic` or `FaultControllerProposedAction`;
- runtime ordering evidence for health-report collection relative to request, arbitration, accept, replay, or audit phases;
- release-gate behavior;
- `BLOCK_MERGE` or `BLOCK_RELEASE` behavior;
- runtime evidence writing;
- real SWMM C API integration;
- real D-Flow FM / BMI integration;
- third-party SWMM, D-Flow FM, or BMI headers/libraries;
- adapter-owned fault policy.

The stage remains project-owned, same-process, mock-stage, side-effect-free, and third-party-free.

## Evidence Summary

### M107 focused diagnostic evidence

Focused target:

```text
cmake --build --preset windows-msvc --target test_coupling_fault_controller_diagnostic
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_fault_controller_diagnostic$"
```

Recorded result in M107:

```text
1/1 Test #30: test_coupling_fault_controller_diagnostic ...   Passed    0.39 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) =   0.59 sec
```

### M109 focused policy evidence

Focused target:

```text
cmake --build --preset windows-msvc --target test_coupling_fault_controller_policy
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_fault_controller_policy$"
```

Recorded result in M109:

```text
Test project H:/githubcode/SCAU-UFM/build/windows-msvc
    Start 31: test_coupling_fault_controller_policy
1/1 Test #31: test_coupling_fault_controller_policy ...   Passed    0.28 sec

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   0.47 sec
```

### Latest full local Debug evidence

Latest full local validation recorded in M109:

```text
cmake --build --preset windows-msvc
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
100% tests passed, 0 tests failed out of 40

Total Test time (real) =   7.05 sec
```

## Review Evidence

Manual review for the M107-M109 chain confirms:

- `make_fault_controller_diagnostic(...)` is side-effect free;
- `propose_fault_controller_action(...)` is side-effect free;
- passive diagnostic flags are not promoted into execution flags;
- `fault_detected` currently maps only to `review_required`, not abort/isolate/reconnect execution;
- no scheduler control, adapter control, native engine control, release gate, or runtime evidence writer is introduced.

## CI Evidence

Pending. Record the push commit and GitHub Actions run after the M107-M110 implementation/evidence changes are pushed.

## Remaining Gaps Before Executable FaultController Work

Before moving from passive diagnostic-to-policy records into executable FaultController behavior, remaining gaps include:

1. Define an explicit FaultController state machine and transition table.
2. Define threshold and temporal policy for transient versus persistent faults.
3. Define how isolation, reconnect, abort, and review-required actions are represented, consumed, audited, and tested.
4. Define scheduler observation and ordering boundaries before any scheduler action boundary.
5. Define real adapter health integration only after mock-only boundaries remain stable.
6. Add CI evidence and avoid GoldenSuite release/merge claims until the project has required gate evidence.

## Decision

M107-M109 form a coherent passive diagnostic-to-policy stage. The next safe implementation step is a non-executing scheduler observation boundary that can observe `FaultControllerProposedAction` values and report them without controlling runtime isolation, reconnect, abort, process lifecycle, release gates, or real adapters.

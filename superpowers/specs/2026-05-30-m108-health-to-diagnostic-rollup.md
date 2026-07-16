# M108 Health To Diagnostic Rollup

## Scope

M108 rolls up the health-to-diagnostic work completed from M103 through M107.

This rollup consolidates evidence for the current project-owned health boundary chain:

```text
EngineReport
→ classify_engine_health(...)
→ aggregate_engine_health(...)
→ make_fault_controller_diagnostic(...)
```

M108 does not add new production behavior, real adapter integration, runtime scheduler behavior, FaultController execution, or release-gate evidence.

## Covered Milestones

- M103: added project-owned mock `EngineReport` DTO and mock drainage/river report helpers.
- M104: added side-effect-free core health classification from `EngineReport` to `EngineHealthDiagnostic`.
- M105: added side-effect-free core health aggregation from multiple diagnostics to `EngineHealthAggregate`.
- M107: added passive core FaultController diagnostic mapping from `EngineHealthAggregate` to `FaultControllerDiagnostic`.

## Current Health-To-Diagnostic Chain

### 1. EngineReport production

`CouplingLib core` owns the report DTO:

- `EngineReport::healthy`
- `EngineReport::engine_id`
- `EngineReport::error_code`
- `EngineReport::elapsed_time`

Mock report producer helpers exist in the drainage and river boundaries:

- `make_swmm_engine_report(const MockSwmmEngine&)`
- `make_swmm_engine_report(const SwmmEngineError&)`
- `make_dflowfm_engine_report(const MockDFlowFMEngine&)`
- `make_dflowfm_engine_report(const DFlowFMEngineError&)`

These helpers convert mock initialized/error state into `EngineReport` values only. They do not implement fault policy.

### 2. Classification

`CouplingLib core` owns:

- `EngineHealthStatus`
- `EngineHealthDiagnostic`
- `classify_engine_health(...)`

Classification behavior:

- rejects non-finite or negative report elapsed time;
- maps `healthy=true` to `EngineHealthStatus::healthy`;
- maps `healthy=false` to `EngineHealthStatus::unhealthy`;
- preserves `engine_id`, `error_code`, and `elapsed_time`.

### 3. Aggregation

`CouplingLib core` owns:

- `EngineHealthAggregateStatus`
- `EngineHealthAggregate`
- `aggregate_engine_health(...)`

Aggregation behavior:

- counts total diagnostics;
- counts unhealthy diagnostics;
- returns `all_healthy` when no unhealthy diagnostics are present;
- returns `any_unhealthy` when at least one unhealthy diagnostic is present;
- treats an empty diagnostic set as `all_healthy` with zero counts.

### 4. Passive FaultController diagnostic mapping

`CouplingLib core` owns:

- `FaultControllerDiagnosticStatus`
- `FaultControllerDiagnostic`
- `make_fault_controller_diagnostic(...)`

Diagnostic mapping behavior:

- maps `all_healthy` aggregate status to `nominal`;
- maps `any_unhealthy` aggregate status to `fault_detected`;
- preserves the input `EngineHealthAggregate`;
- always keeps current action flags passive:
  - `should_isolate = false`;
  - `should_reconnect = false`;
  - `should_abort = false`.

## Confirmed Negative Boundary

M103-M107 do not add:

- FaultController state machine;
- fault isolation or reconnect execution;
- threshold policy;
- consecutive-failure state;
- runtime abort behavior;
- scheduler process control;
- protocol states such as `REVIEW_REQUIRED`, `BLOCK_MERGE`, or `BLOCK_RELEASE`;
- release-gate behavior;
- runtime evidence writing;
- real SWMM C API integration;
- real D-Flow FM / BMI integration;
- third-party SWMM, D-Flow FM, or BMI headers/libraries;
- real `SWMMAdapter` or `DFlowFMAdapter` implementation;
- adapter-owned fault policy.

The stage remains mock-only, project-owned, passive, and third-party-free. It is evidence for DTO flow and passive diagnostic derivation, not evidence that a runtime FaultController is implemented.

## Local Evidence

Focused tests used during the stage:

```text
cmake --build --preset windows-msvc --target test_coupling_engine_report
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_engine_report$"
```

Latest focused engine report result:

```text
1/1 Test #28: test_coupling_engine_report ......   Passed    0.27 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) =   0.45 sec
```

```text
cmake --build --preset windows-msvc --target test_coupling_engine_health
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_engine_health$"
```

Latest focused engine health result:

```text
1/1 Test #29: test_coupling_engine_health ......   Passed    0.22 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) =   0.32 sec
```

```text
cmake --build --preset windows-msvc --target test_coupling_fault_controller_diagnostic
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_fault_controller_diagnostic$"
```

Latest focused passive diagnostic result:

```text
1/1 Test #30: test_coupling_fault_controller_diagnostic ...   Passed    0.39 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) =   0.59 sec
```

Latest full build and serial Debug CTest:

```text
cmake --build --preset windows-msvc
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
39/39 Test #39: test_coupling_system_mass_audit ..... Passed
100% tests passed, 0 tests failed out of 39
Total Test time (real) =   7.75 sec
```

## Review Evidence

Manual read-only reviews completed during this stage:

- M103 review passed project-owned mock health reports and confirmed no FaultController policy or real adapter health integration.
- M104 review passed side-effect-free health classification and confirmed no scheduler action or FaultController policy.
- M105 review passed side-effect-free health aggregation and confirmed no thresholds, isolation, reconnect, scheduler action, or fault policy.
- M107 review passed passive FaultController diagnostic mapping and confirmed no state machine, threshold policy, isolation, reconnect, runtime abort, scheduler action, or real adapter health integration.

Review caveat:

- M108 itself is a rollup only. It does not replace CI evidence and does not claim merge/release readiness.

## CI Evidence

Pending. Record the push commit and GitHub Actions run after the M103-M108 implementation/evidence changes are pushed.

## Remaining Gaps Before Executable FaultController Work

Before moving from passive diagnostic derivation into executable FaultController behavior, remaining gaps include:

1. No FaultController state machine exists.
   - No state such as running, isolated, reconnecting, recovered, or failed is implemented.
   - No state transition table exists.

2. No threshold or temporal policy exists.
   - There is no consecutive-failure counter.
   - There is no timeout or recovery threshold.
   - There is no policy for distinguishing transient faults from persistent faults.

3. No runtime action exists.
   - `should_isolate`, `should_reconnect`, and `should_abort` are currently always false.
   - No isolation, reconnect, abort, review-required, block-merge, or block-release action is emitted or consumed.

4. No scheduler integration exists.
   - The mock scheduler does not consume `FaultControllerDiagnostic`.
   - No runtime ordering evidence exists for health-report collection relative to request, arbitration, accept, replay, or audit phases.

5. No real adapter health integration exists.
   - Reports are mock-only.
   - No native SWMM engine, D-Flow FM/BMI engine, process, resource, or ABI health call is made.

6. CI and GoldenSuite evidence remain incomplete.
   - Local CTest evidence exists.
   - CI evidence is pending.
   - GoldenSuite release/merge gate evidence is not claimed by this stage.

## Decision

M103-M107 form a coherent passive health-to-diagnostic DTO stage. The next safe implementation step should remain below real third-party adapter integration and can move toward a minimal core FaultController policy boundary, such as a non-executing policy helper that maps `FaultControllerDiagnosticStatus::fault_detected` to a proposed action record.

That next step should still avoid executing isolation, reconnect, abort, scheduler process control, and release-gate actions until the corresponding protocol and evidence paths are explicitly implemented.

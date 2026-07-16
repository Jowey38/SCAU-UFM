# M107 Fault Controller Diagnostic Boundary Evidence

## Scope

M107 adds a minimal passive FaultController diagnostic boundary in CouplingLib core.

New core enum/DTO/helper:

- `scau::coupling::core::FaultControllerDiagnosticStatus`
- `scau::coupling::core::FaultControllerDiagnostic`
- `scau::coupling::core::make_fault_controller_diagnostic(...)`

The helper converts an `EngineHealthAggregate` into a passive diagnostic record. It does not execute isolation, reconnect, abort, scheduler actions, or release-gate actions.

## Implemented Behavior

`make_fault_controller_diagnostic(...)`:

- consumes an `EngineHealthAggregate`;
- maps `EngineHealthAggregateStatus::all_healthy` to `FaultControllerDiagnosticStatus::nominal`;
- maps `EngineHealthAggregateStatus::any_unhealthy` to `FaultControllerDiagnosticStatus::fault_detected`;
- preserves the input health aggregate in the diagnostic;
- always returns passive action flags:
  - `should_isolate = false`;
  - `should_reconnect = false`;
  - `should_abort = false`.

## Boundary

M107 is a passive diagnostic boundary only.

It does not add:

- FaultController state machine;
- fault isolation or reconnect behavior;
- scheduler process control;
- runtime abort behavior;
- `REVIEW_REQUIRED`, `BLOCK_MERGE`, `BLOCK_RELEASE`, or release-gate behavior;
- threshold policy or consecutive-failure state;
- runtime evidence writing;
- real SWMM C API integration;
- real D-Flow FM / BMI integration;
- third-party SWMM, D-Flow FM, or BMI headers/libraries;
- real `SWMMAdapter` or `DFlowFMAdapter` implementation;
- adapter-owned fault policy.

The diagnostic name intentionally prepares the eventual FaultController seam while keeping the current implementation passive and side-effect free.

## Tests

Focused unit tests were added under `tests/unit/coupling/test_coupling_fault_controller_diagnostic.cpp`.

Asserted invariants:

- all-healthy aggregates produce `FaultControllerDiagnosticStatus::nominal`;
- any-unhealthy aggregates produce `FaultControllerDiagnosticStatus::fault_detected`;
- health aggregate fields are preserved;
- empty all-healthy aggregates remain nominal;
- `should_isolate`, `should_reconnect`, and `should_abort` remain false for all current diagnostics.

The new test target is registered in `tests/unit/coupling/CMakeLists.txt` as `test_coupling_fault_controller_diagnostic`.

## Local Evidence

Focused build and test:

```text
cmake --build --preset windows-msvc --target test_coupling_fault_controller_diagnostic
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_fault_controller_diagnostic$"
```

Result:

```text
1/1 Test #30: test_coupling_fault_controller_diagnostic ...   Passed    0.39 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) =   0.59 sec
```

Full build and serial Debug CTest:

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

## CI Evidence

Pending. Record the push commit and GitHub Actions run after this implementation/evidence change is pushed.

## Review Evidence

Manual read-only review completed after local validation.

Confirmed by review:

- `make_fault_controller_diagnostic(...)` is side-effect free and only returns a `FaultControllerDiagnostic`;
- the helper maps aggregate health to `nominal` or `fault_detected` and preserves the input `EngineHealthAggregate`;
- `should_isolate`, `should_reconnect`, and `should_abort` are always false in the current boundary;
- no FaultController state machine, threshold policy, consecutive-failure state, isolation, reconnect, runtime abort, scheduler action, protocol state, merge/release gate, or runtime evidence writing is introduced;
- no SWMM C API, D-Flow FM/BMI API, native handle, third-party header, or third-party library is introduced;
- drainage and river mock boundaries remain report producers only and do not own diagnostic policy.

M107 is therefore a passive core diagnostic mapping only, not FaultController execution or real adapter health integration evidence.

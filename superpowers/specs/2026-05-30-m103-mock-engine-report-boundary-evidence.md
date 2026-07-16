# M103 Mock Engine Report Boundary Evidence

## Scope

M103 adds a project-owned mock `EngineReport` boundary for drainage/SWMM and river/D-Flow FM mock engines.

New core DTO:

- `scau::coupling::core::EngineReport`

New mock boundary helpers:

- `scau::coupling::drainage::make_swmm_engine_report(...)`
- `scau::coupling::river::make_dflowfm_engine_report(...)`

This prepares a health-report seam for future FaultController work without implementing FaultController behavior.

## Implemented Behavior

`EngineReport` currently carries the minimum mock health fields:

- `healthy`
- `engine_id`
- `error_code`
- `elapsed_time`

SWMM mock report behavior:

- initialized `MockSwmmEngine` reports `healthy = true`;
- uninitialized `MockSwmmEngine` reports `healthy = false` and `error_code = "swmm_mock_not_initialized"`;
- elapsed time is copied from the mock engine;
- `SwmmEngineError` converts to an unhealthy report preserving `engine_id` and `error_code`.

D-Flow FM mock report behavior:

- initialized `MockDFlowFMEngine` reports `healthy = true`;
- uninitialized `MockDFlowFMEngine` reports `healthy = false` and `error_code = "dflowfm_mock_not_initialized"`;
- elapsed time is copied from the mock engine;
- `DFlowFMEngineError` converts to an unhealthy report preserving `engine_id` and `error_code`.

## Boundary

M103 is a mock health-report boundary only.

It does not add:

- FaultController implementation;
- fault isolation or reconnect behavior;
- `REVIEW_REQUIRED`, `BLOCK_MERGE`, `BLOCK_RELEASE`, or release-gate behavior;
- real SWMM C API integration;
- real D-Flow FM / BMI integration;
- third-party SWMM, D-Flow FM, or BMI headers/libraries;
- real `SWMMAdapter` or `DFlowFMAdapter` implementation;
- process control, wall-clock scheduling, threading, async execution, or external runtime orchestration;
- adapter-owned `Q_limit` or `V_limit` definitions;
- adapter-owned `mass_deficit_account` behavior;
- adapter-owned rollback / replay orchestration;
- runtime evidence writing.

The report remains a project-owned DTO and should be consumed by a future core FaultController boundary, not by drainage/river adapters directly implementing fault policy.

## Tests

Focused unit tests were added under `tests/unit/coupling/test_coupling_engine_report.cpp`.

Asserted invariants:

- SWMM mock report reflects initialized/uninitialized health state;
- SWMM mock report preserves elapsed time;
- D-Flow FM mock report reflects initialized/uninitialized health state;
- D-Flow FM mock report preserves elapsed time;
- `SwmmEngineError` converts to unhealthy report with preserved `engine_id` and `error_code`;
- `DFlowFMEngineError` converts to unhealthy report with preserved `engine_id` and `error_code`.

The new test target is registered in `tests/unit/coupling/CMakeLists.txt` as `test_coupling_engine_report`.

## Local Evidence

Focused build and test:

```text
cmake --build --preset windows-msvc --target test_coupling_engine_report
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_engine_report$"
```

Result:

```text
1/1 Test #28: test_coupling_engine_report ......   Passed    0.27 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) =   0.45 sec
```

Full build and serial Debug CTest:

```text
cmake --build --preset windows-msvc
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
37/37 Test #37: test_coupling_system_mass_audit ..... Passed
100% tests passed, 0 tests failed out of 37
Total Test time (real) =   7.68 sec
```

## CI Evidence

Pending. Record the push commit and GitHub Actions run after this implementation/evidence change is pushed.

## Review Evidence

Manual read-only review completed after local validation.

Confirmed by review:

- `EngineReport` is a project-owned core DTO with mock health fields only;
- SWMM and D-Flow FM report helpers convert mock initialized state and mock adapter errors into `EngineReport` values;
- report helpers do not implement FaultController policy, isolation, reconnect, review states, merge/release gates, or runtime evidence writing;
- no SWMM C API, D-Flow FM/BMI API, native handle, third-party header, or third-party library is introduced;
- drainage and river helpers only produce report DTOs and do not own fault policy.

M103 is therefore a mock health-report boundary only, not FaultController implementation or real third-party adapter health integration evidence.

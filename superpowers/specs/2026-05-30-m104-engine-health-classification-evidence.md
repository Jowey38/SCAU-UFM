# M104 Engine Health Classification Evidence

## Scope

M104 adds the first core-owned health classification helper for project-owned `EngineReport` values.

New core enum/DTO/helper:

- `scau::coupling::core::EngineHealthStatus`
- `scau::coupling::core::EngineHealthDiagnostic`
- `scau::coupling::core::classify_engine_health(...)`

This is a minimal classification seam for future FaultController work. It does not implement FaultController policy or runtime actions.

## Implemented Behavior

`classify_engine_health(...)`:

- consumes an `EngineReport`;
- rejects non-finite or negative `elapsed_time`;
- maps `report.healthy = true` to `EngineHealthStatus::healthy`;
- maps `report.healthy = false` to `EngineHealthStatus::unhealthy`;
- preserves `engine_id`, `error_code`, and `elapsed_time` in the returned diagnostic.

The helper is deliberately side-effect free.

## Boundary

M104 is a core health-classification helper only.

It does not add:

- FaultController implementation;
- fault isolation or reconnect behavior;
- scheduler process control;
- runtime abort behavior;
- `REVIEW_REQUIRED`, `BLOCK_MERGE`, `BLOCK_RELEASE`, or release-gate behavior;
- real SWMM C API integration;
- real D-Flow FM / BMI integration;
- third-party SWMM, D-Flow FM, or BMI headers/libraries;
- real `SWMMAdapter` or `DFlowFMAdapter` implementation;
- adapter-owned fault policy;
- runtime evidence writing.

Drainage and river mock boundaries still only produce `EngineReport` values. The core helper classifies them without owning any adapter lifecycle or reconnect semantics.

## Tests

Focused unit tests were added under `tests/unit/coupling/test_coupling_engine_health.cpp`.

Asserted invariants:

- healthy reports classify as `EngineHealthStatus::healthy`;
- unhealthy reports classify as `EngineHealthStatus::unhealthy`;
- `engine_id`, `error_code`, and `elapsed_time` are preserved;
- non-finite elapsed time is rejected;
- negative elapsed time is rejected.

The new test target is registered in `tests/unit/coupling/CMakeLists.txt` as `test_coupling_engine_health`.

## Local Evidence

Focused build and test:

```text
cmake --build --preset windows-msvc --target test_coupling_engine_health
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_engine_health$"
```

Result:

```text
1/1 Test #29: test_coupling_engine_health ......   Passed    0.42 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) =   0.83 sec
```

Full build and serial Debug CTest:

```text
cmake --build --preset windows-msvc
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
38/38 Test #38: test_coupling_system_mass_audit ..... Passed
100% tests passed, 0 tests failed out of 38
Total Test time (real) =   7.64 sec
```

## CI Evidence

Pending. Record the push commit and GitHub Actions run after this implementation/evidence change is pushed.

## Review Evidence

Manual read-only review completed after local validation.

Confirmed by review:

- `classify_engine_health(...)` is side-effect free and only returns an `EngineHealthDiagnostic`;
- classification maps the report health bit to `healthy` / `unhealthy` and preserves report metadata;
- invalid elapsed time is rejected before classification;
- no FaultController policy, isolation, reconnect, runtime abort, scheduler action, protocol state, merge/release gate, or runtime evidence writing is introduced;
- no SWMM C API, D-Flow FM/BMI API, native handle, third-party header, or third-party library is introduced;
- drainage and river mock boundaries remain report producers only and do not own health policy.

M104 is therefore a side-effect-free core health classification helper only, not FaultController or real adapter health integration evidence.

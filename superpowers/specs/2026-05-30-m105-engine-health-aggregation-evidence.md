# M105 Engine Health Aggregation Evidence

## Scope

M105 adds a minimal core-owned health aggregation helper for multiple `EngineHealthDiagnostic` values.

New core enum/DTO/helper:

- `scau::coupling::core::EngineHealthAggregateStatus`
- `scau::coupling::core::EngineHealthAggregate`
- `scau::coupling::core::aggregate_engine_health(...)`

This is a passive aggregation seam for future FaultController work. It does not execute fault policy or runtime actions.

## Implemented Behavior

`aggregate_engine_health(...)`:

- consumes a vector of `EngineHealthDiagnostic` values;
- counts total reports;
- counts diagnostics with `EngineHealthStatus::unhealthy`;
- returns `EngineHealthAggregateStatus::all_healthy` when no unhealthy diagnostics are present;
- returns `EngineHealthAggregateStatus::any_unhealthy` when at least one unhealthy diagnostic is present;
- treats an empty diagnostic set as `all_healthy` with zero report and unhealthy counts.

The helper is side-effect free.

## Boundary

M105 is a core health aggregation helper only.

It does not add:

- FaultController implementation;
- fault isolation or reconnect behavior;
- scheduler process control;
- runtime abort behavior;
- `REVIEW_REQUIRED`, `BLOCK_MERGE`, `BLOCK_RELEASE`, or release-gate behavior;
- threshold policy or consecutive-failure state;
- real SWMM C API integration;
- real D-Flow FM / BMI integration;
- third-party SWMM, D-Flow FM, or BMI headers/libraries;
- real `SWMMAdapter` or `DFlowFMAdapter` implementation;
- adapter-owned fault policy;
- runtime evidence writing.

Drainage and river mock boundaries still only produce reports. M104 classifies those reports, and M105 only aggregates the resulting diagnostics.

## Tests

Focused unit tests were added to `tests/unit/coupling/test_coupling_engine_health.cpp`.

Asserted invariants:

- all-healthy diagnostics aggregate to `all_healthy`;
- any unhealthy diagnostic changes the aggregate status to `any_unhealthy`;
- total report count is preserved;
- unhealthy count is preserved;
- empty diagnostic sets aggregate to `all_healthy` with zero counts.

The focused test target remains `test_coupling_engine_health`.

## Local Evidence

Focused build and test:

```text
cmake --build --preset windows-msvc --target test_coupling_engine_health
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_engine_health$"
```

Result:

```text
1/1 Test #29: test_coupling_engine_health ......   Passed    0.22 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) =   0.32 sec
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
Total Test time (real) =   8.51 sec
```

## CI Evidence

Pending. Record the push commit and GitHub Actions run after this implementation/evidence change is pushed.

## Review Evidence

Manual read-only review completed after local validation.

Confirmed by review:

- `aggregate_engine_health(...)` is side-effect free and only returns an `EngineHealthAggregate`;
- aggregation only counts total diagnostics and diagnostics marked `unhealthy`;
- aggregate status is limited to `all_healthy` and `any_unhealthy`;
- empty diagnostic sets are treated as `all_healthy` with zero counts;
- no FaultController policy, threshold policy, consecutive-failure state, isolation, reconnect, runtime abort, scheduler action, protocol state, merge/release gate, or runtime evidence writing is introduced;
- no SWMM C API, D-Flow FM/BMI API, native handle, third-party header, or third-party library is introduced;
- drainage and river mock boundaries remain report producers only and do not own health aggregation or fault policy.

M105 is therefore a side-effect-free core health aggregation helper only, not FaultController or real adapter health integration evidence.

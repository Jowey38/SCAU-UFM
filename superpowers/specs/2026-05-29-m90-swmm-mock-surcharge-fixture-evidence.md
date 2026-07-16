# M90 SWMM Mock Surcharge Fixture Evidence

## Scope

M90 extends the M89 SWMM mock fixture API with deterministic surcharge state configuration for adapter-boundary tests.

Added mock-only API:

- `MockSwmmEngine::set_node_surcharge_fixture(int node_id, bool surcharged)`

This API is intentionally mock-only. It does not model SWMM hydraulics and does not imply real SWMM native API support.

## Behavior

`set_node_surcharge_fixture(...)`:

- requires initialized mock engine;
- requires non-negative `node_id`;
- stores deterministic value returned by `is_surcharged(node_id)`.

Unset surcharge state remains `false` after initialization.

`initialize(...)` and `finalize()` clear surcharge fixture state.

## Boundary

M90 is a mock fixture API addition only.

It does not add:

- SWMM headers, C API calls, native libraries, or `extern/swmm5/` dependencies;
- real `SWMMAdapter` implementation;
- D-Flow FM / BMI headers, native libraries, or `extern/dflowfm*` dependencies;
- real `DFlowFMAdapter` implementation;
- new `Q_limit` or `V_limit` definitions outside `CouplingLib core`;
- new `mass_deficit_account` behavior outside `CouplingLib core`;
- shared arbitration, rollback, replay, runtime scheduler, or runtime evidence writing;
- GoldenSuite, release-gate, `REVIEW_REQUIRED`, `BLOCK_MERGE`, or `BLOCK_RELEASE` execution.

## Tests

Focused unit tests remain under `tests/unit/coupling/test_coupling_adapter_boundaries.cpp`.

New/extended asserted invariants:

- SWMM mock surcharge fixture is returned deterministically by `is_surcharged(...)`;
- unset SWMM mock surcharge state defaults to `false` after initialization;
- SWMM mock surcharge fixture setter rejects negative node IDs;
- SWMM mock surcharge fixture setter requires initialized engine.

Existing M77-M89 invariants remain covered.

## Local Evidence

Focused build and test:

```text
cmake --build --preset windows-msvc --target test_coupling_adapter_boundaries
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_adapter_boundaries$"
```

Result:

```text
1/1 Test #24: test_coupling_adapter_boundaries ...   Passed    0.21 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) =   0.31 sec
```

Full serial Debug CTest:

```text
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
33/33 Test #33: test_coupling_system_mass_audit ..... Passed
100% tests passed, 0 tests failed out of 33
Total Test time (real) =   5.61 sec
```

## CI Evidence

Pending. Record the push commit and GitHub Actions run after this implementation/evidence change is pushed.

## Review Evidence

Manual boundary review found the M90 change is limited to:

- adding one SWMM mock surcharge fixture setter API;
- preserving lifecycle and node ID validation at the fixture boundary;
- extending unit tests for deterministic surcharge state and invalid fixture input rejection;
- this evidence record.

The change does not include or link third-party engine code, does not expose native SWMM/D-Flow FM/BMI types to `CouplingLib core`, and does not implement shared arbitration, flow limits, deficit ledger behavior, rollback/replay, scheduler process control, or release/merge gate handling outside their normative owners.

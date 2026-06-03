# M82 Mock Adapter Lifecycle Getter Guard Evidence

## Scope

M82 tightens the M77 mock-only drainage and river adapter boundaries by requiring mock getter calls to occur only while the mock engine is initialized.

Updated lifecycle behavior:

- `MockSwmmEngine::get_node_head(...)` throws before `initialize(...)` and after `finalize()`;
- `MockSwmmEngine::get_node_lateral_inflow(...)` throws before `initialize(...)` and after `finalize()`;
- `MockSwmmEngine::get_link_flow(...)` throws before `initialize(...)` and after `finalize()`;
- `MockSwmmEngine::is_surcharged(...)` throws before `initialize(...)` and after `finalize()`;
- `MockDFlowFMEngine::get_value(...)` throws before `initialize(...)` and after `finalize()`.

This prevents uninitialized mock engines from silently returning default `0.0` values that could be mistaken for valid 1D engine state in adapter-boundary tests.

## Boundary

M82 is a mock lifecycle validation tightening only.

It does not add:

- SWMM headers, C API calls, native libraries, or `extern/swmm5/` dependencies;
- D-Flow FM / BMI headers, native libraries, or `extern/dflowfm*` dependencies;
- real `SWMMAdapter` or `DFlowFMAdapter` implementation;
- new `Q_limit` or `V_limit` definitions outside `CouplingLib core`;
- new `mass_deficit_account` behavior outside `CouplingLib core`;
- shared arbitration, rollback, replay, runtime scheduler, or runtime evidence writing;
- GoldenSuite, release-gate, `REVIEW_REQUIRED`, `BLOCK_MERGE`, or `BLOCK_RELEASE` execution.

## Tests

Focused unit tests remain under `tests/unit/coupling/test_coupling_adapter_boundaries.cpp`.

New asserted invariants:

- SWMM mock getters throw before initialization;
- SWMM mock getters throw after finalization;
- D-Flow FM mock getter throws before initialization;
- D-Flow FM mock getter throws after finalization.

Existing M77/M78/M79/M80/M81 invariants remain covered.

## Local Evidence

Focused build and test:

```text
cmake --build --preset windows-msvc --target test_coupling_adapter_boundaries
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_adapter_boundaries$"
```

Result:

```text
1/1 Test #24: test_coupling_adapter_boundaries ...   Passed    0.34 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) =   0.44 sec
```

Full serial Debug CTest:

```text
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
33/33 Test #33: test_coupling_system_mass_audit ..... Passed
100% tests passed, 0 tests failed out of 33
Total Test time (real) =   5.98 sec
```

## CI Evidence

Pending. Record the push commit and GitHub Actions run after this implementation/evidence change is pushed.

## Review Evidence

Manual boundary review found the M82 change is limited to:

- adding initialized-state checks to mock SWMM and D-Flow FM getter methods;
- adding unit tests for pre-initialize and post-finalize getter rejection;
- this evidence record.

The change does not include or link third-party engine code, does not expose native SWMM/D-Flow FM/BMI types to `CouplingLib core`, and does not implement shared arbitration, flow limits, deficit ledger behavior, rollback/replay, scheduler process control, or release/merge gate handling outside their normative owners.

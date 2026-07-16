# M81 Mock Adapter Complete Decision Field Validation Evidence

## Scope

M81 tightens the M77/M79/M80 mock-only drainage and river adapter accept helpers by validating every numeric field in `core::ExchangeDecision`, not only the flow fields directly written to the mock engine.

Updated validation for both accept helpers:

- `q_granted` must be finite and non-negative;
- `v_granted` must be finite and non-negative;
- `q_repay` must be finite and non-negative;
- `v_repay` must be finite and non-negative;
- `v_unmet` must be finite and non-negative.

This prevents malformed but partially unused `ExchangeDecision` fields from crossing the adapter boundary and keeps the mock seams aligned with coupling-core exchange decision invariants.

## Boundary

M81 is a mock boundary validation tightening only.

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

- SWMM accept helper rejects non-finite `v_granted`;
- SWMM accept helper rejects negative `v_unmet`;
- D-Flow FM accept helper rejects non-finite `v_granted`;
- D-Flow FM accept helper rejects negative `v_unmet`.

Existing M77/M78/M79/M80 invariants remain covered.

## Local Evidence

Focused build and test:

```text
cmake --build --preset windows-msvc --target test_coupling_adapter_boundaries
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_adapter_boundaries$"
```

Result:

```text
1/1 Test #24: test_coupling_adapter_boundaries ...   Passed    0.22 sec
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
Total Test time (real) =   5.96 sec
```

## CI Evidence

Pending. Record the push commit and GitHub Actions run after this implementation/evidence change is pushed.

## Review Evidence

Manual boundary review found the M81 change is limited to:

- extending mock drainage and river accept helper validation to all `ExchangeDecision` numeric fields;
- adding unit tests for non-finite and negative volume-field rejection;
- this evidence record.

The change does not include or link third-party engine code, does not expose native SWMM/D-Flow FM/BMI types to `CouplingLib core`, and does not implement shared arbitration, flow limits, deficit ledger behavior, rollback/replay, scheduler process control, or release/merge gate handling outside their normative owners.

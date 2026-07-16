# M78 River Exchange Point Finite Validation Evidence

## Scope

M78 tightens the M77 mock-only river adapter boundary by requiring `RiverExchangePoint` numeric metadata used by future arbitration to be finite.

Updated validation:

- `priority_weight` must be finite and greater than zero;
- `exchange_width` must be finite and non-negative.

This prevents adapter-reported `+inf` or `NaN` metadata from entering later shared-cell arbitration, priority normalization, audit, replay, or GoldenSuite evidence chains.

## Boundary

M78 is a validation tightening only.

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

- `RiverExchangePoint` rejects `priority_weight == +inf`;
- `RiverExchangePoint` rejects `priority_weight == NaN`;
- `RiverExchangePoint` rejects `exchange_width == +inf`;
- `RiverExchangePoint` rejects `exchange_width == NaN`.

Existing M77 invariants remain covered:

- SWMM and D-Flow FM mock boundaries are third-party-free;
- both mock boundaries use `core::ExchangeRequest` / `core::ExchangeDecision` DTOs;
- SWMM and D-Flow FM mocks remain independent;
- positive finite `priority_weight` is preserved;
- non-positive `priority_weight` is rejected;
- engine interfaces do not carry core runtime semantics.

## Local Evidence

Focused build and test:

```text
cmake --build --preset windows-msvc --target test_coupling_adapter_boundaries
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_adapter_boundaries$"
```

Result:

```text
1/1 Test #24: test_coupling_adapter_boundaries ...   Passed    0.18 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) =   0.27 sec
```

Full serial Debug CTest:

```text
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
33/33 Test #33: test_coupling_system_mass_audit ..... Passed
100% tests passed, 0 tests failed out of 33
Total Test time (real) =   5.93 sec
```

## CI Evidence

Pending. Record the push commit and GitHub Actions run after this implementation/evidence change is pushed.

## Review Evidence

Manual boundary review found the M78 change is limited to:

- adding finite checks to `is_valid_river_exchange_point(...)`;
- adding unit tests for non-finite `priority_weight` and `exchange_width` rejection;
- this evidence record.

The change does not include or link third-party engine code, does not expose native SWMM/D-Flow FM/BMI types to `CouplingLib core`, and does not implement shared arbitration, flow limits, deficit ledger behavior, rollback/replay, scheduler process control, or release/merge gate handling outside their normative owners.

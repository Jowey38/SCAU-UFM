# M86 River Exchange Point Metadata Boundary Evidence

## Scope

M86 tightens `RiverExchangePoint` mock-boundary metadata validation before future river adapter output can enter core arbitration, audit, replay, or GoldenSuite evidence chains.

Updated validation:

- `exchange_type` must be one of:
  - `bank_overtop`
  - `lateral_weir`
  - `gate_outlet`
  - `culvert_link`
- `priority_weight` must remain finite and greater than zero;
- `exchange_width` must remain finite and non-negative;
- `neighbor_2d_edges` must be non-empty;
- every `neighbor_2d_edges` entry must be non-negative.

This aligns the mock river exchange-point seam with the main spec §7.5 minimum metadata contract.

## Boundary

M86 is a mock metadata validation tightening only.

It does not add:

- D-Flow FM / BMI headers, native libraries, or `extern/dflowfm*` dependencies;
- real `DFlowFMAdapter` implementation;
- SWMM headers, C API calls, native libraries, or `extern/swmm5/` dependencies;
- real `SWMMAdapter` implementation;
- new `Q_limit` or `V_limit` definitions outside `CouplingLib core`;
- new `mass_deficit_account` behavior outside `CouplingLib core`;
- shared arbitration, rollback, replay, runtime scheduler, or runtime evidence writing;
- GoldenSuite, release-gate, `REVIEW_REQUIRED`, `BLOCK_MERGE`, or `BLOCK_RELEASE` execution.

## Tests

Focused unit tests remain under `tests/unit/coupling/test_coupling_adapter_boundaries.cpp`.

New asserted invariants:

- unsupported `exchange_type` is rejected;
- empty `neighbor_2d_edges` is rejected;
- negative `neighbor_2d_edges` entry is rejected;
- all supported `exchange_type` values are accepted when the rest of the metadata is valid.

Existing M77/M78/M79/M80/M81/M82/M83/M84/M85 invariants remain covered.

## Local Evidence

Focused build and test:

```text
cmake --build --preset windows-msvc --target test_coupling_adapter_boundaries
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_adapter_boundaries$"
```

Result:

```text
1/1 Test #24: test_coupling_adapter_boundaries ...   Passed    0.26 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) =   0.56 sec
```

Full serial Debug CTest:

```text
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
33/33 Test #33: test_coupling_system_mass_audit ..... Passed
100% tests passed, 0 tests failed out of 33
Total Test time (real) =   7.28 sec
```

## CI Evidence

Pending. Record the push commit and GitHub Actions run after this implementation/evidence change is pushed.

## Review Evidence

Manual boundary review found the M86 change is limited to:

- extending `RiverExchangePoint` validation for supported `exchange_type` values;
- requiring non-empty, non-negative `neighbor_2d_edges` metadata;
- adding unit tests for rejected invalid metadata and accepted supported exchange types;
- this evidence record.

The change does not include or link third-party engine code, does not expose native SWMM/D-Flow FM/BMI types to `CouplingLib core`, and does not implement shared arbitration, flow limits, deficit ledger behavior, rollback/replay, scheduler process control, or release/merge gate handling outside their normative owners.

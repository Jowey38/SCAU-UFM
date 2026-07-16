# M87 River Exchange Point ID And Crest Validation Evidence

## Scope

M87 completes the current `RiverExchangePoint` mock-boundary metadata validation by checking the remaining identifier and elevation fields that can enter future river adapter arbitration, audit, replay, and GoldenSuite evidence chains.

Updated validation:

- `branch_id` must be non-negative;
- `link_id` must be non-negative;
- `crest_level` must be finite;
- existing M86 metadata validation remains active:
  - supported `exchange_type` only;
  - finite positive `priority_weight`;
  - finite non-negative `exchange_width`;
  - non-empty `neighbor_2d_edges`;
  - non-negative `neighbor_2d_edges` entries.

## Boundary

M87 is a mock metadata validation tightening only.

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

- negative `branch_id` is rejected;
- negative `link_id` is rejected;
- non-finite `crest_level` is rejected.

Existing M77-M86 invariants remain covered.

## Local Evidence

Focused build and test:

```text
cmake --build --preset windows-msvc --target test_coupling_adapter_boundaries
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_adapter_boundaries$"
```

Result:

```text
1/1 Test #24: test_coupling_adapter_boundaries ...   Passed    0.17 sec
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
Total Test time (real) =   5.92 sec
```

## CI Evidence

Pending. Record the push commit and GitHub Actions run after this implementation/evidence change is pushed.

## Review Evidence

Manual boundary review found the M87 change is limited to:

- extending `RiverExchangePoint` validation for `branch_id`, `link_id`, and `crest_level`;
- adding unit tests for invalid identifier/elevation metadata;
- this evidence record.

The change does not include or link third-party engine code, does not expose native SWMM/D-Flow FM/BMI types to `CouplingLib core`, and does not implement shared arbitration, flow limits, deficit ledger behavior, rollback/replay, scheduler process control, or release/merge gate handling outside their normative owners.

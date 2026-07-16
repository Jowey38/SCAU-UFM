# M84 D-Flow FM Mock Gate Opening Range Evidence

## Scope

M84 tightens the M83 D-Flow FM mock standard-variable boundary by enforcing the main spec §7.3 value domain for `gate_opening`.

Updated behavior:

- `MockDFlowFMEngine::set_value("gate_opening", ..., value)` accepts finite values in `[0, 1]`;
- `MockDFlowFMEngine::set_value("gate_opening", ..., value)` rejects values below `0`;
- `MockDFlowFMEngine::set_value("gate_opening", ..., value)` rejects values above `1`.

This prevents illegal gate/RTC opening values from entering the river adapter seam through the mock D-Flow FM engine.

## Boundary

M84 is a mock boundary validation tightening only.

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

- `gate_opening == 0.0` is accepted;
- `gate_opening == 1.0` is accepted;
- `gate_opening < 0.0` is rejected;
- `gate_opening > 1.0` is rejected.

Existing M77/M78/M79/M80/M81/M82/M83 invariants remain covered.

## Local Evidence

Focused build and test:

```text
cmake --build --preset windows-msvc --target test_coupling_adapter_boundaries
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_adapter_boundaries$"
```

Result:

```text
1/1 Test #24: test_coupling_adapter_boundaries ...   Passed    0.20 sec
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
Total Test time (real) =   6.02 sec
```

## CI Evidence

Pending. Record the push commit and GitHub Actions run after this implementation/evidence change is pushed.

## Review Evidence

Manual boundary review found the M84 change is limited to:

- adding `gate_opening` value-domain validation to the D-Flow FM mock setter;
- adding unit tests for accepted boundary values and rejected out-of-range values;
- this evidence record.

The change does not include or link third-party engine code, does not expose native SWMM/D-Flow FM/BMI types to `CouplingLib core`, and does not implement shared arbitration, flow limits, deficit ledger behavior, rollback/replay, scheduler process control, or release/merge gate handling outside their normative owners.

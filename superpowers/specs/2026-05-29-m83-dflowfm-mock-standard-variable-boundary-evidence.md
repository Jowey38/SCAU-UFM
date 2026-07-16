# M83 D-Flow FM Mock Standard Variable Boundary Evidence

## Scope

M83 tightens the mock-only D-Flow FM adapter boundary by allowing only the standard BMI-style variable names defined by the main spec §7.3.

Allowed mock variable names:

- `water_level`
- `discharge`
- `lateral_discharge`
- `gate_opening`

Updated behavior:

- `MockDFlowFMEngine::set_value(...)` rejects unknown variable names;
- `MockDFlowFMEngine::get_value(...)` rejects unknown variable names;
- `accept_dflowfm_exchange_decision(...)` rejects unknown variable names through the engine boundary.

This prevents arbitrary strings from entering the river adapter seam as if they were supported D-Flow FM / BMI variables.

## Boundary

M83 is a mock boundary validation tightening only.

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

- D-Flow FM mock accepts `water_level`;
- D-Flow FM mock accepts `discharge`;
- D-Flow FM mock accepts `lateral_discharge`;
- D-Flow FM mock accepts `gate_opening`;
- D-Flow FM mock `set_value(...)` rejects an unknown variable;
- D-Flow FM mock `get_value(...)` rejects an unknown variable;
- D-Flow FM accept helper rejects an unknown variable.

Existing M77/M78/M79/M80/M81/M82 invariants remain covered.

## Local Evidence

Focused build and test:

```text
cmake --build --preset windows-msvc --target test_coupling_adapter_boundaries
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_adapter_boundaries$"
```

Result:

```text
1/1 Test #24: test_coupling_adapter_boundaries ...   Passed    0.28 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) =   0.37 sec
```

Full serial Debug CTest:

```text
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
33/33 Test #33: test_coupling_system_mass_audit ..... Passed
100% tests passed, 0 tests failed out of 33
Total Test time (real) =   6.29 sec
```

## CI Evidence

Pending. Record the push commit and GitHub Actions run after this implementation/evidence change is pushed.

## Review Evidence

Manual boundary review found the M83 change is limited to:

- adding standard variable-name validation to D-Flow FM mock get/set;
- adding unit tests for accepted standard names and rejected unknown names;
- this evidence record.

The change does not include or link third-party engine code, does not expose native SWMM/D-Flow FM/BMI types to `CouplingLib core`, and does not implement shared arbitration, flow limits, deficit ledger behavior, rollback/replay, scheduler process control, or release/merge gate handling outside their normative owners.

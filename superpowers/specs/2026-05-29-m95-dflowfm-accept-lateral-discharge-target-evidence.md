# M95 D-Flow FM Accept Lateral Discharge Target Evidence

## Scope

M95 tightens the mock-only D-Flow FM accept boundary so accepted core exchange decisions can only be written to the project-standard `lateral_discharge` variable.

Updated behavior:

- `accept_dflowfm_exchange_decision(...)` accepts `var_name == "lateral_discharge"`;
- `accept_dflowfm_exchange_decision(...)` rejects other standard state variables such as:
  - `water_level`
  - `discharge`
  - `gate_opening`
- unknown variables continue to be rejected.

This prevents an already-decided exchange flow from being accidentally applied to D-Flow FM state variables that are not exchange-input targets.

## Boundary

M95 is a mock accept-boundary validation tightening only.

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

- D-Flow FM accept helper rejects `water_level` as an accepted exchange target;
- D-Flow FM accept helper rejects `discharge` as an accepted exchange target;
- D-Flow FM accept helper rejects `gate_opening` as an accepted exchange target;
- existing positive accept paths continue to write `lateral_discharge` only.

Existing M77-M94 invariants remain covered.

## Local Evidence

Focused build and test:

```text
cmake --build --preset windows-msvc --target test_coupling_adapter_boundaries
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_adapter_boundaries$"
```

Result:

```text
1/1 Test #24: test_coupling_adapter_boundaries ...   Passed    0.19 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) =   0.29 sec
```

Full serial Debug CTest:

```text
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
33/33 Test #33: test_coupling_system_mass_audit ..... Passed
100% tests passed, 0 tests failed out of 33
Total Test time (real) =   5.95 sec
```

## CI Evidence

Pending. Record the push commit and GitHub Actions run after this implementation/evidence change is pushed.

## Review Evidence

Manual boundary review found the M95 change is limited to:

- adding a D-Flow FM accept-target guard for `lateral_discharge`;
- adding unit tests for rejected non-exchange target variables;
- this evidence record.

The change does not include or link third-party engine code, does not expose native SWMM/D-Flow FM/BMI types to `CouplingLib core`, and does not implement shared arbitration, flow limits, deficit ledger behavior, rollback/replay, scheduler process control, or release/merge gate handling outside their normative owners.

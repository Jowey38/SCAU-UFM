# M91 D-Flow FM Mock Typed Fixture API Evidence

## Scope

M91 adds typed fixture setters to the mock-only D-Flow FM adapter boundary so adapter-boundary tests can configure project-standard D-Flow FM state without scattering string variable names through test setup.

Added mock-only APIs:

- `MockDFlowFMEngine::set_water_level_fixture(int location_id, double water_level)`
- `MockDFlowFMEngine::set_discharge_fixture(int location_id, double discharge)`
- `MockDFlowFMEngine::set_lateral_discharge_fixture(int location_id, double lateral_discharge)`
- `MockDFlowFMEngine::set_gate_opening_fixture(int location_id, double gate_opening)`

These APIs are intentionally mock-only wrappers over the project-standard `IDFlowFMEngine::set_value(...)` boundary. They do not model D-Flow FM hydraulics and do not imply real BMI native variable-name support beyond the project boundary names.

## Behavior

Each typed fixture setter reuses the existing D-Flow FM mock boundary validation:

- initialized mock engine is required;
- non-negative `location_id` is required;
- finite value is required;
- `gate_opening` remains constrained to `[0, 1]`.

The values are still readable through `get_value(...)` using the project-standard names:

- `water_level`
- `discharge`
- `lateral_discharge`
- `gate_opening`

## Boundary

M91 is a mock fixture API addition only.

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

- typed water-level fixture is returned deterministically through `get_value("water_level", ...)`;
- typed discharge fixture is returned deterministically through `get_value("discharge", ...)`;
- typed lateral-discharge fixture is returned deterministically through `get_value("lateral_discharge", ...)`;
- typed gate-opening fixture is returned deterministically through `get_value("gate_opening", ...)`;
- typed fixture setters reuse invalid location, non-finite value, and gate-opening range validation;
- typed fixture setters require initialized engine.

Existing M77-M90 invariants remain covered.

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
Total Test time (real) =   5.56 sec
```

## CI Evidence

Pending. Record the push commit and GitHub Actions run after this implementation/evidence change is pushed.

## Review Evidence

Manual boundary review found the M91 change is limited to:

- adding four D-Flow FM mock typed fixture setter APIs;
- delegating typed setters to the existing project-standard variable validation path;
- adding unit tests for deterministic fixture state and validation reuse;
- this evidence record.

The change does not include or link third-party engine code, does not expose native SWMM/D-Flow FM/BMI types to `CouplingLib core`, and does not implement shared arbitration, flow limits, deficit ledger behavior, rollback/replay, scheduler process control, or release/merge gate handling outside their normative owners.

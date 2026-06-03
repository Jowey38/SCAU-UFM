# M92 Dual Mock Fixture Boundary Scenario Evidence

## Scope

M92 adds a deterministic dual mock adapter-boundary scenario using the M89-M91 fixture APIs. The scenario proves SWMM and D-Flow FM mock states can be configured side by side and can independently accept project-owned core exchange decisions without introducing real third-party engines.

## Scenario

The test configures SWMM mock state:

- node head fixture;
- link flow fixture;
- node surcharge fixture.

The test configures D-Flow FM mock state:

- water level fixture;
- discharge fixture;
- gate opening fixture.

Then it creates project-owned core DTOs:

- `core::ExchangeRequest` for SWMM;
- `core::ExchangeRequest` for D-Flow FM;
- `core::ExchangeDecision` for SWMM;
- `core::ExchangeDecision` for D-Flow FM.

Finally, it applies each accepted decision through the corresponding mock adapter boundary and asserts:

- SWMM fixture state remains readable and deterministic;
- SWMM accepted flow writes only SWMM lateral inflow;
- D-Flow FM fixture state remains readable and deterministic;
- D-Flow FM accepted flow writes only D-Flow FM lateral discharge;
- the two mock engines remain independent.

## Boundary

M92 is a test/evidence addition only.

It does not add:

- SWMM headers, C API calls, native libraries, or `extern/swmm5/` dependencies;
- real `SWMMAdapter` implementation;
- D-Flow FM / BMI headers, native libraries, or `extern/dflowfm*` dependencies;
- real `DFlowFMAdapter` implementation;
- new `Q_limit` or `V_limit` definitions outside `CouplingLib core`;
- new `mass_deficit_account` behavior outside `CouplingLib core`;
- shared arbitration, rollback, replay, runtime scheduler, or runtime evidence writing;
- GoldenSuite, release-gate, `REVIEW_REQUIRED`, `BLOCK_MERGE`, or `BLOCK_RELEASE` execution.

The test deliberately does not implement shared-cell arbitration. It only verifies the mock boundary can hold two independent 1D states and apply already-decided core exchange DTOs.

## Tests

Focused unit tests remain under `tests/unit/coupling/test_coupling_adapter_boundaries.cpp`.

New asserted invariant:

- `DualMockFixtureScenarioAcceptsCoreDecisionsIndependently` configures both mock engines, applies independent core decisions, and verifies deterministic state/accepted-flow results.

Existing M77-M91 invariants remain covered.

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
Total Test time (real) =   0.43 sec
```

Full serial Debug CTest:

```text
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
33/33 Test #33: test_coupling_system_mass_audit ..... Passed
100% tests passed, 0 tests failed out of 33
Total Test time (real) =   5.73 sec
```

## CI Evidence

Pending. Record the push commit and GitHub Actions run after this implementation/evidence change is pushed.

## Review Evidence

Manual boundary review found the M92 change is limited to:

- adding one deterministic dual mock fixture scenario test;
- reusing existing SWMM and D-Flow FM mock fixture APIs;
- reusing project-owned `core::ExchangeRequest` and `core::ExchangeDecision` DTOs;
- this evidence record.

The change does not include or link third-party engine code, does not expose native SWMM/D-Flow FM/BMI types to `CouplingLib core`, and does not implement shared arbitration, flow limits, deficit ledger behavior, rollback/replay, scheduler process control, or release/merge gate handling outside their normative owners.

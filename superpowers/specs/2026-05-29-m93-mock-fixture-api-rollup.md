# M93 Mock Fixture API Rollup

## Scope

M93 rolls up the mock fixture API and deterministic dual mock scenario work performed from M89 through M92.

This rollup is a stage review and evidence consolidation. It does not add new production code, adapter behavior, third-party integration, or runtime scheduler behavior.

## Covered Milestones

- M89: added SWMM mock fixture APIs for deterministic node head and link flow.
- M90: added SWMM mock surcharge fixture API.
- M91: added D-Flow FM mock typed fixture APIs for project-standard state variables.
- M92: added a deterministic dual SWMM + D-Flow FM mock fixture scenario using project-owned core DTOs.

## Current Fixture API State

### SWMM mock fixture API

`MockSwmmEngine` now supports mock-only deterministic fixture setters:

- `set_node_head_fixture(int node_id, double head)`
- `set_link_flow_fixture(int link_id, double q)`
- `set_node_surcharge_fixture(int node_id, bool surcharged)`

These fixture setters:

- are only present on `MockSwmmEngine`, not the `ISwmmEngine` interface;
- require initialized mock engine;
- reject negative IDs;
- reject non-finite numeric fixture values where applicable;
- clear fixture state on `initialize(...)` and `finalize()`.

### D-Flow FM mock typed fixture API

`MockDFlowFMEngine` now supports mock-only typed fixture setters:

- `set_water_level_fixture(int location_id, double water_level)`
- `set_discharge_fixture(int location_id, double discharge)`
- `set_lateral_discharge_fixture(int location_id, double lateral_discharge)`
- `set_gate_opening_fixture(int location_id, double gate_opening)`

These fixture setters:

- are only present on `MockDFlowFMEngine`, not the `IDFlowFMEngine` interface;
- delegate to the existing project-standard `set_value(...)` path;
- therefore reuse initialized-state, non-negative location ID, finite value, standard variable name, and `gate_opening` range validation;
- clear fixture state on `initialize(...)` and `finalize()`.

### Dual mock scenario

The M92 test configures both mock engines side by side, applies already-decided `core::ExchangeDecision` values through each boundary, and verifies:

- SWMM fixture state remains deterministic;
- SWMM accepted exchange writes SWMM lateral inflow only;
- D-Flow FM fixture state remains deterministic;
- D-Flow FM accepted exchange writes D-Flow FM lateral discharge only;
- both mock engines remain independent.

## Confirmed Negative Boundary

M89-M92 do not add:

- third-party SWMM headers, native libraries, or C API calls;
- third-party D-Flow FM / BMI headers, native libraries, or native calls;
- real `SWMMAdapter` or `DFlowFMAdapter` implementation;
- adapter-owned `Q_limit` or `V_limit` definitions;
- adapter-owned `mass_deficit_account` behavior;
- adapter-owned shared arbitration;
- adapter-owned rollback / replay orchestration;
- runtime scheduler process control;
- runtime evidence writing;
- GoldenSuite, release-gate, `REVIEW_REQUIRED`, `BLOCK_MERGE`, or `BLOCK_RELEASE` execution.

The fixture APIs remain mock-only scenario setup tools. They are not evidence of native SWMM or native D-Flow FM/BMI support.

## Local Evidence

Focused build and test:

```text
cmake --build --preset windows-msvc --target test_coupling_adapter_boundaries
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_adapter_boundaries$"
```

Result:

```text
1/1 Test #24: test_coupling_adapter_boundaries ...   Passed    0.16 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) =   0.19 sec
```

Full serial Debug CTest:

```text
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
33/33 Test #33: test_coupling_system_mass_audit ..... Passed
100% tests passed, 0 tests failed out of 33
Total Test time (real) =   6.69 sec
```

## Review Evidence

A read-only code review of M89-M92 found the mock fixture stage generally passes.

Confirmed by review:

- fixture APIs remain mock-only and are not added to the engine interfaces;
- no third-party SWMM/D-Flow FM/BMI headers or libraries are included or linked;
- core runtime semantics do not move into the engine interfaces;
- deterministic fixture state, validation reuse, and dual mock independent accepted-decision flow are covered by tests.

Review cautions:

- CI evidence remains pending and must be recorded after push before claiming merge/release readiness.
- Some files are currently untracked in the local working tree; ensure implementation, tests, and evidence are included together before commit.
- The `EngineInterfacesDoNotCarryCoreRuntimeSemantics` test remains a weak smoke check because abstract interfaces are naturally not constructible from core DTOs. Treat the boundary conclusion as source/CMake review plus focused behavior tests, not as a full mechanical proof.
- Test names containing `ThirdPartyFree` are supported by source/CMake review, not by binary dependency scanning.

## CI Evidence

Pending. Record the push commit and GitHub Actions run after this rollup and the M89-M92 changes are pushed.

## Decision

M89-M92 form a coherent mock fixture API stage. The next safe implementation step should move to a higher-level mock adapter scenario design or strengthen boundary tests with more direct compile-time interface checks. Real SWMM/D-Flow FM integration remains blocked until the third-party spike evidence is archived.

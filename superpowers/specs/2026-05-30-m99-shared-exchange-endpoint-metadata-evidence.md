# M99 Shared Exchange Endpoint Metadata Evidence

## Scope

M99 records and tests the endpoint metadata chain for shared-cell exchange intents and decisions.

Relevant core DTOs:

- `scau::coupling::core::SharedExchangeEngine`
- `scau::coupling::core::SharedExchangeEndpoint`
- `scau::coupling::core::SharedExchangeIntent`
- `scau::coupling::core::SharedExchangeDecision`

The endpoint metadata is the minimum core-owned mapping/audit hook needed to distinguish drainage/SWMM and river/D-Flow FM exchange intents without introducing real adapter implementations.

## Implemented Behavior

Shared exchange intents carry endpoint metadata:

- `engine` identifies the logical source family:
  - `drainage`
  - `river`
- `node_id` stores the project-owned endpoint identifier used by mock/unit-test scenarios.

`evaluate_shared_exchange(...)` preserves each intent endpoint in the corresponding `SharedExchangeDecision`.

`CouplingState::apply_shared_exchange(...)` returns the per-engine decisions after enqueueing the aggregate replay event, so endpoint metadata remains available to caller-side adapter accept paths and audit inspection.

## Boundary

M99 is metadata preservation only.

It does not add:

- real SWMM C API integration;
- real D-Flow FM / BMI integration;
- third-party SWMM, D-Flow FM, or BMI headers/libraries;
- real `SWMMAdapter` or `DFlowFMAdapter` implementation;
- adapter-owned `Q_limit` or `V_limit` definitions;
- adapter-owned `mass_deficit_account` behavior;
- adapter-owned rollback / replay orchestration;
- runtime scheduler process control;
- runtime evidence writing;
- GoldenSuite, release-gate, `REVIEW_REQUIRED`, `BLOCK_MERGE`, or `BLOCK_RELEASE` execution.

The metadata is intentionally project-owned and remains inside CouplingLib core DTOs. It is not a native SWMM node handle, D-Flow FM/BMI location handle, or third-party ABI object.

## Tests

Focused coverage exists in:

- `tests/unit/coupling/test_coupling_shared_exchange.cpp`
- `tests/unit/coupling/test_coupling_shared_replay.cpp`

Asserted invariants:

- `evaluate_shared_exchange(...)` preserves drainage endpoint metadata in `SharedExchangeDecision`;
- `evaluate_shared_exchange(...)` preserves river endpoint metadata in `SharedExchangeDecision`;
- `CouplingState::apply_shared_exchange(...)` returns per-engine decisions with preserved endpoint metadata;
- endpoint metadata preservation does not change shared replay aggregate accounting.

## Local Evidence

Focused build and replay test:

```text
cmake --build --preset windows-msvc --target test_coupling_shared_replay
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_shared_replay$"
```

Result:

```text
1/1 Test #26: test_coupling_shared_replay ......   Passed    0.28 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) =   0.40 sec
```

Full build and serial Debug CTest:

```text
cmake --build --preset windows-msvc
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
35/35 Test #35: test_coupling_system_mass_audit ..... Passed
100% tests passed, 0 tests failed out of 35
Total Test time (real) =   6.27 sec
```

## CI Evidence

Pending. Record the push commit and GitHub Actions run after this implementation/evidence change is pushed.

## Review Evidence

Pending. A follow-up read-only review should verify that endpoint metadata remains project-owned and does not imply real SWMM/D-Flow FM adapter integration.

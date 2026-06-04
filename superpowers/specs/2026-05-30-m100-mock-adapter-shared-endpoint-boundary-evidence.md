# M100 Mock Adapter Shared Endpoint Boundary Evidence

## Scope

M100 documents and tests the mock adapter boundary helpers that connect drainage/river mock adapters to shared exchange endpoint metadata.

Covered helper families:

- SWMM mock boundary:
  - `make_swmm_shared_exchange_intent(...)`
  - `accept_swmm_shared_exchange_decision(...)`
- D-Flow FM mock boundary:
  - `make_dflowfm_shared_exchange_intent(...)`
  - `accept_dflowfm_shared_exchange_decision(...)`

These helpers remain mock/project-owned boundary glue. They do not introduce real SWMM C API calls or D-Flow FM/BMI calls.

## Implemented Behavior

### Intent helpers

`make_swmm_shared_exchange_intent(...)`:

- rejects negative node IDs;
- rejects non-finite or negative `q_request`;
- rejects non-finite or non-positive `priority_weight`;
- creates a `SharedExchangeIntent` with `endpoint.engine = drainage`;
- stores the validated node ID in `endpoint.node_id`.

`make_dflowfm_shared_exchange_intent(...)`:

- rejects negative location IDs;
- rejects non-finite or negative `q_request`;
- rejects non-finite or non-positive `priority_weight`;
- creates a `SharedExchangeIntent` with `endpoint.engine = river`;
- stores the validated location ID in `endpoint.node_id`.

### Accept helpers

`accept_swmm_shared_exchange_decision(...)`:

- rejects decisions whose endpoint engine is not `drainage`;
- rejects endpoint IDs outside the `int` range required by the mock SWMM engine interface;
- forwards the decision to the existing SWMM accepted-decision path using the endpoint ID.

`accept_dflowfm_shared_exchange_decision(...)`:

- rejects decisions whose endpoint engine is not `river`;
- rejects endpoint IDs outside the `int` range required by the mock D-Flow FM engine interface;
- forwards the decision to the existing D-Flow FM lateral-discharge accepted-decision path using the endpoint ID.

## Boundary

M100 is a mock adapter endpoint-boundary stage only.

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

The endpoint IDs are project-owned mock/interface identifiers and must not be treated as native third-party object handles.

## Tests

Focused coverage remains under `tests/unit/coupling/test_coupling_shared_exchange.cpp`.

Asserted invariants:

- SWMM helper creates a drainage shared intent with preserved endpoint ID, request, and priority weight;
- D-Flow FM helper creates a river shared intent with preserved endpoint ID, request, and priority weight;
- helper input validation rejects invalid IDs, invalid requests, and invalid priority weights;
- SWMM shared accept helper rejects river-targeted decisions;
- D-Flow FM shared accept helper rejects drainage-targeted decisions;
- both shared accept helpers reject endpoint IDs outside the downstream mock interface `int` range;
- a dual mock shared-cell scenario can create typed intents, arbitrate in core, and accept the resulting decisions through the typed shared accept helpers.

## Local Evidence

Focused build and test:

```text
cmake --build --preset windows-msvc --target test_coupling_shared_exchange
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_shared_exchange$"
```

Result:

```text
1/1 Test #25: test_coupling_shared_exchange ....   Passed    0.20 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) =   0.28 sec
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
Total Test time (real) =   6.88 sec
```

## CI Evidence

Pending. Record the push commit and GitHub Actions run after this implementation/evidence change is pushed.

## Review Evidence

Manual read-only review completed after local validation.

Confirmed by review:

- SWMM and D-Flow FM shared intent helpers only create project-owned `SharedExchangeIntent` DTOs;
- shared accept helpers validate the target engine metadata before forwarding to existing mock accepted-decision paths;
- shared accept helpers reject endpoint IDs that cannot round-trip into the downstream mock engine `int` API;
- no SWMM C API, D-Flow FM/BMI API, native handle, third-party header, or third-party library is introduced;
- shared arbitration remains in CouplingLib core through `evaluate_shared_exchange(...)` and does not move into drainage or river helpers.

M100 is therefore a mock endpoint-boundary stage only, not real adapter integration evidence.

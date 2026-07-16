# M77 Drainage/River Mock Adapter Boundary Evidence

## Scope

M77 adds mock-only, third-party-free drainage and river adapter boundary skeletons. The goal is to establish project-owned ABI firewalls before any real SWMM or D-Flow FM integration, while respecting the third-party spike gate that blocks formal adapter implementation until spike evidence is archived.

Added library boundaries:

- `libs/coupling/drainage/`
- `libs/coupling/river/`

Added public boundary types:

- `scau::coupling::drainage::ISwmmEngine`
- `scau::coupling::drainage::MockSwmmEngine`
- `scau::coupling::river::IDFlowFMEngine`
- `scau::coupling::river::MockDFlowFMEngine`
- `scau::coupling::river::RiverExchangePoint`

Added mock boundary helpers:

- `make_swmm_exchange_request(...)`
- `accept_swmm_exchange_decision(...)`
- `make_dflowfm_exchange_request(...)`
- `accept_dflowfm_exchange_decision(...)`
- `is_valid_river_exchange_point(...)`

## Boundary

M77 is a mock-only adapter-seam boundary.

It does not add:

- SWMM headers, C API calls, native libraries, or `extern/swmm5/` dependencies;
- D-Flow FM / BMI headers, native libraries, or `extern/dflowfm*` dependencies;
- real `SWMMAdapter` or `DFlowFMAdapter` implementation;
- new `Q_limit` or `V_limit` definitions outside `CouplingLib core`;
- new `mass_deficit_account` behavior outside `CouplingLib core`;
- shared arbitration, rollback, replay, runtime scheduler, or runtime evidence writing;
- GoldenSuite, release-gate, `REVIEW_REQUIRED`, `BLOCK_MERGE`, or `BLOCK_RELEASE` execution.

## Tests

Focused unit tests were added under `tests/unit/coupling/test_coupling_adapter_boundaries.cpp`.

Asserted invariants:

- SWMM mock boundary is third-party-free and uses `core::ExchangeRequest` / `core::ExchangeDecision` DTOs;
- D-Flow FM mock boundary is third-party-free and uses `core::ExchangeRequest` / `core::ExchangeDecision` DTOs;
- SWMM and D-Flow FM mocks remain independent;
- `RiverExchangePoint.priority_weight` is positive and preserved as adapter-reported metadata;
- non-positive `RiverExchangePoint.priority_weight` is rejected by the boundary validator;
- engine interfaces do not carry core runtime semantics such as exchange decision construction or deficit-account construction.

## Local Evidence

Configure, focused build and test:

```text
cmake --preset windows-msvc
cmake --build --preset windows-msvc --target test_coupling_adapter_boundaries
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_adapter_boundaries$"
```

Result:

```text
1/1 Test #24: test_coupling_adapter_boundaries ...   Passed    0.47 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) =   0.66 sec
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

Manual boundary review found the M77 change is limited to:

- new drainage and river mock-only library targets;
- project-owned engine interfaces and mock implementations;
- `RiverExchangePoint` metadata validation for positive `priority_weight`;
- unit tests proving DTO usage, mock independence, and ABI firewall assumptions;
- this evidence record.

The change does not include or link third-party engine code, does not expose native SWMM/D-Flow FM/BMI types to `CouplingLib core`, and does not implement shared arbitration, flow limits, deficit ledger behavior, rollback/replay, scheduler process control, or release/merge gate handling outside their normative owners.

# M88 Mock Adapter Boundary Hardening Rollup

## Scope

M88 rolls up the mock-only drainage/river adapter boundary hardening performed from M77 through M87.

This rollup is a stage review and evidence consolidation. It does not add new production code, adapter behavior, or third-party integration.

## Covered Milestones

- M77: add mock-only, third-party-free drainage and river adapter boundary skeletons.
- M78: require finite `RiverExchangePoint.priority_weight` and `exchange_width`.
- M79: require finite mock adapter time-step, request, value, and accepted-flow inputs.
- M80: require non-negative accepted `q_granted` and `q_repay` values.
- M81: require all `ExchangeDecision` numeric fields to be finite and non-negative at mock accept boundaries.
- M82: require initialized mock engines for getter access.
- M83: restrict D-Flow FM mock variables to project-standard names.
- M84: restrict D-Flow FM mock `gate_opening` to `[0, 1]`.
- M85: reject negative SWMM node/link IDs and D-Flow FM location IDs.
- M86: validate `RiverExchangePoint.exchange_type` and `neighbor_2d_edges` metadata.
- M87: validate `RiverExchangePoint.branch_id`, `link_id`, and `crest_level`.

## Current Boundary State

### Drainage Boundary

Current drainage code is limited to:

```text
libs/coupling/drainage/CMakeLists.txt
libs/coupling/drainage/include/coupling/drainage/swmm_boundary.hpp
libs/coupling/drainage/src/swmm_boundary.cpp
```

It defines a project-owned mock seam only:

- `ISwmmEngine`
- `MockSwmmEngine`
- `SwmmEngineError`
- `make_swmm_exchange_request(...)`
- `accept_swmm_exchange_decision(...)`

The drainage target links only project code:

- `scau::coupling_core`
- `scau::warnings`

It does not include or link SWMM C headers, SWMM native libraries, `extern/swmm5/`, or real `SWMMAdapter` implementation.

### River Boundary

Current river code is limited to:

```text
libs/coupling/river/CMakeLists.txt
libs/coupling/river/include/coupling/river/dflowfm_boundary.hpp
libs/coupling/river/src/dflowfm_boundary.cpp
```

It defines a project-owned mock seam only:

- `IDFlowFMEngine`
- `MockDFlowFMEngine`
- `DFlowFMEngineError`
- `RiverExchangePoint`
- `is_valid_river_exchange_point(...)`
- `make_dflowfm_exchange_request(...)`
- `accept_dflowfm_exchange_decision(...)`

The river target links only project code:

- `scau::coupling_core`
- `scau::warnings`

It does not include or link D-Flow FM headers, BMI headers, native libraries, `extern/dflowfm*`, or real `DFlowFMAdapter` implementation.

## Confirmed Negative Boundary

M77-M87 do not add:

- third-party SWMM headers, native libraries, or C API calls;
- third-party D-Flow FM / BMI headers, native libraries, or native calls;
- real `SWMMAdapter` or `DFlowFMAdapter`;
- adapter-owned `Q_limit` or `V_limit` definitions;
- adapter-owned `mass_deficit_account` behavior;
- adapter-owned shared arbitration;
- adapter-owned rollback / replay orchestration;
- runtime scheduler process control;
- runtime evidence writing;
- GoldenSuite, release-gate, `REVIEW_REQUIRED`, `BLOCK_MERGE`, or `BLOCK_RELEASE` execution.

The adapter seams consume project-owned core DTOs and keep core semantics in `libs/coupling/core/`.

## Confirmed Positive Boundary

The current mock seams enforce:

### SWMM mock

- initialized lifecycle for `step`, getter, and setter calls;
- finite positive `dt_swmm`;
- non-negative node and link IDs;
- finite node lateral inflow;
- finite non-negative request flow;
- finite positive `dt_sub`;
- finite non-negative `ExchangeDecision` numeric fields before accepted exchange is written.

### D-Flow FM mock

- initialized lifecycle for `update`, getter, and setter calls;
- finite positive `dt_dfm`;
- non-negative `location_id`;
- supported project-standard variables only:
  - `water_level`
  - `discharge`
  - `lateral_discharge`
  - `gate_opening`
- finite state values;
- `gate_opening` within `[0, 1]`;
- finite non-negative request flow;
- finite positive `dt_sub`;
- finite non-negative `ExchangeDecision` numeric fields before accepted exchange is written.

### River exchange-point metadata

`RiverExchangePoint` validation requires:

- non-negative `branch_id`;
- non-negative `link_id`;
- supported `exchange_type` only:
  - `bank_overtop`
  - `lateral_weir`
  - `gate_outlet`
  - `culvert_link`
- finite `crest_level`;
- finite non-negative `exchange_width`;
- finite positive `priority_weight`;
- non-empty `neighbor_2d_edges`;
- non-negative `neighbor_2d_edges` entries.

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
Total Test time (real) =   0.18 sec
```

Full serial Debug CTest:

```text
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
33/33 Test #33: test_coupling_system_mass_audit ..... Passed
100% tests passed, 0 tests failed out of 33
Total Test time (real) =   6.28 sec
```

## Review Evidence

A read-only code review of M77-M87 found the mock adapter boundary hardening generally passes.

Confirmed by review:

- no third-party SWMM/D-Flow FM/BMI headers or libraries are included or linked;
- adapter layer does not define `Q_limit`, `V_limit`, deficit, rollback, replay, or arbitration semantics;
- lifecycle, finite, non-negative, standard-variable, `gate_opening`, ID, and `RiverExchangePoint` metadata guards are covered by tests;
- tests exercise the intended boundary-hardening behaviors.

Review cautions:

- `EngineInterfacesDoNotCarryCoreRuntimeSemantics` is only a weak compile-time smoke check because abstract classes are naturally not constructible from core DTOs. Treat the boundary conclusion as source review plus focused tests, not as a full mechanical proof.
- The D-Flow FM mock variable names are project-standard boundary names from the main spec, not verified third-party BMI native names. Real BMI names remain gated by the third-party spike.
- The mock accept helpers validate field shape and value domains, but intentionally do not validate deeper core decision consistency such as `v_granted == q_granted * dt_sub`; that remains a core responsibility.
- `MockSwmmEngine` does not currently provide configurable initial node-head/link-flow fixtures. Unset values return `0.0` after initialization; this is a skeleton behavior, not evidence of real SWMM state initialization.

## CI Evidence

Pending. Record the push commit and GitHub Actions run after this rollup and the M77-M87 changes are pushed.

## Decision

M77-M87 form a coherent mock-only adapter boundary hardening stage. The next safe implementation step should avoid adding more ad-hoc guards unless a specific invariant gap is identified. Recommended next work is either:

1. push and record CI evidence for M74-M88; or
2. begin a small mock-fixture API for deterministic adapter-boundary scenarios, while still avoiding real SWMM/D-Flow FM integration until the third-party spike evidence is archived.

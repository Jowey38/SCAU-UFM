# M101 Shared Cell Mock Coupling Rollup

## Scope

M101 rolls up the shared-cell mock coupling work completed from M96 through M100.

This rollup consolidates evidence for the current mock-only drainage/river coupling stage. It does not add new production behavior, real adapter integration, runtime scheduler behavior, or release-gate evidence.

## Covered Milestones

- M96: added core-owned shared-cell multi-engine arbitration for `priority_weight` splitting, proportional scaling, deficit-priority repayment, and finite input validation.
- M97: connected shared-cell arbitration results to `CouplingState` pending/replay through aggregate `CouplingEvent` semantics.
- M98: clarified empty shared-exchange intent lists as state-level no-op behavior to avoid audit-noise pending events.
- M99: preserved project-owned endpoint metadata from shared exchange intents into decisions and state helper return values.
- M100: connected mock drainage/river adapter boundaries to shared endpoint metadata through typed intent and accept helpers.

## Current Shared-Cell Mock Coupling State

### Core arbitration

`CouplingLib core` now owns the shared-cell arbitration helper:

- `SharedExchangeIntent`
- `SharedExchangeDecision`
- `evaluate_shared_exchange(...)`

Implemented core behavior:

- computes canonical `V_limit` / `Q_limit` with the existing flow-limit helper;
- rejects non-finite or invalid cell geometry, `dt_sub`, requests, priority weights, and deficit volume;
- splits `V_limit` / `Q_limit` by `priority_weight`;
- reserves repayment capacity from `mass_deficit_account.volume` before granting new requests;
- scales competing new requests by weighted proportional scaling;
- returns per-engine decisions with granted, unmet, repayment, allocated limit, priority weight, and endpoint metadata.

### State replay closure

`CouplingState::apply_shared_exchange(...)` now provides the state-level bridge:

- evaluates shared intents against the current cell;
- returns per-engine shared decisions for adapter/audit consumers;
- aggregates non-empty decisions into one pending `CouplingEvent`;
- reuses existing `replay_pending()` behavior for volume, unmet deficit, and repayment accounting;
- treats empty intent lists as a no-op and does not enqueue a zero-valued event.

### Endpoint metadata

Shared exchange endpoint metadata is project-owned and core DTO based:

- `SharedExchangeEngine::drainage`
- `SharedExchangeEngine::river`
- `SharedExchangeEndpoint::node_id`

The metadata is preserved from intent to decision and through `CouplingState::apply_shared_exchange(...)` return values. It is not a native SWMM node handle, native D-Flow FM/BMI location handle, or third-party ABI object.

### Mock adapter boundary helpers

Drainage/SWMM mock boundary helpers:

- `make_swmm_shared_exchange_intent(...)`
- `accept_swmm_shared_exchange_decision(...)`

River/D-Flow FM mock boundary helpers:

- `make_dflowfm_shared_exchange_intent(...)`
- `accept_dflowfm_shared_exchange_decision(...)`

These helpers:

- create typed shared intents with project-owned endpoint metadata;
- validate request and priority-weight fields;
- reject wrong-engine shared decisions;
- reject endpoint IDs outside the downstream mock engine `int` API range;
- forward accepted decisions to the existing mock accepted-decision paths.

## Confirmed Negative Boundary

M96-M100 do not add:

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

The stage remains mock-only and third-party-free. It is evidence for project-owned shared-cell coupling semantics, not evidence that native SWMM or native D-Flow FM/BMI integration is complete.

## Local Evidence

Focused tests used during the stage:

```text
cmake --build --preset windows-msvc --target test_coupling_shared_exchange
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_shared_exchange$"
```

Latest focused shared exchange result:

```text
1/1 Test #25: test_coupling_shared_exchange ....   Passed    0.20 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) =   0.28 sec
```

```text
cmake --build --preset windows-msvc --target test_coupling_shared_replay
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_shared_replay$"
```

Latest focused shared replay result:

```text
1/1 Test #26: test_coupling_shared_replay ......   Passed    0.28 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) =   0.40 sec
```

Latest full build and serial Debug CTest:

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

## Review Evidence

Manual/read-only reviews completed during this stage:

- M96 initial review found deficit-priority and finite-input gaps; follow-up fixes were applied and re-reviewed as Pass.
- M97 review passed aggregate replay accounting and evidence consistency.
- M98 review passed empty-intent no-op behavior.
- M99 manual review passed endpoint metadata as project-owned audit/mapping metadata.
- M100 manual review passed mock adapter endpoint-boundary helpers as third-party-free boundary glue.

Review caveat:

- M101 itself is a rollup only. It does not replace CI evidence and does not claim merge/release readiness.

## CI Evidence

Pending. Record the push commit and GitHub Actions run after the M96-M101 implementation/evidence changes are pushed.

## Remaining Gaps Before Scheduler/FaultController Work

Before moving beyond mock shared-cell semantics into scheduler or fault-control behavior, remaining gaps include:

1. A core-level runtime scheduler boundary is still absent.
   - No `dt_couple` execution loop drives SWMM and D-Flow FM mock engines together.
   - No scheduler-owned ordering evidence exists for request, arbitration, accept, replay, and audit phases.

2. FaultController behavior is still absent.
   - No shared `EngineReport` implementation path has been added for mock drainage/river engines.
   - No `healthy=false`, isolation, reconnect, `REVIEW_REQUIRED`, or protocol-state path is implemented.

3. Real third-party adapter integration remains blocked.
   - No SWMM C API calls are introduced.
   - No D-Flow FM/BMI calls are introduced.
   - Third-party spike evidence and ABI policy evidence must be archived before formal real adapter work.

4. GoldenSuite / CI gate evidence remains incomplete.
   - Local CTest evidence exists.
   - CI evidence is pending.
   - GoldenSuite release/merge gate evidence is not claimed by this stage.

5. Multi-cycle shared-cell audit is not yet implemented.
   - M96-M100 cover single-step arbitration, replay, endpoint metadata, and mock accept boundaries.
   - There is not yet a multi-cycle scheduler/audit trace proving repeated request/repayment/write-off behavior across coupling windows.

## Decision

M96-M100 form a coherent shared-cell mock coupling stage. The next safe implementation step should stay below real third-party adapter integration and move toward either:

1. a mock scheduler boundary that orchestrates request → core arbitration → adapter accept → state replay in a deterministic order; or
2. a mock `EngineReport` / health-report boundary that prepares drainage and river adapters for later FaultController integration.

Real SWMM/D-Flow FM adapter implementation remains blocked until third-party spike evidence is archived.

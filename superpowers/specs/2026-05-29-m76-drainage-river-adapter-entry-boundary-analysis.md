# M76 Drainage/River Adapter Entry Boundary Analysis

## Scope

M76 analyzes the next safe development boundary for moving from `CouplingLib core` helper work toward drainage/river coupling. It does not implement `SWMMAdapter`, `DFlowFMAdapter`, third-party engine bindings, or scheduler execution. Its purpose is to make the adapter entry conditions explicit before `libs/coupling/drainage/` and `libs/coupling/river/` are created or populated.

## Current Development State

The repository has an implemented `libs/coupling/core/` only:

```text
libs/coupling/core/CMakeLists.txt
libs/coupling/core/include/coupling/core/state.hpp
libs/coupling/core/src/state.cpp
```

There is no current `libs/coupling/drainage/` or `libs/coupling/river/` implementation. This matches the indexed project constraints: adapter implementation must not be invented ahead of the third-party spike exit criteria.

Recent coupling-core progress has closed the system-mass runtime control chain:

```text
compute_system_mass
-> audit
-> diagnostic
-> gate_decision
-> runtime_gate_outcome
-> runtime_control_decision
-> runtime_control_result
-> runtime_operator_action
```

This chain is still a coupling-core/runtime-control chain. It does not prove SWMM or D-Flow FM adapter readiness, and it does not execute a real coupled scheduler.

## Normative Entry Conditions

`superpowers/INDEX.md` identifies `2026-05-08-third-party-spike-design.md` as a conditional-authority spec for SWMM and D-Flow FM spike activity. That spec states that, before the spike exit criteria are met, the main repository must not enter formal implementation of:

- `libs/coupling/drainage/` for SWMM;
- `libs/coupling/river/` for D-Flow FM.

The same spike design requires each engine spike to prove or document:

- lifecycle: initialize, finalize, reset/destroy, error reporting;
- time stepping: externally controlled step-by-step advancement and stable state between steps;
- state read/write: required coupling state can be read and coupling input can be written between steps;
- hot-start/state save and restart consistency;
- error/exception containment without host-process abort;
- version stability across patch/minor version checks;
- thread safety, memory ownership, field units, valid domains, OS resources, and external dependency requirements.

## Adapter Boundary From The Normative Specs

### `libs/coupling/core/`

Core owns shared coupling semantics and must remain free of third-party engine ABI:

- shared DTOs such as `ExchangeRequest`, `ExchangeDecision`, and engine reports;
- shared flow-limit and arbitration semantics;
- `Q_limit` and `V_limit` semantics;
- `priority_weight` arbitration semantics;
- `mass_deficit_account`;
- snapshot / rollback / replay contracts;
- fault-controller-facing state.

Core must not include SWMM, D-Flow FM, or BMI headers, native handles, native enums, or native memory-layout-dependent fields.

### `libs/coupling/drainage/`

Drainage is the SWMM boundary. It may own:

- `ISwmmEngine`;
- `SWMMAdapter`;
- `MockSwmmEngine`;
- node/manhole/inlet mapping;
- conversion from SWMM node/link state into shared core DTOs;
- application of already-arbitrated `ExchangeDecision` back to SWMM.

Drainage must not own shared flow pools, `Q_limit`, deficit ledger semantics, D-Flow FM state, or rollback/replay orchestration.

### `libs/coupling/river/`

River is the D-Flow FM / BMI boundary. It may own:

- `IDFlowFMEngine`;
- `DFlowFMAdapter`;
- `MockDFlowFMEngine`;
- project-side BMI abstraction;
- `RiverExchangePoint` production and validation;
- river/bank/structure/boundary exchange mapping;
- conversion from D-Flow FM state into shared core DTOs;
- application of already-arbitrated `ExchangeDecision` back to D-Flow FM.

River must not own SWMM state, shared arbitration, `Q_limit`, deficit ledger semantics, or native D-Flow FM / BMI bridge source copies.

## Recommended Next Executable Task

The next safe implementation task is not a real SWMM/D-Flow FM adapter. The spike exit criteria are not present in the repository, and the normative spike design blocks formal adapter implementation until those criteria are met.

Recommended next task:

```text
M77: add coupling adapter boundary skeletons using mock-only, third-party-free interfaces.
```

M77 should be limited to project-owned boundary types and compile-time wiring, without third-party integration:

1. Add `libs/coupling/drainage/` with a minimal `ISwmmEngine` interface and `MockSwmmEngine` only.
2. Add `libs/coupling/river/` with a minimal `IDFlowFMEngine`, `RiverExchangePoint`, and `MockDFlowFMEngine` only.
3. Add CMake targets for both libraries, linked only to `scau::coupling_core` and project warnings.
4. Add unit tests proving:
   - no third-party headers are required;
   - SWMM and D-Flow FM mocks are independent;
   - `IDFlowFMEngine` does not carry `Q_limit`, `V_limit`, deficit, rollback, replay, or arbitration semantics;
   - `RiverExchangePoint.priority_weight` is positive and preserved as adapter-reported metadata;
   - mock adapters can produce shared `ExchangeRequest` intent and accept a core `ExchangeDecision` without redefining arbitration.
5. Keep real `SWMMAdapter` and `DFlowFMAdapter` out of scope until spike evidence is archived.

## M77 Non-Goals

M77 must not add:

- SWMM headers, C API calls, libraries, or `extern/swmm5/` dependencies;
- D-Flow FM / BMI headers, libraries, or `extern/dflowfm*` dependencies;
- real adapter lifecycle beyond mock-only project interfaces;
- new `Q_limit` or `V_limit` definitions outside core;
- new deficit account logic outside core;
- rollback/replay orchestration inside drainage or river;
- scheduler process control or runtime evidence writing;
- GoldenSuite, release-gate, `REVIEW_REQUIRED`, `BLOCK_MERGE`, or `BLOCK_RELEASE` execution.

## Evidence Required For M77

M77 evidence should record:

- new library paths and CMake targets;
- public headers added under drainage and river;
- unit tests proving mock-only third-party-free boundaries;
- focused build/test output for the new adapter-boundary tests;
- full serial Debug CTest output;
- CI run and job results after push.

## Decision

Proceed with M77 before any real adapter work. This advances drainage/river coupling by establishing the project-owned ABI firewall and mock-only adapter seams, while respecting the third-party spike gate that blocks formal SWMM/D-Flow FM implementation.

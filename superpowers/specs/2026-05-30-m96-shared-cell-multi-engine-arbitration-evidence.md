# M96 Shared Cell Multi-Engine Arbitration Evidence

## Supersession Note

This is historical pre-M148 evidence. Aggregate shared deficit wording in this file, including cell-level `mass_deficit_account` repayment and priority-weight repayment redistribution, is superseded by `superpowers/specs/2026-06-01-m148-shared-cell-endpoint-deficit-replay-correction-evidence.md`. Current shared-cell deficit ownership is endpoint-owned and duplicate shared endpoints fail closed.

## Scope

M96 adds a CouplingLib core-owned shared-cell arbitration helper for the mock-only drainage/river coupling stage.

New core DTOs and helper:

- `scau::coupling::core::SharedExchangeIntent`
- `scau::coupling::core::SharedExchangeDecision`
- `scau::coupling::core::evaluate_shared_exchange(...)`

The helper evaluates multiple 1D exchange intents against one shared 2D exchange cell and preserves the canonical ownership boundary: adapters report intent and accept already-decided results; CouplingLib core owns `Q_limit`, `V_limit`, and shared arbitration.

## Implemented Behavior

M96 implements the minimum shared-cell arbitration path needed for a SWMM + D-Flow FM mock scenario:

- computes the global `V_limit` and `Q_limit` through the existing `compute_flow_limit(...)` helper;
- splits the global `V_limit/Q_limit` by each intent's positive `priority_weight`;
- applies weighted proportional scaling when competing requests exceed the shared cell limit;
- consumes existing `mass_deficit_account.volume` first by reserving `Q_repay` before granting new requests;
- distributes repayment across shared intents by `priority_weight` while keeping the total repayment within the global `Q_limit`;
- reports each engine's granted flow, granted volume, repayment flow, repayment volume, and unmet volume through existing `ExchangeDecision` fields.

The implemented validation rejects:

- non-finite or non-positive `dt_sub`;
- non-finite or negative `q_request`;
- non-finite or non-positive `priority_weight`;
- non-finite or negative shared-cell deficit volume;
- non-finite shared-cell `phi_t`, `h`, or `area` values through the canonical flow-limit helper.

An empty intent list returns an empty decision list.

## Boundary

M96 is a core arbitration helper and focused mock scenario only.

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

This is intentionally still below the real third-party adapter boundary. It advances G12-style shared-cell behavior only through project-owned core DTOs and mock engines.

## Tests

Focused unit tests were added under `tests/unit/coupling/test_coupling_shared_exchange.cpp`.

Asserted invariants:

- shared-cell `V_limit/Q_limit` split follows `priority_weight` ratios;
- competing requests are scaled and clipped so total granted flow does not exceed the global `Q_limit`;
- existing deficit repayment consumes shared limit before new requests are granted;
- partial deficit repayment reduces the remaining shared limit for new requests;
- asymmetric request/weight combinations never grant above request or above the global limit;
- zero-storage shared cells grant no new request and report unmet volume;
- zero requests remain zero and report no unmet volume;
- empty intent lists return no decisions;
- invalid `dt_sub`, request, priority-weight, cell geometry, and deficit inputs are rejected;
- a dual mock SWMM + D-Flow FM scenario accepts core-arbitrated decisions independently through:
  - `accept_swmm_exchange_decision(...)`;
  - `accept_dflowfm_lateral_discharge_exchange_decision(...)`.

The new test target is registered in `tests/unit/coupling/CMakeLists.txt` as `test_coupling_shared_exchange`.

## Local Evidence

Focused build and test:

```text
cmake --build --preset windows-msvc --target test_coupling_shared_exchange
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_shared_exchange$"
```

Result:

```text
1/1 Test #25: test_coupling_shared_exchange ....   Passed    0.17 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) =   0.27 sec
```

Full serial Debug CTest:

```text
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
34/34 Test #34: test_coupling_system_mass_audit ..... Passed
100% tests passed, 0 tests failed out of 34
Total Test time (real) =   5.96 sec
```

## CI Evidence

Pending. Record the push commit and GitHub Actions run after this implementation/evidence change is pushed.

## Review Evidence

Manual boundary review found the M96 change is limited to:

- adding shared-cell arbitration DTOs and helper in CouplingLib core;
- adding focused unit tests for priority-weight splitting, proportional scaling, deficit repayment priority, validation, and dual mock acceptance;
- registering the new focused test target;
- this evidence record.

The change keeps native SWMM/D-Flow FM/BMI types out of CouplingLib core and keeps shared arbitration out of drainage/river adapters. Real third-party adapter implementation remains blocked until the third-party spike evidence is archived.

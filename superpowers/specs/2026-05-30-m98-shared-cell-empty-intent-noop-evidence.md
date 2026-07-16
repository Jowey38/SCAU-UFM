# M98 Shared Cell Empty Intent No-Op Evidence

## Supersession Note

This is historical pre-M148 evidence. Empty-intent no-op behavior remains valid, but any aggregate shared deficit wording is superseded by `superpowers/specs/2026-06-01-m148-shared-cell-endpoint-deficit-replay-correction-evidence.md`. Current shared-cell deficit ledger ownership and replay are endpoint-owned and duplicate shared endpoints fail closed.

## Scope

M98 tightens the M97 shared-cell replay helper by making empty shared-exchange intent lists an explicit no-op at the `CouplingState` layer.

Updated helper:

- `scau::coupling::core::CouplingState::apply_shared_exchange(...)`

## Implemented Behavior

When `apply_shared_exchange(...)` receives an empty intent list:

- it calls the existing core evaluator, which returns an empty decision list;
- it returns the empty decision list immediately;
- it does not enqueue a zero-valued `CouplingEvent`;
- it leaves cell volume and `mass_deficit_account` unchanged after replay;
- snapshots do not contain audit-noise pending events for empty shared exchanges.

Non-empty shared exchanges keep the M97 aggregate event behavior:

- `volume_delta = sum(v_granted + v_repay)`;
- `unmet_volume = sum(v_unmet)`;
- `repayment_volume = sum(v_repay)`.

## Boundary

M98 is a no-op behavior clarification inside CouplingLib core only.

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

## Tests

Focused unit tests remain under `tests/unit/coupling/test_coupling_shared_replay.cpp`.

New asserted invariant:

- `apply_shared_exchange(..., empty intents, ...)` returns an empty decision list, does not enqueue a pending event, and leaves cell volume and deficit unchanged after replay.

Existing M97 invariants remain covered for non-empty shared exchanges:

- aggregate pending replay event;
- replay volume/deficit accounting;
- deterministic rollback and replay;
- snapshot capture of pending shared-exchange events;
- out-of-range cell index rejection.

## Local Evidence

Focused build and test:

```text
cmake --build --preset windows-msvc --target test_coupling_shared_replay
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_shared_replay$"
```

Result:

```text
1/1 Test #26: test_coupling_shared_replay ......   Passed    0.21 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) =   0.30 sec
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
Total Test time (real) =   7.93 sec
```

## CI Evidence

Pending. Record the push commit and GitHub Actions run after this implementation/evidence change is pushed.

## Review Evidence

Pending. A follow-up read-only review should verify that the empty-intent no-op avoids audit noise without changing non-empty shared-cell replay semantics.

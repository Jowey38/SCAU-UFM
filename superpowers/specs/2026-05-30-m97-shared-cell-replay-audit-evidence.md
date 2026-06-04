# M97 Shared Cell Replay Audit Evidence

## Scope

M97 connects the M96 shared-cell multi-engine arbitration result to `CouplingState` pending/replay semantics.

New state-level helper:

- `scau::coupling::core::CouplingState::apply_shared_exchange(...)`

The helper evaluates multiple shared-cell intents through CouplingLib core arbitration, aggregates the resulting per-engine decisions into one deterministic `CouplingEvent`, and then relies on existing `replay_pending()` semantics for volume and `mass_deficit_account` updates.

## Implemented Behavior

`apply_shared_exchange(...)`:

- rejects out-of-range exchange cell indices;
- calls `evaluate_shared_exchange(...)` using the current cell state;
- aggregates all returned `SharedExchangeDecision` records into one pending event:
  - `volume_delta = sum(v_granted + v_repay)`;
  - `unmet_volume = sum(v_unmet)`;
  - `repayment_volume = sum(v_repay)`;
- enqueues that aggregate event through the existing `CouplingState::enqueue_event(...)` path;
- returns the per-engine decisions to the caller for adapter accept paths and audit inspection.

Replay remains owned by the existing `CouplingState::replay_pending()` implementation:

- cell `volume` receives the aggregate `volume_delta`;
- `mass_deficit_account` first rolls new unmet volume and then applies repayment volume;
- pending events are cleared after replay;
- rollback restores cells, runtime counters, and pending events from snapshots.

## Boundary

M97 is a CouplingLib core replay/audit closure only.

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

M97 keeps shared arbitration and replay accounting inside CouplingLib core. Drainage and river adapter boundaries remain mock-only and third-party-free.

## Tests

Focused unit tests were added under `tests/unit/coupling/test_coupling_shared_replay.cpp`.

Asserted invariants:

- shared exchange decisions are aggregated into one pending replay event;
- replay applies aggregate granted and repayment volume to cell volume;
- replay rolls unmet volume and applies repayment through the existing deficit-account helpers;
- rollback plus re-application of the same shared exchange produces deterministic cell volume and deficit state;
- snapshots capture pending shared-exchange events before replay;
- out-of-range shared exchange cell indices are rejected.

The new test target is registered in `tests/unit/coupling/CMakeLists.txt` as `test_coupling_shared_replay`.

## Local Evidence

Focused build and test:

```text
cmake --build --preset windows-msvc --target test_coupling_shared_replay
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_shared_replay$"
```

Result:

```text
1/1 Test #26: test_coupling_shared_replay ......   Passed    0.23 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) =   0.33 sec
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
Total Test time (real) =   7.26 sec
```

## CI Evidence

Pending. Record the push commit and GitHub Actions run after this implementation/evidence change is pushed.

## Review Evidence

Pending. A follow-up read-only review should verify that the aggregate event semantics match the intended shared-cell accounting boundary and do not move adapter, scheduler, or third-party engine responsibilities into CouplingLib core.

# M102 Mock Coupling Scheduler Boundary Evidence

## Supersession Note

This is historical pre-M148 mock scheduler evidence. The mock scheduler boundary remains non-executable and third-party-free, but shared deficit/replay ownership is now endpoint-owned and duplicate shared endpoints fail closed. See `superpowers/specs/2026-06-01-m148-shared-cell-endpoint-deficit-replay-correction-evidence.md` for current shared deficit/replay semantics.

## Scope

M102 adds the first mock scheduler boundary for shared-cell drainage/river coupling.

New core helper:

- `scau::coupling::core::CouplingState::run_mock_coupling_scheduler_step(...)`

New result DTO:

- `scau::coupling::core::MockCouplingSchedulerStepResult`

This is a deterministic same-process mock scheduler step. It is not a real runtime scheduler, process controller, wall-clock loop, or third-party adapter integration.

## Implemented Behavior

`run_mock_coupling_scheduler_step(...)` performs a minimal deterministic sequence:

1. receives project-owned shared exchange intents;
2. calls the existing `CouplingState::apply_shared_exchange(...)` path;
3. replays pending events through existing `CouplingState::replay_pending()` semantics;
4. returns the per-engine `SharedExchangeDecision` records for caller-side adapter accept paths and audit inspection.

The helper therefore connects the M96-M100 mock coupling chain:

```text
mock SWMM/D-Flow FM intents
→ CouplingLib core shared arbitration
→ CouplingState aggregate event
→ replay_pending()
→ returned decisions for typed mock adapter accept helpers
```

## Boundary

M102 is a mock scheduler-boundary helper only.

It does not add:

- real SWMM C API integration;
- real D-Flow FM / BMI integration;
- third-party SWMM, D-Flow FM, or BMI headers/libraries;
- real `SWMMAdapter` or `DFlowFMAdapter` implementation;
- process control, wall-clock scheduling, threading, async execution, or external runtime orchestration;
- FaultController behavior;
- `EngineReport` health handling;
- adapter-owned `Q_limit` or `V_limit` definitions;
- adapter-owned `mass_deficit_account` behavior;
- adapter-owned rollback / replay orchestration;
- runtime evidence writing;
- GoldenSuite, release-gate, `REVIEW_REQUIRED`, `BLOCK_MERGE`, or `BLOCK_RELEASE` execution.

All arbitration and replay accounting remains in CouplingLib core.

## Tests

Focused unit tests were added under `tests/unit/coupling/test_coupling_mock_scheduler.cpp`.

Asserted invariants:

- one scheduler step returns per-engine decisions with preserved drainage/river endpoint metadata;
- one scheduler step replays state deterministically and clears pending events;
- returned decisions can be accepted by the typed SWMM and D-Flow FM mock shared accept helpers;
- empty intent steps return no decisions, leave state unchanged, and leave no pending events.

The new test target is registered in `tests/unit/coupling/CMakeLists.txt` as `test_coupling_mock_scheduler`.

## Local Evidence

Focused build and test:

```text
cmake --build --preset windows-msvc --target test_coupling_mock_scheduler
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_mock_scheduler$"
```

Result:

```text
1/1 Test #27: test_coupling_mock_scheduler .....   Passed    0.28 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) =   0.91 sec
```

Full build and serial Debug CTest:

```text
cmake --build --preset windows-msvc
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
36/36 Test #36: test_coupling_system_mass_audit ..... Passed
100% tests passed, 0 tests failed out of 36
Total Test time (real) =   6.72 sec
```

## CI Evidence

Pending. Record the push commit and GitHub Actions run after this implementation/evidence change is pushed.

## Review Evidence

Manual read-only review completed after local validation.

Confirmed by review:

- `run_mock_coupling_scheduler_step(...)` is a synchronous helper on `CouplingState`, not a production runtime scheduler;
- the helper only calls existing core `apply_shared_exchange(...)` and `replay_pending()` paths;
- returned decisions remain project-owned `SharedExchangeDecision` DTOs for mock adapter accept paths;
- no process control, threading, async execution, wall-clock scheduling, FaultController state, or `EngineReport` health handling is introduced;
- no SWMM C API, D-Flow FM/BMI API, native handle, third-party header, or third-party library is introduced;
- shared arbitration and replay accounting remain in CouplingLib core and are not moved into drainage or river helpers.

M102 is therefore a deterministic mock scheduler-boundary step only, not production runtime scheduler or real adapter integration evidence.

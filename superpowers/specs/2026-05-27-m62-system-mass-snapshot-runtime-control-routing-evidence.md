# M62 System Mass Snapshot Runtime Control Routing Evidence

## Scope

M62 refactored the snapshot-baseline runtime control helper for system-mass conservation decisions:

- `CouplingState::decide_system_mass_runtime_control_against_snapshot(const CouplingSnapshot& baseline, double h_wet) const`

The helper now composes the existing snapshot runtime gate outcome helper and runtime control decision function:

- `evaluate_system_mass_runtime_gate_against_snapshot(...)`
- `make_system_mass_runtime_control_decision(...)`

It mirrors the M58 reference-side refactor and preserves the existing strict runtime-control mapping:

- conserved snapshot mass -> `SystemMassRuntimeGateStatus::running` -> `SystemMassRuntimeAbortHandlingState::continue_run` -> `should_abort == false`
- drifted snapshot mass -> `SystemMassRuntimeGateStatus::abort` -> `SystemMassRuntimeAbortHandlingState::abort` -> `should_abort == true`

## Boundary

M62 is a pure snapshot-baseline runtime-control routing refactor only.

It does not add:

- new public API;
- process termination;
- black-box or evidence file writing from runtime;
- rollback or replay orchestration;
- snapshot creation requirements;
- tolerance or `epsilon_mass` behavior;
- `REVIEW_REQUIRED`, `BLOCK_MERGE`, or `BLOCK_RELEASE` handling.

## Local Evidence

Focused build and test:

```text
cmake --build --preset windows-msvc --target test_coupling_core_state
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_core_state$"
```

Result:

```text
1/1 Test #23: test_coupling_core_state .........   Passed    0.23 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) =   0.32 sec
```

Full serial Debug CTest:

```text
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
32/32 Test #32: test_coupling_system_mass_audit ..... Passed
100% tests passed, 0 tests failed out of 32
Total Test time (real) =   5.97 sec
```

## CI Evidence

M62 commit:

```text
2605d58 refactor(coupling): route snapshot runtime control via gate outcome
```

GitHub Actions CI run:

```text
26522293836 push master success
```

Jobs:

```text
spike-isolation-check success
linux-gcc success
windows-msvc success
```

## Review Evidence

Manual boundary review found the M62 change is limited to:

- rerouting the existing snapshot-baseline runtime-control helper through the snapshot runtime gate outcome helper;
- preserving the existing runtime-control DTO and abort handling mapping;
- relying on existing conserved and drifted snapshot-baseline runtime-control tests, including the M61 abort-predicate-versus-decision consistency tests.

The implementation only changes helper composition inside `CouplingState`. It does not mutate `CouplingState` while deciding runtime control and does not introduce process termination, runtime side effects, evidence writing, rollback, replay, tolerance, snapshot creation requirements, public API changes, or release/merge gate handling.

# M47 System Mass Runtime Control Decision Evidence

## Scope

M47 added a minimal state-level runtime-control decision DTO for system-mass snapshot checks:

- `SystemMassRuntimeControlDecision`
- `CouplingState::decide_system_mass_runtime_control_against_snapshot(...)`

The decision packages the existing M43-M46 runtime-control chain for external orchestration:

- `CouplingState::evaluate_system_mass_runtime_gate_against_snapshot(...)`
- `classify_system_mass_runtime_abort_handling(...)`
- `should_abort_system_mass_runtime(...)`

It exposes the gate outcome, abort handling state, and final abort predicate result:

- conserved snapshot comparison -> `running`, `continue_run`, `false`
- drifted snapshot comparison -> `abort`, `abort`, `true`

## Boundary

M47 is a pure state-level decision packaging boundary only.

It does not add:

- process termination;
- black-box or evidence file writing from runtime;
- rollback or replay orchestration;
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
1/1 Test #23: test_coupling_core_state .........   Passed    0.21 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) =   0.30 sec
```

Full serial Debug CTest:

```text
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
32/32 Test #32: test_coupling_system_mass_audit ..... Passed
100% tests passed, 0 tests failed out of 32
Total Test time (real) =   6.01 sec
```

## CI Evidence

M47 commit:

```text
390abb5 feat(coupling): expose runtime control decision
```

GitHub Actions CI run:

```text
26487785598 push master success
```

Jobs:

```text
spike-isolation-check success
linux-gcc success
windows-msvc success
```

## Review Evidence

Manual boundary review found the M47 change is limited to:

- a new `SystemMassRuntimeControlDecision` DTO;
- a new `CouplingState::decide_system_mass_runtime_control_against_snapshot(...)` state-level method;
- tests for conserved and drifted snapshot comparisons.

The implementation composes the existing runtime gate, abort handling classification, and abort predicate helpers. It does not mutate `CouplingState` and does not introduce process termination, runtime side effects, evidence writing, rollback, replay, tolerance, or release/merge gate handling.

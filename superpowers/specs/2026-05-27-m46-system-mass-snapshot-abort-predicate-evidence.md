# M46 System Mass Snapshot Abort Predicate Evidence

## Scope

M46 added a minimal state-level runtime-control predicate for system-mass snapshot checks:

- `CouplingState::should_abort_system_mass_runtime_against_snapshot(...)`

The predicate composes the existing M43-M45 runtime gate chain:

- `CouplingState::evaluate_system_mass_runtime_gate_against_snapshot(...)`
- `classify_system_mass_runtime_abort_handling(...)`
- `should_abort_system_mass_runtime(...)`

It returns whether runtime control should abort for the current state against a captured snapshot:

- conserved snapshot comparison -> `false`
- drifted snapshot comparison -> `true`

## Boundary

M46 is a pure state-level predicate boundary only.

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
1/1 Test #23: test_coupling_core_state .........   Passed    0.24 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) =   0.33 sec
```

Full serial Debug CTest:

```text
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
32/32 Test #32: test_coupling_system_mass_audit ..... Passed
100% tests passed, 0 tests failed out of 32
Total Test time (real) =   6.66 sec
```

## CI Evidence

M46 commit:

```text
3f995ed feat(coupling): expose snapshot abort predicate
```

GitHub Actions CI run:

```text
26486813649 push master success
```

Jobs:

```text
spike-isolation-check success
linux-gcc success
windows-msvc success
```

## Review Evidence

Manual boundary review found the M46 change is limited to:

- a new `CouplingState::should_abort_system_mass_runtime_against_snapshot(...)` predicate;
- tests for conserved and drifted snapshot comparisons.

The implementation composes the existing runtime gate, abort handling classification, and abort predicate helpers. It does not mutate `CouplingState` and does not introduce process termination, runtime side effects, evidence writing, rollback, replay, tolerance, or release/merge gate handling.

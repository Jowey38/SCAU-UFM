# M53 System Mass Diagnostic Runtime Abort Helper Evidence

## Scope

M53 extracted a pure diagnostic-to-runtime-abort predicate helper for system-mass runtime decisions:

- `should_abort_system_mass_runtime(const SystemMassConservationDiagnostic& diagnostic)`

The helper composes the existing diagnostic-to-runtime-control decision helper:

- `make_system_mass_runtime_control_decision(const SystemMassConservationDiagnostic& diagnostic)`

It preserves the existing diagnostic-to-action, action-to-runtime-status, runtime-status-to-handling, and handling-to-abort mappings:

- conserved diagnostic -> `continue_run` gate action -> `running` runtime gate status -> `continue_run` handling -> `false`
- drifted diagnostic -> `abort_run` gate action -> `abort` runtime gate status -> `abort` handling -> `true`

M53 also refactored the snapshot abort predicate to share the diagnostic-input helper:

- `CouplingState::should_abort_system_mass_runtime_against_snapshot(...)`

## Boundary

M53 is a pure helper extraction and reuse boundary only.

It does not add:

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
1/1 Test #23: test_coupling_core_state .........   Passed    0.19 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) =   0.29 sec
```

Full serial Debug CTest:

```text
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
32/32 Test #32: test_coupling_system_mass_audit ..... Passed
100% tests passed, 0 tests failed out of 32
Total Test time (real) =   6.35 sec
```

## CI Evidence

M53 commit:

```text
cc83be1 feat(coupling): factor diagnostic runtime abort helper
```

GitHub Actions CI run:

```text
26504068022 push master success
```

Jobs:

```text
spike-isolation-check success
linux-gcc success
windows-msvc success
```

## Review Evidence

Manual boundary review found the M53 change is limited to:

- a new diagnostic-input overload of `should_abort_system_mass_runtime(...)`;
- reuse of that helper by snapshot abort predicate packaging;
- tests for conserved and drifted diagnostic abort mappings.

The implementation composes the existing diagnostic-to-runtime-control decision helper and returns its `should_abort` predicate. It does not mutate `CouplingState` and does not introduce process termination, runtime side effects, evidence writing, rollback, replay, tolerance, snapshot creation requirements, or release/merge gate handling.

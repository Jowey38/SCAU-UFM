# M52 System Mass Diagnostic Runtime Control Helper Evidence

## Scope

M52 extracted a pure diagnostic-to-runtime-control decision packaging helper for system-mass runtime decisions:

- `make_system_mass_runtime_control_decision(const SystemMassConservationDiagnostic& diagnostic)`

The helper composes the existing runtime-gate and runtime-control helpers:

- `make_system_mass_runtime_gate_outcome(const SystemMassConservationDiagnostic& diagnostic)`
- `make_system_mass_runtime_control_decision(const SystemMassRuntimeGateOutcome& gate_outcome)`

It preserves the existing diagnostic-to-action, action-to-runtime-status, runtime-status-to-handling, and handling-to-abort mappings:

- conserved diagnostic -> `continue_run` gate action -> `running` runtime gate status -> `continue_run` handling -> `false`
- drifted diagnostic -> `abort_run` gate action -> `abort` runtime gate status -> `abort` handling -> `true`

It preserves the input `SystemMassConservationDiagnostic` payload in the returned `SystemMassRuntimeControlDecision`.

M52 also refactored runtime-control paths to share the diagnostic-input helper:

- `CouplingState::decide_system_mass_runtime_control_against_snapshot(...)`
- `CouplingState::decide_system_mass_runtime_control_against_reference(...)`

## Boundary

M52 is a pure helper extraction and reuse boundary only.

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
1/1 Test #23: test_coupling_core_state .........   Passed    0.29 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) =   0.57 sec
```

Full serial Debug CTest:

```text
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
32/32 Test #32: test_coupling_system_mass_audit ..... Passed
100% tests passed, 0 tests failed out of 32
Total Test time (real) =   7.49 sec
```

## CI Evidence

M52 commit:

```text
d8b86ed feat(coupling): factor diagnostic runtime control helper
```

GitHub Actions CI run:

```text
26500979981 push master success
```

Jobs:

```text
spike-isolation-check success
linux-gcc success
windows-msvc success
```

## Review Evidence

Manual boundary review found the M52 change is limited to:

- a new diagnostic-input overload of `make_system_mass_runtime_control_decision(...)`;
- reuse of that helper by snapshot and reference-baseline runtime-control packaging;
- tests for conserved and drifted diagnostic runtime-control mappings.

The implementation composes the existing diagnostic-to-runtime-gate outcome helper and runtime-gate-outcome-to-runtime-control helper. It does not mutate `CouplingState` and does not introduce process termination, runtime side effects, evidence writing, rollback, replay, tolerance, snapshot creation requirements, or release/merge gate handling.

# M51 System Mass Diagnostic Runtime Gate Helper Evidence

## Scope

M51 extracted a pure diagnostic-to-runtime-gate outcome packaging helper for system-mass runtime decisions:

- `make_system_mass_runtime_gate_outcome(const SystemMassConservationDiagnostic& diagnostic)`

The helper composes the existing gate-decision and runtime-gate outcome helpers:

- `decide_system_mass_gate_action(...)`
- `make_system_mass_runtime_gate_outcome(const SystemMassGateDecision& decision)`

It preserves the existing diagnostic-to-action and action-to-runtime-status mappings:

- conserved diagnostic -> `SystemMassGateAction::continue_run` -> `SystemMassRuntimeGateStatus::running`
- drifted diagnostic -> `SystemMassGateAction::abort_run` -> `SystemMassRuntimeGateStatus::abort`

It preserves the input `SystemMassConservationDiagnostic` payload in the returned `SystemMassRuntimeGateOutcome`.

M51 also refactored runtime paths to share the diagnostic-input helper:

- `CouplingState::evaluate_system_mass_runtime_gate_against_snapshot(...)`
- `CouplingState::decide_system_mass_runtime_control_against_reference(...)`

## Boundary

M51 is a pure helper extraction and reuse boundary only.

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
1/1 Test #23: test_coupling_core_state .........   Passed    0.22 sec
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
Total Test time (real) =   5.97 sec
```

## CI Evidence

M51 commit:

```text
2caaa70 feat(coupling): factor diagnostic runtime gate helper
```

GitHub Actions CI run:

```text
26491715813 push master success
```

Jobs:

```text
spike-isolation-check success
linux-gcc success
windows-msvc success
```

## Review Evidence

Manual boundary review found the M51 change is limited to:

- a new diagnostic-input overload of `make_system_mass_runtime_gate_outcome(...)`;
- reuse of that helper by snapshot runtime gate evaluation and reference-baseline runtime-control packaging;
- tests for conserved and drifted diagnostic mappings.

The implementation composes the existing diagnostic-to-gate-decision helper and gate-decision-to-runtime-outcome helper. It does not mutate `CouplingState` and does not introduce process termination, runtime side effects, evidence writing, rollback, replay, tolerance, snapshot creation requirements, or release/merge gate handling.

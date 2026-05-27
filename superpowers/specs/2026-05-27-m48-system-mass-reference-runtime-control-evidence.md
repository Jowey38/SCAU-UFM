# M48 System Mass Reference Runtime Control Evidence

## Scope

M48 added a minimal state-level runtime-control decision API for system-mass checks against a precomputed reference baseline:

- `CouplingState::decide_system_mass_runtime_control_against_reference(...)`

The decision packages the existing runtime-control chain for callers that already hold a `SystemMassAudit` baseline:

- `CouplingState::audit_system_mass_against_reference(...)`
- `make_system_mass_conservation_diagnostic(...)`
- `decide_system_mass_gate_action(...)`
- `classify_system_mass_runtime_abort_handling(...)`
- `should_abort_system_mass_runtime(...)`

It exposes the same DTO shape introduced by M47:

- conserved reference comparison -> `running`, `continue_run`, `false`
- drifted reference comparison -> `abort`, `abort`, `true`

## Boundary

M48 is a pure state-level reference-baseline decision packaging boundary only.

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
1/1 Test #23: test_coupling_core_state .........   Passed    0.31 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) =   0.40 sec
```

Full serial Debug CTest:

```text
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
32/32 Test #32: test_coupling_system_mass_audit ..... Passed
100% tests passed, 0 tests failed out of 32
Total Test time (real) =   6.11 sec
```

## CI Evidence

M48 commit:

```text
54f8f1d feat(coupling): expose reference runtime control decision
```

GitHub Actions CI run:

```text
26489503645 push master success
```

Jobs:

```text
spike-isolation-check success
linux-gcc success
windows-msvc success
```

## Review Evidence

Manual boundary review found the M48 change is limited to:

- a new `CouplingState::decide_system_mass_runtime_control_against_reference(...)` state-level method;
- tests for conserved and drifted reference-baseline comparisons.

The implementation composes the existing system-mass reference audit, diagnostic, gate action, abort handling classification, and abort predicate helpers. It does not mutate `CouplingState` and does not introduce process termination, runtime side effects, evidence writing, rollback, replay, tolerance, snapshot creation requirements, or release/merge gate handling.

# M55 System Mass Reference Diagnostic Helper Evidence

## Scope

M55 added a reference-baseline diagnostic convenience helper for system-mass conservation decisions:

- `CouplingState::diagnose_system_mass_against_reference(const SystemMassAudit& baseline, double h_wet) const`

The helper composes the existing reference audit and diagnostic packaging functions:

- `audit_system_mass_against_reference(...)`
- `make_system_mass_conservation_diagnostic(...)`

It preserves the existing strict conservation mapping:

- zero residual -> `SystemMassConservationStatus::conserved`
- nonzero residual -> `SystemMassConservationStatus::drifted`

M55 also refactored the reference runtime-control and runtime-abort predicate paths to share the reference diagnostic helper:

- `CouplingState::decide_system_mass_runtime_control_against_reference(...)`
- `CouplingState::should_abort_system_mass_runtime_against_reference(...)`

## Boundary

M55 is a pure reference-baseline diagnostic helper extraction and reuse boundary only.

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
Total Test time (real) =   0.53 sec
```

Full serial Debug CTest:

```text
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
32/32 Test #32: test_coupling_system_mass_audit ..... Passed
100% tests passed, 0 tests failed out of 32
Total Test time (real) =   8.00 sec
```

## CI Evidence

M55 commit:

```text
53331e0 feat(coupling): add reference mass diagnostic helper
```

GitHub Actions CI run:

```text
26509067215 push master success
```

Jobs:

```text
spike-isolation-check success
linux-gcc success
windows-msvc success
```

## Review Evidence

Manual boundary review found the M55 change is limited to:

- a new reference-baseline diagnostic helper on `CouplingState`;
- reuse of that helper by reference runtime-control and runtime-abort predicate packaging;
- tests for conserved and drifted reference-baseline diagnostic mappings.

The implementation only packages the existing reference system-mass audit into the existing diagnostic DTO. It does not mutate `CouplingState` while diagnosing and does not introduce process termination, runtime side effects, evidence writing, rollback, replay, tolerance, snapshot creation requirements, or release/merge gate handling.

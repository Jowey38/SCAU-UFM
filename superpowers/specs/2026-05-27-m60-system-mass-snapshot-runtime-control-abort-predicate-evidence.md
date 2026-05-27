# M60 System Mass Snapshot Runtime Control Abort Predicate Evidence

## Scope

M60 added a snapshot-baseline runtime-control abort predicate convenience helper for system-mass conservation decisions:

- `CouplingState::should_abort_system_mass_runtime_control_against_snapshot(const CouplingSnapshot& baseline, double h_wet) const`

The helper composes the existing snapshot runtime-control helper and reads its boolean abort flag:

- `decide_system_mass_runtime_control_against_snapshot(...)`
- `SystemMassRuntimeControlDecision::should_abort`

It preserves the existing strict runtime-control mapping:

- conserved snapshot mass -> `should_abort == false`
- drifted snapshot mass -> `should_abort == true`

It mirrors the reference-side runtime-control abort predicate added in M59:

- `should_abort_system_mass_runtime_control_against_reference(...)`

## Boundary

M60 is a pure snapshot-baseline runtime-control abort predicate boundary only.

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
1/1 Test #23: test_coupling_core_state .........   Passed    0.35 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) =   0.42 sec
```

Full serial Debug CTest:

```text
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
32/32 Test #32: test_coupling_system_mass_audit ..... Passed
100% tests passed, 0 tests failed out of 32
Total Test time (real) =   6.23 sec
```

## CI Evidence

M60 commit:

```text
83313ec feat(coupling): add snapshot runtime control abort predicate
```

GitHub Actions CI run:

```text
26520528716 push master success
```

Jobs:

```text
spike-isolation-check success
linux-gcc success
windows-msvc success
```

## Review Evidence

Manual boundary review found the M60 change is limited to:

- a new snapshot-baseline runtime-control abort predicate on `CouplingState`;
- composition through the existing snapshot runtime-control helper;
- tests for conserved and drifted snapshot-baseline runtime-control abort flag mappings.

The implementation only forwards the existing snapshot runtime-control DTO `should_abort` field. It does not mutate `CouplingState` while reading the predicate and does not introduce process termination, runtime side effects, evidence writing, rollback, replay, tolerance, snapshot creation requirements, or release/merge gate handling.

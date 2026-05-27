# M56 System Mass Reference Gate Helper Evidence

## Scope

M56 added a reference-baseline gate action convenience helper for system-mass conservation decisions:

- `CouplingState::decide_system_mass_gate_action_against_reference(const SystemMassAudit& baseline, double h_wet) const`

The helper composes the existing reference diagnostic helper and gate decision function:

- `diagnose_system_mass_against_reference(...)`
- `decide_system_mass_gate_action(...)`

It preserves the existing strict diagnostic-to-gate mapping:

- conserved diagnostic -> `SystemMassGateAction::continue_run`
- drifted diagnostic -> `SystemMassGateAction::abort_run`

## Boundary

M56 is a pure reference-baseline gate action helper boundary only.

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
1/1 Test #23: test_coupling_core_state .........   Passed    0.21 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) =   0.31 sec
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

M56 commit:

```text
518da8c feat(coupling): add reference mass gate helper
```

GitHub Actions CI run:

```text
26513237494 push master success
```

Jobs:

```text
spike-isolation-check success
linux-gcc success
windows-msvc success
```

## Review Evidence

Manual boundary review found the M56 change is limited to:

- a new reference-baseline gate action helper on `CouplingState`;
- composition through the existing reference diagnostic helper and diagnostic-to-gate action helper;
- tests for conserved and drifted reference-baseline gate mappings.

The implementation only packages the existing reference system-mass diagnostic into the existing gate action DTO. It does not mutate `CouplingState` while deciding the gate action and does not introduce process termination, runtime side effects, evidence writing, rollback, replay, tolerance, snapshot creation requirements, or release/merge gate handling.

# M70 System Mass Audit-To-Gate-Decision End-To-End Consistency Evidence

## Scope

M70 added end-to-end consistency tests that chain `audit_system_mass_against_*` through `make_system_mass_conservation_diagnostic` into `decide_system_mass_gate_action` and assert the result equals the matching `decide_system_mass_gate_action_against_*` helper, for both reference and snapshot baselines, under conserved and drifted system-mass states.

Implementation note: prior to writing tests, the snapshot-side `decide_system_mass_gate_action_against_snapshot` implementation was reviewed and found already minimal — it composes `diagnose_system_mass_against_snapshot` with `core::decide_system_mass_gate_action`, so no implementation change was required.

Helpers exercised:

- `CouplingState::audit_system_mass_against_reference(...)`
- `CouplingState::audit_system_mass_against_snapshot(...)`
- `CouplingState::decide_system_mass_gate_action_against_reference(...)`
- `CouplingState::decide_system_mass_gate_action_against_snapshot(...)`
- `make_system_mass_conservation_diagnostic(const SystemMassDelta&)`
- `decide_system_mass_gate_action(const SystemMassConservationDiagnostic&)`

Asserted invariants:

- `decide_system_mass_gate_action(make_system_mass_conservation_diagnostic(audit_against_*)).action == decide_gate_action_against_*.action`;
- diagnostic fields agree (`status`, `residual`, `baseline_total_mass`, `current_total_mass`);
- conserved -> `action == continue_run`;
- drifted -> `action == abort_run`.

The tests close the multi-hop bridge `audit -> diagnostic -> gate_decision` as a single end-to-end equality, complementing M67 (`audit -> diagnostic`) and M68 (`diagnostic -> gate_decision`). Together with M61/M63/M64/M65/M66/M69 the full `compute_system_mass -> audit -> diagnostic -> gate_decision -> gate_outcome -> handling_state -> should_abort` chain is now locked end-to-end on both reference and snapshot baselines.

## Boundary

M70 is a pure consistency-test boundary only.

It does not add:

- new public API or DTOs;
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
Total Test time (real) =   6.34 sec
```

## CI Evidence

M70 commit:

```text
f7b15e4 test(coupling): assert gate action against baseline matches audit chain
```

GitHub Actions CI run:

```text
26556381330 push master success
```

Jobs:

```text
spike-isolation-check success
linux-gcc success
windows-msvc success
```

## Review Evidence

Manual boundary review found the M70 change is limited to:

- a verification that the snapshot-side `decide_system_mass_gate_action_against_snapshot` implementation is already minimal (`diagnose_system_mass_against_snapshot` + `core::decide_system_mass_gate_action`), with no implementation change required;
- new GoogleTest cases under `tests/unit/coupling/test_coupling_core_state.cpp` that assert the audit -> diagnostic -> gate_decision chain agrees with `decide_system_mass_gate_action_against_*` for reference and snapshot baselines under conserved and drifted system-mass states.

The change does not modify production headers or source, does not add or change DTOs, and does not introduce process termination, runtime side effects, evidence writing, rollback, replay, tolerance, snapshot creation requirements, or release/merge gate handling.

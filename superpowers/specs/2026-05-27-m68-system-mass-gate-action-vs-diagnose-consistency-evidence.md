# M68 System Mass Gate Action Vs Diagnose Consistency Evidence

## Scope

M68 added consistency tests that assert applying `decide_system_mass_gate_action` to the result of `diagnose_system_mass_against_*` yields the same gate decision as the matching `decide_system_mass_gate_action_against_*` helper for both reference and snapshot baselines, under conserved and drifted system-mass states.

Helpers exercised:

- `CouplingState::diagnose_system_mass_against_reference(...)`
- `CouplingState::decide_system_mass_gate_action_against_reference(...)`
- `CouplingState::diagnose_system_mass_against_snapshot(...)`
- `CouplingState::decide_system_mass_gate_action_against_snapshot(...)`
- `decide_system_mass_gate_action(const SystemMassConservationDiagnostic&)`

Asserted invariants:

- `decide_system_mass_gate_action(diagnose_against_*).action == decide_gate_action_against_*.action`;
- diagnostic fields agree (`status`, `residual`, `baseline_total_mass`, `current_total_mass`);
- conserved -> `action == continue_run`;
- drifted -> `action == abort_run`.

The tests close the second hop in the runtime decision chain: `diagnostic -> gate_decision`. Together with M61/M63/M64/M65/M66/M67, the full `audit -> diagnostic -> gate_decision -> gate_outcome -> handling_state -> should_abort` chain is now locked for both reference and snapshot baselines.

## Boundary

M68 is a pure consistency-test boundary only.

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
1/1 Test #23: test_coupling_core_state .........   Passed    0.25 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) =   0.34 sec
```

Full serial Debug CTest:

```text
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
32/32 Test #32: test_coupling_system_mass_audit ..... Passed
100% tests passed, 0 tests failed out of 32
Total Test time (real) =   6.32 sec
```

## CI Evidence

M68 commit:

```text
4d3edd1 test(coupling): assert gate action against baseline matches diagnose
```

GitHub Actions CI run:

```text
26552320318 push master success
```

Jobs:

```text
spike-isolation-check success
linux-gcc success
windows-msvc success
```

## Review Evidence

Manual boundary review found the M68 change is limited to:

- new GoogleTest cases under `tests/unit/coupling/test_coupling_core_state.cpp` that assert `decide_system_mass_gate_action(diagnose_system_mass_against_*)` agrees with `decide_system_mass_gate_action_against_*` for reference and snapshot baselines under conserved and drifted system-mass states.

The change does not modify production headers or source, does not add or change DTOs, and does not introduce process termination, runtime side effects, evidence writing, rollback, replay, tolerance, snapshot creation requirements, or release/merge gate handling.

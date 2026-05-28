# M67 System Mass Diagnose Vs Audit Consistency Evidence

## Scope

M67 added consistency tests that assert applying `make_system_mass_conservation_diagnostic` to the result of `audit_system_mass_against_*` yields the same diagnostic as the matching `diagnose_system_mass_against_*` helper for both reference and snapshot baselines, under conserved and drifted system-mass states.

Helpers exercised:

- `CouplingState::audit_system_mass_against_reference(...)`
- `CouplingState::diagnose_system_mass_against_reference(...)`
- `CouplingState::audit_system_mass_against_snapshot(...)`
- `CouplingState::diagnose_system_mass_against_snapshot(...)`
- `make_system_mass_conservation_diagnostic(const SystemMassDelta&)`

Asserted invariants:

- `make_system_mass_conservation_diagnostic(audit_against_*) == diagnose_against_*` field-wise (`status`, `residual`, `baseline_total_mass`, `current_total_mass`);
- conserved -> `status == conserved`, `residual == 0.0`;
- drifted -> `status == drifted`, `residual == 4.0` (matches the fixed test fixture drift).

The tests close the first hop in the runtime decision chain: `audit -> diagnostic`. Together with the M61/M63/M64/M65/M66 consistency tests, the full `audit -> diagnostic -> gate_decision -> gate_outcome -> handling_state -> should_abort` chain is now locked for both reference and snapshot baselines.

## Boundary

M67 is a pure consistency-test boundary only.

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
1/1 Test #23: test_coupling_core_state .........   Passed    0.27 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) =   0.35 sec
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

M67 commit:

```text
8148013 test(coupling): assert diagnose against baseline matches audit
```

GitHub Actions CI run:

```text
26551082404 push master success
```

Jobs:

```text
spike-isolation-check success
linux-gcc success
windows-msvc success
```

## Review Evidence

Manual boundary review found the M67 change is limited to:

- new GoogleTest cases under `tests/unit/coupling/test_coupling_core_state.cpp` that assert `make_system_mass_conservation_diagnostic(audit_system_mass_against_*)` agrees field-wise with `diagnose_system_mass_against_*` for reference and snapshot baselines under conserved and drifted system-mass states.

The change does not modify production headers or source, does not add or change DTOs, and does not introduce process termination, runtime side effects, evidence writing, rollback, replay, tolerance, snapshot creation requirements, or release/merge gate handling.

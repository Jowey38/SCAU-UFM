# M69 System Mass Audit Baseline Round-Trip Consistency Evidence

## Scope

M69 added consistency tests that assert the baseline and current fields of the system-mass delta returned by `audit_system_mass_against_*` round-trip with the inputs used to compute them, for both reference and snapshot baselines, under conserved and drifted system-mass states.

Helpers exercised:

- `CouplingState::compute_system_mass(double)`
- `CouplingSnapshot::compute_system_mass(double)`
- `CouplingState::audit_system_mass_against_reference(const SystemMassAudit&, double)`
- `CouplingState::audit_system_mass_against_snapshot(const CouplingSnapshot&, double)`

Asserted invariants:

- reference: `audit.baseline` field-wise equals the supplied baseline audit (`surface_mass`, `deficit_mass`, `total_mass`, `wet_cell_count`);
- snapshot: `audit.baseline` field-wise equals `CouplingSnapshot::compute_system_mass(h_wet)`;
- both: `audit.current` field-wise equals `CouplingState::compute_system_mass(h_wet)`;
- conserved: `audit.conserved == true`, `audit.residual == 0.0`;
- drifted: `audit.conserved == false`, `audit.residual == current.total_mass - baseline.total_mass`, equal to the fixed test fixture drift of `4.0`.

The tests close the entry hop in the runtime decision chain: `compute_system_mass -> audit.{baseline,current}`. Combined with M67 (`audit -> diagnostic`) and M68 (`diagnostic -> gate_decision`), and the earlier M61/M63/M64/M65/M66 consistency tests, the full `compute_system_mass -> audit -> diagnostic -> gate_decision -> gate_outcome -> handling_state -> should_abort` chain is now locked for both reference and snapshot baselines.

## Boundary

M69 is a pure consistency-test boundary only.

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
Total Test time (real) =   0.28 sec
```

Full serial Debug CTest:

```text
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
32/32 Test #32: test_coupling_system_mass_audit ..... Passed
100% tests passed, 0 tests failed out of 32
Total Test time (real) =   6.08 sec
```

## CI Evidence

M69 commit:

```text
c0c1e31 test(coupling): assert audit preserves baseline and current mass
```

GitHub Actions CI run:

```text
26552868070 push master success
```

Jobs:

```text
spike-isolation-check success
linux-gcc success
windows-msvc success
```

## Review Evidence

Manual boundary review found the M69 change is limited to:

- new GoogleTest cases under `tests/unit/coupling/test_coupling_core_state.cpp` that assert reference and snapshot audits preserve baseline mass (matching either the supplied baseline or `CouplingSnapshot::compute_system_mass`) and current mass (matching `CouplingState::compute_system_mass`) under conserved and drifted system-mass states.

The change does not modify production headers or source, does not add or change DTOs, and does not introduce process termination, runtime side effects, evidence writing, rollback, replay, tolerance, snapshot creation requirements, or release/merge gate handling.

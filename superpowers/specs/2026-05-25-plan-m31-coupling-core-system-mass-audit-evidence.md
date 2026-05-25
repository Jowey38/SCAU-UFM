# M31 Coupling Core System Mass Audit Evidence

## Scope

M31 adds pure system-mass audit helpers to `CouplingLib` core:

- `SystemMassAudit`
- `SystemMassDelta`
- `compute_system_mass(...)`
- `audit_system_mass_against_reference(...)`

The audit path is intentionally strict: no tolerance comparison, no state mutation, and no `CouplingState` baseline persistence.

## Normative Coverage

- §5.1 conservation reference: `audit_system_mass_against_reference(...)` reports `current.total_mass - baseline.total_mass` and marks conservation only when the strict residual is exactly zero.
- §11.4 wet/dry threshold: tests use `h_wet = 1.0e-4`; cells with `h >= h_wet` contribute surface mass, and cells below the threshold do not.
- §6.3 deficit ledger inclusion: `mass_deficit_account.volume` contributes to deficit mass for every cell, including dry cells.

## Red Test Evidence

Focused target build before implementation failed as expected because the M31 API did not exist yet:

- `SystemMassAudit` not declared
- `SystemMassDelta` not declared
- `compute_system_mass(...)` not declared
- `audit_system_mass_against_reference(...)` not declared

Command used:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_system_mass_audit --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_system_mass_audit.exe
```

## Focused Test Evidence

After implementation, the focused M31 target passed:

- Target: `test_coupling_system_mass_audit`
- Result: 10/10 tests passed

Command used:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc --target test_coupling_system_mass_audit --config Debug && build/windows-msvc/tests/unit/coupling/Debug/test_coupling_system_mass_audit.exe
```

Covered cases:

- empty cell list produces zero mass
- single wet cell computes `phi_t * h * area`
- dry cells are excluded from surface mass while deficit still counts
- `h == h_wet` is counted as wet
- deficit volumes accumulate across all cells
- invalid `h_wet`, `phi_t`, `h`, `area`, and deficit inputs throw
- unchanged state has strict zero residual
- missing surface mass produces non-zero residual
- granted surface loss matched by deficit preserves strict conservation
- negative baseline total mass throws

## Full CTest Evidence

Default full CTest runs showed transient Windows process-launch `BAD_COMMAND` failures on different tests across runs:

- First default run: `test_conservative_update` and `test_coupling_exchange_decision` reported `BAD_COMMAND`.
- Re-running those focused tests in isolation passed.
- Second default run: `test_momentum_transport` reported `BAD_COMMAND`.

The failure moved between unrelated tests, and isolated re-runs passed, indicating local concurrent process-spawn flakiness rather than a code defect.

Serial full CTest bypassed the local spawn race and passed:

```bash
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug -j1 --output-on-failure
```

Observed result:

- 32/32 tests passed
- 0 tests failed
- Initial serial run total test time: 2.27 sec
- Review re-run of the same serial command also passed 32/32, with total test time 20.78 sec

## Status

M31 implementation is validated locally with focused and serial full-suite evidence. The transient default CTest `BAD_COMMAND` behavior is recorded as a local Windows runner issue because it is non-deterministic across tests and disappears under isolated or serial execution.

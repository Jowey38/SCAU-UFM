# M39 Coupling System Mass Diagnostic Evidence Rollup

## Scope

This evidence rollup records the completed M36-M38 read-only system-mass diagnostic chain in `CouplingLib` core:

1. M36: strict conservation classification from `SystemMassDelta`.
2. M37: diagnostic value type and pure diagnostic helper.
3. M38: `CouplingState` snapshot-baseline diagnostic entry point.

This chain remains diagnostic-only. It does not introduce runtime gates, tolerances, abort/review/block states, scheduler behavior, adapter behavior, or external engine ABI integration.

## API Chain

### M36 Strict Classification

- `SystemMassConservationStatus`
  - `conserved`
  - `drifted`
- `classify_system_mass_conservation(const SystemMassDelta& delta)`

Coverage:

- returns `conserved` only when existing strict `delta.conserved` is true
- returns `drifted` when `delta.conserved` is false
- does not reinterpret residual with epsilon/tolerance
- does not trigger any control-flow action

### M37 Diagnostic Value Type

- `SystemMassConservationDiagnostic`
  - `status`
  - `residual`
  - `baseline_total_mass`
  - `current_total_mass`
- `make_system_mass_conservation_diagnostic(const SystemMassDelta& delta)`

Coverage:

- derives status through `classify_system_mass_conservation(delta)`
- copies `delta.residual`
- copies `delta.baseline.total_mass`
- copies `delta.current.total_mass`
- remains pure and read-only

### M38 State/Snapshot Diagnostic Entry Point

- `CouplingState::diagnose_system_mass_against_snapshot(const CouplingSnapshot& baseline, double h_wet) const`

Coverage:

- computes strict snapshot-baseline delta through `audit_system_mass_against_snapshot(...)`
- converts the delta into `SystemMassConservationDiagnostic`
- uses snapshot-captured baseline cells and current state cells
- does not mutate state, manage baseline lifecycle, or gate runtime execution

## Local Validation Evidence

### M36

- Red build: focused `test_coupling_system_mass_audit` target failed because `SystemMassConservationStatus` / `classify_system_mass_conservation(...)` did not exist.
- Focused test: `test_coupling_system_mass_audit` passed 12/12.
- Full suite: serial CTest passed 32/32 in 5.36 sec.
- Code review: no findings.

### M37

- Red build: focused `test_coupling_system_mass_audit` target failed because `make_system_mass_conservation_diagnostic(...)` did not exist.
- Focused test: `test_coupling_system_mass_audit` passed 14/14.
- Full suite: serial CTest passed 32/32 in 5.93 sec.
- Code review: no findings.

### M38

- Red build: focused `test_coupling_core_state` target failed because `CouplingState::diagnose_system_mass_against_snapshot(...)` did not exist.
- Focused test: `test_coupling_core_state` passed 23/23.
- Full suite: serial CTest passed 32/32 in 7.75 sec.
- Code review: no findings.

## Remote CI Evidence

| Milestone | Commit | CI Run | Result | Jobs |
|---|---:|---:|---|---|
| M36 | `b0d8f26` | `26425566669` | success | `spike-isolation-check`, `linux-gcc`, `windows-msvc` |
| M37 | `c1cb4ab` | `26432697401` | success | `spike-isolation-check`, `linux-gcc`, `windows-msvc` |
| M38 | `8bba170` | `26441377364` | success | `spike-isolation-check`, `linux-gcc`, `windows-msvc` |

## Boundary Confirmation

The M36-M38 diagnostic layer preserves these boundaries:

- no `epsilon_mass` tolerance path
- no runtime conservation gate
- no `ABORT`, `REVIEW_REQUIRED`, `BLOCK_MERGE`, or `BLOCK_RELEASE` state transitions
- no scheduler integration
- no adapter integration
- no SWMM, D-Flow FM, or external engine ABI dependency
- no mutation during diagnostic calls
- no baseline lifecycle ownership

## Status

M36-M38 establish a strict, read-only diagnostic layer on top of the M31-M35 system-mass audit evidence chain. A later runtime gate or operator action path can build on these diagnostics without changing their semantics.

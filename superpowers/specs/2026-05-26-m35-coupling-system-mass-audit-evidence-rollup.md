# M35 Coupling System Mass Audit Evidence Rollup

## Scope

This evidence rollup records the completed M31-M34 system-mass audit chain in `CouplingLib` core:

1. M31: pure system mass helpers and value types.
2. M32: current `CouplingState` mass audit entry points.
3. M33: captured `CouplingSnapshot` mass audit entry point.
4. M34: snapshot-baseline vs current-state delta audit entry point.

No runtime conservation gate, tolerance, automatic baseline lifecycle, scheduler integration, adapter integration, or external engine ABI connection is included in this chain.

## API Chain

### M31 Pure Helpers

- `SystemMassAudit`
- `SystemMassDelta`
- `compute_system_mass(const std::vector<ExchangeCellState>& cells, double h_wet)`
- `audit_system_mass_against_reference(const SystemMassAudit& baseline, const std::vector<ExchangeCellState>& current_cells, double h_wet)`

Coverage:

- surface mass is `sum(phi_t * h * area)` for cells where `h >= h_wet`
- deficit mass includes `mass_deficit_account.volume` for all cells
- total mass is surface plus deficit
- residual is `current.total_mass - baseline.total_mass`
- conservation uses strict `residual == 0.0`

### M32 Current State Entry Points

- `CouplingState::compute_system_mass(double h_wet) const`
- `CouplingState::audit_system_mass_against_reference(const SystemMassAudit& baseline, double h_wet) const`

Coverage:

- audits current `CouplingState` cells without mutating state
- accepts caller-provided baseline audit
- does not own or persist baseline lifecycle

### M33 Snapshot Entry Point

- `CouplingSnapshot::compute_system_mass(double h_wet) const`

Coverage:

- audits captured snapshot cells
- remains stable after later `CouplingState` event replay mutates current cells

### M34 Snapshot Baseline Delta Entry Point

- `CouplingState::audit_system_mass_against_snapshot(const CouplingSnapshot& baseline, double h_wet) const`

Coverage:

- computes baseline from captured snapshot cells
- computes current mass from current `CouplingState` cells
- reuses strict `SystemMassDelta` semantics

## Local Validation Evidence

### M31

- Red build: focused `test_coupling_system_mass_audit` target failed before the M31 API existed.
- Focused test: `test_coupling_system_mass_audit` passed 10/10.
- Full suite: serial CTest passed 32/32.
- Note: default parallel local CTest showed transient Windows `BAD_COMMAND` process-launch failures on unrelated tests; isolated and serial runs passed.

Evidence file:

- `superpowers/specs/2026-05-25-plan-m31-coupling-core-system-mass-audit-evidence.md`

### M32

- Red build: focused `test_coupling_core_state` target failed because `CouplingState::compute_system_mass(...)` and `CouplingState::audit_system_mass_against_reference(...)` did not exist.
- Focused test: `test_coupling_core_state` passed 20/20.
- Full suite: serial CTest passed 32/32 in 5.50 sec.
- Code review: no findings.

### M33

- Red build: focused `test_coupling_core_state` target failed because `CouplingSnapshot::compute_system_mass(...)` did not exist.
- Focused test: `test_coupling_core_state` passed 21/21.
- Full suite: serial CTest passed 32/32 in 5.81 sec.
- Code review: no findings.

### M34

- Red build: focused `test_coupling_core_state` target failed because `CouplingState::audit_system_mass_against_snapshot(...)` did not exist.
- Focused test: `test_coupling_core_state` passed 22/22.
- Full suite: serial CTest passed 32/32 in 7.00 sec.
- Code review: no findings.

## Remote CI Evidence

| Milestone | Commit | CI Run | Result | Jobs |
|---|---:|---:|---|---|
| M31 | `f8f628e` | `26400296368` | success | `spike-isolation-check`, `linux-gcc`, `windows-msvc` |
| M32 | `032f1a5` | `26402566832` | success | `spike-isolation-check`, `linux-gcc`, `windows-msvc` |
| M33 | `5c95786` | `26406146671` | success | `spike-isolation-check`, `linux-gcc`, `windows-msvc` |
| M34 | `970a38c` | `26424517343` | success | `spike-isolation-check`, `linux-gcc`, `windows-msvc` |

## Boundary Confirmation

The M31-M34 chain remains inside `CouplingLib` core and preserves these boundaries:

- no dependency on SWMM, D-Flow FM, or any 1D engine ABI
- no cross-engine arbitration behavior
- no scheduler behavior
- no fault hook behavior
- no automatic abort/review/block gate
- no `epsilon_mass` tolerance path
- no mutation during audit calls
- no baseline ownership beyond caller-held values or snapshots

## Status

M31-M34 establish a strict, read-only system-mass audit path from pure helpers through state and snapshot entry points. The next behavior-changing slice can build on this evidence if runtime conservation gates or operator-facing diagnostics are introduced later.

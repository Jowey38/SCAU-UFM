# M265 / G7 STCF v4 to v5 Migration Evidence

Date: 2026-07-30
Base: `origin/master@7f24a77`

## Scope

G7 implements the evidence-backed subset of main Spec legacy.15.7:

- `phi_s` is renamed to `phi_t` without value or unit drift;
- legacy `psi_tensor` components map to `phi_xx/phi_xy/phi_yy`;
- required v5 fields missing from v4 receive explicit fixture defaults;
- every rename/default is recorded in a stable machine-readable report;
- the result passes the current strict STCF v5 validation gate.

The current v5 minimal slice does not yet carry `drag_a_parallel`,
`drag_a_perp`, or `drag_cd`. The migration code does not invent inaccessible
production fields. A later schema slice must extend both the legacy DTO and
report when those fields become part of `StcfDataset`.

## Architecture

`libs/stcf/migrate_v4_to_v5.*` owns a pure in-memory migration boundary.
NetCDF I/O and CLI parsing are intentionally separate. This keeps field mapping
unit-testable and prevents a file parser from silently supplying defaults.

The v4 DTO contains only fields with a locked historical mapping:

- `phi_s`;
- `psi_xx/psi_xy/psi_yy`;
- explicit edge count.

The output uses the existing v5 `StcfDataset`; therefore all current v5 shape,
finite-value, SPD, range, soil and edge checks are applied before a migration is
accepted.

## Evidence

Unit coverage:

- exact mapped-value preservation;
- explicit default reporting;
- deterministic report and output;
- wrong schema version rejection;
- shape mismatch rejection;
- invalid physical values rejected by the v5 gate.

G7 Golden coverage:

- three-cell, four-edge authored v4 fixture;
- exact `phi_s -> phi_t` and tensor-component preservation;
- strict v5 validation;
- deterministic mapping report;
- explicit `omega_edge` default evidence.

## Gate

G7 is registered `implemented, ci_gate:true` with the canonical `golden` CTest
label and GoldenSuite four-touch registration.

## Non-claims

This slice does not claim a general historical NetCDF v4 reader, arbitrary
unknown-field preservation, drag-field migration, or a production CLI upgrade
command. Those require an authorized real v4 file/schema fixture.

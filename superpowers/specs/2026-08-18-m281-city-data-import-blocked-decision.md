# M281 Real City-Data Import: BLOCKED Decision Record (Phase D-5)

Date: 2026-08-18
Decision: **BLOCKED — no implementation until an authorized upstream sample
and format contract exist**

## Basis

The completion plan (2026-08-09) requires for real city-data import:
"obtain authorized upstream sample and format contract, then importer
spike/topology/field validation; do not invent schemas."

- No authorized upstream city dataset (mesh/GIS/UGRID export, drainage
  network, land cover, soil) has been provided to the repository, and no
  format contract from a data-owning authority exists.
- M267 already landed the fail-closed entry contracts:
  `libs/stcf/import_contract` and `libs/mesh/quality` reject any claim of a
  real importer until a governed source format is bound (no real importer
  is claimed by the codebase).
- M262's decision stands: external GIS/UGRID import is deferred until an
  authorized upstream sample exists; inventing a JSON/YAML/GeoJSON schema
  would overstate capability and create an untestable contract.

## Consequence

- Phase D-5 is formally BLOCKED with this accepted decision record, which
  is the completion state the plan defines for workstreams without scope.
- The unblock path is externally owned: a project authority supplies an
  authorized sample + format commitment; then the plan's sequence applies
  (importer spike -> topology/field validation -> STCF binding -> golden).
- Until then, all runnable cases remain project-authored (write_stcf /
  scau_preproc profiles), which the G21/G22 gates already cover.

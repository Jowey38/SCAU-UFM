# D-Flow FM BMI 1.0 spike report

**Status:** TBD — spike run not yet executed (M29 = host skeleton only).

## §3.1 Lifecycle

TBD

## §3.2 Time advance

TBD

## §3.3 State read / write

TBD

## §3.4 Hot-start / state save

TBD

## §3.5 Error / exception

TBD

## §3.6 Version stability

TBD

## §4 Must-document

- Thread safety: TBD
- Memory ownership: see `evidence/abi_gotchas.md` (partial; `get_var` pointer
  lifetime is critical)
- Units: TBD
- Domain / sentinels: TBD
- OS resources: TBD (working directory dependence noted)
- External deps: TBD (netCDF, MPI, PETSc, MKL likely)

## Exit-criteria checklist (§7)

- [ ] §3 all assumptions have replayable evidence
- [ ] §4 six must-document items answered
- [ ] `interface_gap_matrix.md` statuses finalised (no `TBD` left)
- [ ] each `GAP_BLOCKING` has concrete §7 redesign proposal
- [ ] host binary runs 100 steps clean from `cases/`
- [ ] `abi_gotchas.md` covers all 6 categories

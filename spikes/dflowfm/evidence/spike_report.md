# D-Flow FM BMI 1.0 spike report

**Status:** Runtime-readiness boundary exists in the main graph, but the real
external D-Flow FM kernel/case run is still blocked. G11
`dflowfm_river_steady` must remain `pending` and `ci_gate:false` until a
replayable 100-step run from `spikes/dflowfm/cases/` is recorded here.

## §3.1 Lifecycle

Main-graph readiness: `DFlowFMEngine` now runtime-loads the BMI library and
fails closed when the library is missing, when initialization receives an empty
`.mdu` path, or when a second initialized instance would own the process-global
BMI state.

Real-kernel evidence: TBD. The spike host has not yet been executed against a
provided Delft3D/D-Flow FM build and model case.

## §3.2 Time advance

Main-graph readiness: `DFlowFMEngine::update(dt_dfm)` validates finite positive
`dt_dfm`, calls BMI `update(double)`, and refreshes `get_current_time` after a
successful step.

Real-kernel evidence: TBD. A G11 spike must verify that `update(dt)` honors the
caller-supplied step and must record `get_current_time` over 100 steps. The
spike host now supports `--steps`, `--dt`, and `--trace-out <file>` so that the
step trace can be archived directly instead of copied from terminal output. The
trace summary records completed steps, requested steps, last time, expected last
time, and maximum observed `dt` absolute error. A non-zero `update()` or
`finalize()` return now makes the host exit non-zero, so a scripted spike run can
reject partial traces.

## §3.3 State read / write

Main-graph readiness: `DFlowFMEngine` resolves BMI `get_var`/`set_var` at
runtime. Reads treat `get_var` as an engine-owned double buffer and index by
`location_id`; writes currently support scalar/location-0 `set_var` only.

Real-kernel evidence: TBD. Before G11 can become an executable runtime Golden,
the spike must enumerate real variable names, shapes, units, and the correct
lateral-discharge/stage variables, and record them in
`evidence/var_inventory.md`. The spike host now emits a markdown inventory table
for this purpose; a future real-runtime pass should paste that output into the
inventory file and then annotate roles/notes. Non-scalar writes require
`set_var_slice` evidence before use.

## §3.4 Hot-start / state save

Still blocking. BMI 1.0 does not expose save/restore state. Any G11/G12 replay
claim needs either a documented D-Flow FM-specific hot-start path or an explicit
Phase-2 limitation recorded in the evidence.

## §3.5 Error / exception

Main-graph readiness: loader, missing-symbol, invalid-use, initialize/update,
and finalize failures are converted to `DFlowFMEngineError` with
machine-readable `engine_id="DFlowFM"` and `dflowfm_*` error codes.

Real-kernel evidence: TBD. The spike must record representative non-zero return
codes and any log callback diagnostics emitted by the real kernel.

## §3.6 Version stability

TBD. The runtime adapter resolves the BMI 1.0 symbol set by name and does not
expose third-party headers through public SCAU-UFM DTOs, but no real D-Flow FM
version has been locked for G11 yet.

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

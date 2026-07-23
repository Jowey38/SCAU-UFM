# D-Flow FM BMI 1.0 spike report

**Status:** Real D-Flow FM release runtime and a local 1D companion case were
executed successfully on 2026-07-19. The read-only 100-step trace completed
with `last_time=6000`, `expected_last_time=6000`, `max_dt_abs_error=0`, and
`time_trace_valid=true`. A follow-up real-runtime pass also validated compound
lateral write/read/restore through `laterals/lat1/water_discharge` and another
100 exact steps. G11 remains `pending` / `ci_gate:false` because the local
Deltares Flow1D companion files are not redistributable project fixtures and
BMI 1.0 has no hot-start/save-state path for replay evidence.

## §3.1 Lifecycle

Main-graph readiness: `DFlowFMEngine` now runtime-loads the BMI library and
fails closed when the library is missing, when initialization receives an empty
`.mdu` path, or when a second initialized instance would own the process-global
BMI state.

Real-kernel evidence: release `dflowfm.dll` loaded with its installed runtime
dependencies; `initialize("Flow1D.mdu")` returned 0, constructed 186 1D flow
nodes / 190 links, wrote initial output, and `finalize()` returned 0. The local
case includes `Flow1D_net.nc`, `IniCondition.ini`, and `initialfields.ini`.

## §3.2 Time advance

Main-graph readiness: `DFlowFMEngine::update(dt_dfm)` validates finite positive
`dt_dfm`, calls BMI `update(double)`, and refreshes `get_current_time` after a
successful step.

Real-kernel evidence: `single_reach.trace.txt` records 100 calls to
`update(60)` and `get_current_time`; every observed increment is exactly 60 s.
Summary: `completed_steps=100`, `requested_steps=100`, `last_time=6000`,
`expected_last_time=6000`, `max_dt_abs_error=0`, `time_trace_valid=true`.
Stage `s1[0]` remained finite through the run and ended at 9.938400.

## §3.3 State read / write

Main-graph readiness: `DFlowFMEngine` resolves BMI `get_var`/`set_var` at
runtime. Reads treat `get_var` as an engine-owned double buffer and index by
`location_id`; writes currently support scalar/location-0 `set_var` only.

Real-kernel evidence: 195 variables captured in
`var_inventory.captured.md`. Confirmed rank-1 state names include `s1`
(water level, shape 186), `hs` (depth, shape 186), and `q1` / `q1_main`
(discharge, shape 190). `laterals` is confirmed as a compound object:
`get_var_shape("laterals")` returns `[1,1]` for the derived one-lateral case,
and `laterals/lat1/water_discharge` supports double write/read/restore through
`set_var` (0 -> 0.125 -> 0). Top-level `set_var_slice("laterals", ...)` is not
the applicable API. The runtime also exposes two ABI limitations: no
`get_var_units` symbol, and `get_var_shape` access-violates for some rank>1
variables (first reproduced on `bodsed`).

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

See also `evidence/runbook.md` for the exact capture procedure once a real
runtime library and `.mdu` case are available.

## Exit-criteria checklist (§7)

- [ ] §3 all assumptions have replayable evidence
- [ ] §4 six must-document items answered
- [ ] `interface_gap_matrix.md` statuses finalised (no `TBD` left)
- [ ] each `GAP_BLOCKING` has concrete §7 redesign proposal
- [x] host binary runs 100 read-only steps clean from the local `cases/single_reach/` companion set
- [ ] `abi_gotchas.md` covers all 6 categories

# D-Flow FM BMI `vol1` volume contract spike (M258)

Conclusion level: **CONFIRMED**

`vol1` is the per-flow-node current water volume in m^3. `sum(vol1)` over all
flow nodes is usable as the D-Flow FM subsystem total-water term of the
`whole_system_mass` audit.

## Run configuration

| Item | Value |
|---|---|
| Date | 2026-07-28 |
| Runtime | `Delft3D-main/install_fm-suite_release/bin/dflowfm.dll` (2026-07-19 release build), dependent DLLs resolved via PATH |
| Host | `spikes/dflowfm/host/dflowfm_spike_host.cpp` built Release, VS 17 2022 x64, MSVC 19.44, build dir `H:/scau-vb` |
| Case | `spikes/dflowfm/cases/single_reach_1d/single_reach.mdu` (project-authored: 1000 m closed 1D reach, 11 flow nodes, width 5 m, `WaterLevIni=1.0`, `BedlevUni=0.0`, lateral `lat1` at chainage 500 m with file discharge 0.0) |
| Stepping | 10 x `update(60.0)`; engine_dt=60 s; `completed_steps=10`, `max_dt_abs_error=0`, `time_trace_valid=true` in both runs |
| Probe | New host option `--probe-sum-vars vol1,vol0,hs,ba,qext,vextcum` (sum/min/max/finite over full rank-1 arrays, copied out of engine memory immediately after `get_var`) |
| Experiment A (control) | `--skip-boundary-write`, no lateral write. Trace: `vol1_expA_control.trace.txt` |
| Experiment B (injection) | `--skip-boundary-write --inject-lateral-id lat1 --inject-lateral-q 0.125` -> writes `laterals/lat1/water_discharge = 0.125` before every `update(60)`. Trace: `vol1_expB_lateral.trace.txt` |

Exact commands (run from the case directory, PATH prefixed with the release
`bin/`):

```bash
dflowfm_spike_host.exe single_reach.mdu --steps 10 --dt 60 \
  --skip-boundary-write --stage-var s1 \
  --probe-sum-vars vol1,vol0,hs,ba,qext,vextcum \
  --trace-out .../vol1_expA_control.trace.txt

dflowfm_spike_host.exe single_reach.mdu --steps 10 --dt 60 \
  --skip-boundary-write --stage-var s1 \
  --inject-lateral-id lat1 --inject-lateral-q 0.125 \
  --probe-sum-vars vol1,vol0,hs,ba,qext,vextcum \
  --trace-out .../vol1_expB_lateral.trace.txt
```

## Raw sum tables

All values in m^3 (vol1, sum_hs_ba) unless noted. `step 0` = after
`initialize`, `step k` = after the k-th `update(60)`. Array length for
vol1/vol0/hs/ba in this case: 11 (all finite, 11/11, every step).

### Experiment A (control, closed reach, no injection)

| step | sum(vol1) | sum(hs*ba) | sum(vol0) |
|---|---|---|---|
| 0 | 5500.000000000000 | 5500.000000000000 | 5500.000000000000 |
| 1 | 5500.000000000000 | 5500.000000000000 | 5500.000000000000 |
| 2 | 5500.000000000000 | 5500.000000000000 | 5500.000000000000 |
| 3 | 5500.000000000000 | 5500.000000000000 | 5500.000000000000 |
| 4 | 5500.000000000000 | 5500.000000000000 | 5500.000000000000 |
| 5 | 5500.000000000000 | 5500.000000000000 | 5500.000000000000 |
| 6 | 5500.000000000000 | 5500.000000000000 | 5500.000000000000 |
| 7 | 5500.000000000000 | 5500.000000000000 | 5500.000000000000 |
| 8 | 5499.999999999990 | 5499.999999999990 | 5499.999999999990 |
| 9 | 5499.999999999990 | 5499.999999999990 | 5499.999999999990 |
| 10 | 5499.999999999990 | 5499.999999999990 | 5499.999999999990 |

Closed-reach drift over 10 steps: -1.0e-11 m^3 (relative -1.8e-15), i.e.
floating-point noise only. Geometric check: 11 nodes x 100 m x 5 m x 1.0 m
depth = 5500 m^3, matched exactly at t=0.

### Experiment B (lateral injection 0.125 m^3/s at lat1)

| step | sum(vol1) | sum(hs*ba) | delta sum(vol1) |
|---|---|---|---|
| 0 | 5500.000000000000 | 5500.000000000000 | - |
| 1 | 5507.500000000000 | 5507.500000000000 | +7.500000000000 |
| 2 | 5515.000000000000 | 5515.000000000000 | +7.500000000000 |
| 3 | 5522.500000000000 | 5522.500000000000 | +7.500000000000 |
| 4 | 5530.000000000000 | 5530.000000000000 | +7.500000000000 |
| 5 | 5537.500000000000 | 5537.500000000000 | +7.500000000000 |
| 6 | 5545.000000000000 | 5545.000000000000 | +7.500000000000 |
| 7 | 5552.500000000000 | 5552.500000000000 | +7.500000000000 |
| 8 | 5560.000000000000 | 5560.000000000000 | +7.500000000000 |
| 9 | 5567.500000000000 | 5567.500000000000 | +7.500000000000 |
| 10 | 5575.000000000000 | 5575.000000000000 | +7.500000000000 |

## Analysis

1. `sum(vol1)` vs `sum(hs*ba)`: identical to all 15 printed significant
   digits at every step of both experiments (relative difference 0.0 as
   computed in double precision from the traces). On this uniform-width
   rectangular 1D case, `vol1[i] = hs[i] * ba[i]` holds per node, confirming
   `vol1` has volume semantics (m^3) tied to the per-node wet area `ba` and
   water depth `hs`. On cases with non-rectangular cross-sections or 2D
   subgrid bathymetry the equality will not stay exact; `vol1` remains the
   authoritative volume, `hs*ba` only a geometric plausibility check.
2. Differential conservation (B minus A): per-step
   `delta sum(vol1)_B - delta sum(vol1)_A = 7.500000000000` m^3 for all 10
   steps (worst per-step error +1.0e-11 m^3, from the step-8 FP drift in the
   control run). Expected: `0.125 m^3/s * 60 s = 7.5 m^3`. Ten-step total:
   75.000000000010 m^3 vs expected 75 (relative error +1.3e-13). The lateral
   source enters `sum(vol1)` exactly, with no unexplained boundary term.
3. `vol0` equals `vol1` at every probe point (probe runs after `update`
   returns, when the start-of-step volume has been rolled forward), so `vol0`
   adds no independent information for an end-of-step audit; use `vol1`.
4. `qext` and `vextcum` appear in the BMI inventory but `get_var` returned a
   null pointer for both in this case (arrays not allocated; likely only
   populated for qext-type external forcings, not `[Lateral]` blocks). They
   were NOT needed: the differential A/B design already isolates the source
   term. Do not build the audit contract on `qext`/`vextcum` availability.

## Contract statement for whole_system_mass

- D-Flow FM subsystem total water volume term:
  `V_dflowfm = sum_i vol1[i]` over the full `vol1` array (rank-1 double,
  length = number of flow nodes, engine-owned pointer copied immediately
  after `get_var` and before any other BMI call).
- Sampling point: after `update` returns (end-of-step state).
- Conservation quality demonstrated: closed system drift ~1e-11 m^3 per
  10 min; source-term accounting exact to ~1e-13 relative over 10 steps.

## Limits of this evidence

- One authored 11-node 1D case, one release DLL build, 10 steps. No 2D or
  mixed-dimension mesh, no open boundary in the runs (boundary terms were
  eliminated by A/B differencing, not measured).
- `qext`/`vextcum` null-pointer behavior is case-dependent evidence, not a
  general claim about all configurations.

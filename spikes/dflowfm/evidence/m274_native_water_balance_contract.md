# M274 Native Water-Balance Contract Evidence

Date: 2026-08-15

## Runtime

- Governed source checkout: `H:/githubcode/SCAU-UFM/Delft3D-main`
- Governed build root: `H:/dflow-m275-build`
- Compiler: Intel Fortran 2026.1.0 with Visual Studio 2022
- DLL: `H:/dflow-m275-build/dflowfm_lib/Release/dflowfm.dll`
- Bridge source: `H:/scau-m274/spikes/dflowfm/bridge/dflowfm_water_balance_bridge.F90`
- Export verified with MSVC `link /dump /exports`:
  `dflowfm_get_water_balance_v1`
- The bridged DLL was staged into the known-good runtime bin before the runs.

The governed top-level build still returns failure because the unrelated
`test_dflowfm_kernel_gtest` discovery step exits `0xc0000135` when its runtime
DLL set is incomplete. The `dflowfm_dll` target itself linked successfully and
was verified independently by its changed timestamp, SHA-256, and export table.

## Executed matrix

### Closed control: 10 x 60 s

Case: `cases/single_reach_1d/single_reach.mdu`

Trace: `cases/single_reach_1d/m274_closed_dt60.trace.txt`

- ABI symbol: available
- ABI version/size/components: valid on every snapshot
- Native time: 0 through 600 s, exact observed 60 s increments
- Storage: 5500 m3 at initialization and 5499.99999999999 m3 at the end
- All classified cumulative input/output components: zero
- Native cumulative values: monotonic
- Result: PASS; closed drift is at floating-point / native volume-error scale

### Open boundary: 10 x 60 s

Case: `cases/single_reach_open_boundary/single_reach_open.mdu`

Trace: `cases/single_reach_open_boundary/m274_open_dt60.trace.txt`

- Native time: 0 through 600 s, exact observed 60 s increments
- `boundary_in_m3`: 0 -> 75.0 m3
- Expected forcing: 0.125 m3/s x 600 s = 75.0 m3
- Lateral, source, qext, rain, and groundwater components: zero
- Result: PASS for native boundary classification and 75 m3 cumulative input

### Internal lateral: 10 x 60 s

Case: `cases/single_reach_1d/single_reach.mdu`, registered `lat1`

Trace: `cases/single_reach_1d/m274_lateral_dt60.trace.txt`

- BMI lateral write/read/restore: PASS (`lat1`, 0 -> 0.125 -> 0)
- Native time: 0 through 600 s, exact observed 60 s increments
- `lateral_1d_in_m3`: 0 -> 75.0 m3 for 0.125 m3/s
- `boundary_in_m3`: zero
- Result: PASS for native lateral classification and provenance-relevant separation from boundary

The separately documented `single_reach_lateral/` local runtime asset directory
was absent, so the existing authored `single_reach_1d` case and its verified
`lat1` registration were used instead.

### Actual-dt matrix

Case: `single_reach_open_boundary/single_reach_open.mdu`

- `dt=120`, 5 steps: PASS; observed time 600 s, `time_trace_valid=true`
- `dt=30`, 1 step: EXPECTED FAIL; engine observed 60 s, host returned nonzero and
  `time_trace_valid=false`

The host uses observed engine time and does not integrate native cumulative
quantities from requested caller dt.

### Outflow-only boundary: 10 x 60 s (2026-08-14 completion session)

Case: `cases/single_reach_open_boundary/single_reach_outflow.mdu`
(authored variant: `WaterLevIni=1.5`, downstream `waterlevelbnd=1.0` only, no
discharge boundary)

Trace: `cases/single_reach_open_boundary/m274_outflow_dt60.trace.txt`

- Native time: 0 through 600 s, exact observed 60 s increments
- `boundary_in_m3`: exactly 0 on every snapshot
- `boundary_out_m3`: monotonic 0 -> 2827.75840699658 m3
- Storage decrease equals `boundary_out_m3` exactly (volerr at 1e-12 scale)
- All other components zero
- Result: PASS; out-classification proven independent of inflow

### Mixed boundary + lateral: 10 x 60 s (2026-08-14 completion session)

Case: `cases/single_reach_open_boundary/single_reach_open_lateral.mdu`
(authored: upstream `dischargebnd` 0.125 m3/s + downstream `waterlevelbnd` 1.0
+ registered `lat1` on `branch_main` at 500 m; combined fileVersion 2.02 ext)

Trace: `cases/single_reach_open_boundary/m274_open_lateral_dt60.trace.txt`

- BMI lateral write/read/restore: PASS (`lat1`, 0 -> 0.125 -> 0)
- Host injects `laterals/lat1/water_discharge = 0.125` before every update
- At t=600: `boundary_in_m3 = 75.0` exact, `lateral_1d_in_m3 = 75.0` exact,
  `boundary_out_m3 = 114.193781521175` (nonzero downstream outflow)
- Closure: 5500 + 75 + 75 - 114.193781521175 = 5535.80621847882 = storage, exact
- Boundary and lateral classes accumulate independently with no cross-talk
- Result: PASS for mixed-forcing classification separation and closure

Note: the previously recorded open-boundary run (`m274_open_dt60.trace.txt`)
is itself a mixed in/out boundary case: `boundary_in` 0 -> 75.0 and
`boundary_out` 0 -> 57.0803981715844 accumulate simultaneously and close
against storage exactly.

### Restart A/B continuity (2026-08-14 completion session)

- Leg A: `single_reach_open.mdu`, 20 x 60 s continuous,
  trace `m274_open_restartA_20x60.trace.txt`; run regenerates the native
  restart file at t=600 (`single_reach_open_20260808_001000_rst.nc`).
- Leg B: authored `single_reach_open_restart600.mdu`
  (`RestartFile` = that rst, `RestartDateTime=20260808001000`, `TStart=600`
  per the known restore-state-not-time engine behavior), 10 x 60 s,
  trace `m274_open_restartB_10x60.trace.txt`.

| Quantity | Leg A | Leg B | Verdict |
|---|---|---|---|
| storage at t=600 | 5517.91960182841 | 5517.91960182841 (after-initialize) | bit-identical |
| storage at t=660 | 5511.43279502614 | 5511.43279502614 | bit-identical |
| storage at t=1200 | 5479.62648451149 | 5479.62648451149 | bit-identical |
| boundary_in over [600,1200] | 150 - 75 = 75.0 | 75.0 | exact |
| boundary_out over [600,1200] | 170.373515488506 - 57.0803981715844 = 113.2931173169216 | 113.293117316922 | equal to fp accumulation order (~1e-12) |
| cumulative counters at restart initialize | (continuing) | ALL ZERO | counters are per-initialize |

- Result: PASS. Restart restores hydraulic state bit-identically and
  per-window external-flux deltas replay to floating-point accumulation error.
- CONTRACT CONSEQUENCE: native cumulative counters reset to zero on every
  `initialize`, including restart reload. Any provider MUST snapshot and
  re-baseline cumulative components at reload; raw counters must never be
  compared across an initialize boundary.

### qext: BLOCKED (engine-side, re-confirmed on the bridged DLL)

Trace: `cases/single_reach_open_boundary/m274_qext_probe.trace.txt`

- BMI `get_var` returns null for `qext`, `qextreal`, `vextcum` at
  initialization and after every update (array never allocated for file-based
  forcing; BMI 1.0 cannot allocate or write it).
- No project-authored runnable qext forcing path exists at BMI level; this
  matches the M273 verdict and is now re-confirmed against the bridged DLL.
- Native cumulative qext classes remain observable through the ABI and stay
  zero in all executed cases.
- Consequence: qext positive/negative exercises are BLOCKED at engine level.
  The provider contract must treat `qext_*`, `rain/evaporation`, and
  `groundwater` components as observed-zero guards for SCAU-UFM cases (our
  coupling exchanges use only boundary and lateral classes) and fail closed
  if any of them becomes nonzero.

## Phase A verdict

The executed matrix now covers: closed control, inflow+outflow mixed boundary,
outflow-only, boundary+lateral mixed with classification separation, actual-dt
behavior (dt=120 pass, dt=30 failure-revealing), and restart A/B continuity
with the re-baseline contract. qext forcing is engine-blocked and guarded.

This closes the Phase A external-flux contract for the scope SCAU-UFM
actually exchanges (boundary + lateral + storage + volume error). G27
implementation (driver-owned provider + golden) is now justified; G19
promotion remains blocked until G27 evidence lands.

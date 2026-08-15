# M273 authored open-boundary spike case

This is a project-authored diagnostic case for the D-Flow FM external-boundary contract spike. It reuses the authored `single_reach_1d/single_reach_net.nc` geometry and adds:

- upstream constant `dischargebnd`;
- downstream constant `waterlevelbnd`;
- a zero-discharge control with the same downstream stage.

The case is spike-only. It is not a production fixture, GoldenSuite test, or external-net provider contract.

Run from this directory with the release D-Flow FM `bin` directory on the Windows DLL search path:

```bash
H:/scau-vb273/Release/dflowfm_spike_host.exe single_reach_open.mdu \
  --steps 10 --dt 60 --skip-boundary-write --stage-var s1 \
  --probe-sum-vars vol1,qext,qextreal,vextcum,q1 \
  --probe-values-vars q1 \
  --probe-int-values-vars kcu \
  --probe-int-matrix-vars ln \
  --trace-out m273_external_boundary_indexed.trace.txt
```

The boundary topology diagnostic identifies `kcu=-1` at q1 indices 10 and 11 and
reports their `ln` node pairs. This mapping is evidence for this authored case,
not a production-wide boundary-ID contract. Caller `dt=30` is intentionally
failure-revealing: the runtime advances by its internal 60 s and the host marks
`time_trace_valid=false`; caller `dt=120` advances by 120 s. A provider must
therefore govern engine dt before integrating boundary fluxes.

The committed traces are the raw compact BMI observations. Generated D-Flow FM map/history/restart output and cache files are ignored.

## M274 completion-matrix variants (authored)

- `single_reach_outflow.mdu` / `.ext`: outflow-only (`WaterLevIni=1.5`,
  downstream stage 1.0 only). Proves `boundary_out` classification with
  `boundary_in` exactly zero.
- `single_reach_open_lateral.mdu` / `.ext`: both boundaries plus registered
  `lat1` (combined fileVersion 2.02 ext). Run with
  `--inject-lateral-id lat1 --inject-lateral-q 0.125` to prove
  boundary/lateral classification separation and exact closure.
- `single_reach_open_restart600.mdu`: restart leg B (`RestartFile` = t=600 rst
  from a leg-A run of `single_reach_open.mdu`, `TStart=600`). Leg A/B traces
  prove bit-identical state restore, fp-exact per-window flux replay, and the
  per-initialize reset of native cumulative counters.
- `m274_qext_probe.trace.txt`: re-confirms `qext`/`qextreal`/`vextcum` are
  unavailable through BMI on the bridged DLL (engine-side qext BLOCKED).

All M274 runs add `--probe-native-water-balance` and require the bridged
`dflowfm.dll` exporting `dflowfm_get_water_balance_v1` on the DLL search path.

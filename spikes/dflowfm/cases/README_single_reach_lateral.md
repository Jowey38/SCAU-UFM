# Local single_reach lateral runtime variant

This recipe creates the local, non-redistributed case used to verify the real
D-Flow FM BMI compound lateral write contract. The base `Flow1D.mdu` and
companions come from a local Deltares checkout and remain gitignored; only this
project-authored recipe is tracked.

## Local companion set

Copy these local Deltares test assets into
`spikes/dflowfm/cases/single_reach_lateral/`:

- `Flow1D.mdu`
- `Flow1D_net.nc`
- `IniCondition.ini`
- `initialfields.ini`

Set this MDU field:

```ini
[external forcing]
ExtForceFileNew = Flow1D.ext
```

Add `Flow1D.ext`:

```ini
[General]
fileVersion = 2.02
fileType = extForce

[Lateral]
id = lat1
locationType = 1d
nodeId = nameKNP_1
discharge = 0.0
```

`nameKNP_1` is a real `network_node_id` in the local `Flow1D_net.nc`.
The constant zero discharge registers the lateral without perturbing the base
case; BMI then exposes it as `laterals/lat1/water_discharge`.

## Verification command

Run the release host beside the installed runtime DLLs:

```bash
dflowfm_spike_host.exe Flow1D.mdu \
  --steps 100 \
  --dt 60 \
  --skip-boundary-write \
  --stage-var s1 \
  --verify-lateral-id lat1 \
  --trace-out single_reach_lateral.trace.txt
```

Expected evidence:

```text
lateral_write_restore_valid=true variable=laterals/lat1/water_discharge before=0 probe=0.125 restored=0
completed_steps=100 requested_steps=100 last_time=6000 expected_last_time=6000 max_dt_abs_error=0 time_trace_valid=true
```

The host restores the original lateral value before advancing time. This is an
ABI write/read/restore check, not a hydraulic forcing experiment.

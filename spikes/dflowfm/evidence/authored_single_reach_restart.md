# Real D-Flow FM restart checkpoint evidence

Date: 2026-07-23

Case: project-authored `single_reach_1d`.

The base run used `RstInterval = 600` and produced native `*_rst.nc` files.
A generated restart MDU selected the 600 s checkpoint and set:

```ini
[time]
TStart = 600

[restart]
RestartFile = DFM_OUTPUT_single_reach/single_reach_20260723_001000_rst.nc
```

The release BMI DLL initialized the checkpoint successfully. One subsequent
`update(60)` produced:

```text
t0=600
last_time=660
expected_last_time=660
max_dt_abs_error=0
time_trace_valid=true
```

Important boundary: `RestartFile` alone restores hydraulic state but BMI time
starts from the MDU `TStart`; the checkpoint coordinator must persist logical
time and override `TStart` to the checkpoint time.

This evidence supports file checkpoint reload via `finalize` + generated MDU +
`initialize`. It does not imply in-memory or arbitrary sub-checkpoint rollback.

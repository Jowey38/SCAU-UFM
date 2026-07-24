# D-Flow FM caller-dt failure-revealing evidence

Date: 2026-07-23

The authored case reports `engine_dt=60`. A 1000-call probe requested
`update(6)` to test sub-internal-step scheduling.

Observed behavior:

- early calls advanced by 60 s, not 6 s;
- after reaching model end time, later calls returned success while time stopped;
- final time happened to equal 6000 s, but `max_dt_abs_error=54`;
- therefore cumulative final-time equality alone is insufficient evidence.

The spike host previously marked any finite time trace valid. It now sets
`time_trace_valid=false` whenever an observed increment differs from requested
dt by more than 1e-9, and exits non-zero.

Failure-revealing result:

```text
completed_steps=1000
requested_steps=1000
last_time=6000
expected_last_time=6000
max_dt_abs_error=54
time_trace_valid=false
host_exit=1
```

Control run `100 x update(60)` remains valid with zero error.

Operational contract: the current real D-Flow FM build must be scheduled at its
reported 60 s step for deterministic coupling. SCAU-UFM must not request 6 s
substeps and assume BMI `update(dt)` honours them merely because it returns 0.

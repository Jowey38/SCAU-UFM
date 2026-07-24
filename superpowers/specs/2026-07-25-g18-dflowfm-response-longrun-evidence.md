# G18 real lateral response and 1000-step long-run evidence

Date: 2026-07-25

Status: G18 `dflowfm_lateral_response` implemented, `ci_gate:false`
(skips explicitly without the governed real runtime env).

## G18 controlled physical response

Two identical runs of the authored `single_reach_1d` case (20 x update(60)),
strictly finalized between runs:

- baseline: `laterals/lat1/water_discharge = 0`
- forced: `laterals/lat1/water_discharge = 0.01 m3/s`

Assertions: every `s1` (11 nodes) and `q1` (10 links) value finite in both
runs, and the max |forced - baseline| across stage/flow exceeds 1e-6
(the injected 12 m3 over 1200 s corresponds to a ~2.4 mm mean stage rise on
the 1000 m x 5 m reach). Local real-runtime result: passed in 1.71 s.

This upgrades the lateral write contract from ABI-level (write/read/restore)
to hydraulic-level: a nonzero native lateral measurably changes the real
river state.

## 1000 x 60 s real long-run

The authored case `TStop` was extended to 60000 s. The strict-dt spike host
(any per-step mismatch > 1e-9 invalidates the trace and exits non-zero)
completed:

```text
completed_steps=1000 requested_steps=1000
last_time=60000 expected_last_time=60000
max_dt_abs_error=0 time_trace_valid=true
```

Trace: `spikes/dflowfm/evidence/authored_single_reach_1000.trace`.

## Case configuration note

`RstInterval` stays at 600 s so the 600 s native checkpoint used by the
checkpoint-reload Golden regenerates from a fresh run. After the TStop
extension, all five real-runtime Goldens were re-verified in one session:
river steady, dual-real shared cell, tri-coupling minimal, checkpoint reload,
and lateral response — 5/5 passed.

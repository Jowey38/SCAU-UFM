# M279 G20 Long-run Policy Implementation Evidence

Date: 2026-08-17
Base: origin/master d7fe1a3
Plan: docs/superpowers/plans/2026-08-17-m279-g20-longrun-policy.md

## What landed

G20 `dflowfm_longrun_10000` (ci_gate:false, candidate_non_gating), executed
by the self-hosted gateway (now 8 real goldens) and built by the
real-dflowfm-golden CI job. Authored long-run cases:
`single_reach_open_longrun.mdu` (TStop=600,000 s, native rst at 300,000 s)
and `single_reach_open_longrun_restart300000.mdu`.

## Local real-run evidence (bridged dflowfm.dll, Quadro-class build host)

- Leg A: 10,000 x 60 s; per-step actual-dt audit max_dt_abs_error == 0
  (exact 60 s every step); final native time 600,000 s exactly.
- Native closure at every 1,000-step sample:
  |delta storage - (boundary_in - boundary_out)| <= 1e-6 m3; cumulative
  volume error at run end = -2.68e-9 m3 (budget 1e-6).
- boundary_in = 75,000.0 m3 EXACT (0.125 m3/s x 600,000 s);
  boundary_out = 74,998.575 m3 (steady state; storage +1.425 m3).
- BMI vol1 cross-check equals native storage at run end.
- Resource/time policy: wall 1.84 s, 5,441 steps/s (budget 1,800 s).
- Leg B: restart from the native 300,000 s checkpoint — cumulative counters
  reset (M274 per-initialize contract), restored storage and t=360,000 s
  replayed storage match leg A samples within 1e-9 m3, per-step dt exact.

## Boundaries

- Non-gating until the gateway runs green on master (promotion follows the
  stability protocol, as with G11/G16/G17/G18/G27).
- Engine-only long-run contract; whole-system conservation stays G19 scope.

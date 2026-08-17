# M279: G20 long-run policy golden (Phase D-2 of the completion plan)

Date: 2026-08-17
Base: origin/master d7fe1a3 (M277 G19 promotion)

## Objective

Land G20 `dflowfm_longrun_10000` as the long-run policy gate defined by the
completion plan: 10,000-step real-engine case, per-step actual-dt audit,
restart/checkpoint replay, and a documented resource/time policy.

## Design

- Case: authored `single_reach_open_longrun.mdu` (open-boundary case extended
  to TStop=600,000 s; native rst at 300,000 s) plus
  `single_reach_open_longrun_restart300000.mdu` (TStart=300,000, RestartFile
  pinned to the leg-A checkpoint, per the M274 restore-state-not-time rule).
- Leg A (10,000 x 60 s): per-step observed dt must equal 60 s exactly
  (max_dt_abs_error == 0; the production update() dt fail-close stays armed);
  native water-balance sampled every 1,000 steps with monotonic cumulative
  classes and closure |delta storage - (in - out)| <= 1e-6 m3 (engine volume
  error budget, three orders above the observed 1e-12/step scale);
  |cumulative volume error| <= 1e-6 m3 at every sample; final BMI vol1
  cross-check against native storage; wall-clock budget 1,800 s recorded as
  the resource policy with measured steps/second printed as evidence.
- Leg B (restart replay): initialize from the native 300,000 s checkpoint;
  cumulative counters must read zero (M274 per-initialize contract); restored
  storage and the t=360,000 s replayed storage must match leg A's samples
  within 1e-9 m3; same per-step dt audit.
- Registration: `ci_gate:false` + `candidate_non_gating` first landing
  (2026-08-08 labeling rule); executed by the self-hosted gateway (now 8
  goldens) and built by the real-dflowfm-golden CI job. Promotion follows
  the stability protocol after the gateway runs green on master.

## Out of scope

- G9/CUDA (separate workstream; toolchain being provisioned).
- Any tri-coupling involvement: G20 audits the D-Flow engine's own long-run
  contract; whole-system conservation is G19's scope.

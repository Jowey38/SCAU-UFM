# M281 G27 Promotion Decision Evidence

Date: 2026-08-18
Decision: **PROMOTED — G27 `dflowfm_external_net` becomes an active gate
(`ci_gate:true`)**

## Stability-protocol basis (represented-in-CI + repeated green)

G27 executed green in the enforcing self-hosted gateway on:

- PR #73 lane and master run 31867411713 (M276 merge) — 7/7 goldens;
- master run 32005608524 (M279 merge) — 8/8 goldens;
- master run 32140995554 (M280 merge) — 8/8 goldens.

Master run 31982929275 (M277 merge) failed at the CONFIGURE step of the
gateway job during a documented host disk-full incident (bug-211 window);
the gateway never executed, so the failure carries no evidence about G27
stability. The three green master executions above are the consecutive
gateway RUNS of the test.

## Content basis

G27 verifies the production external-flux contract the G19 audit depends
on: mixed-boundary independent closure vs `sum(vol1[0:ndxi])` (<= 1e-6 m3),
API-lateral dedup exactness, and the per-initialize re-baseline contract
(M274/M276 evidence chain). It has additionally been exercised implicitly
by every scope-complete G19 run since M277.

## G20 status

G20 `dflowfm_longrun_10000` has two green master gateway executions
(32005608524, 32140995554); it stays `candidate_non_gating` until a third
green master run lands (expected from this PR's own master push), after
which the same promotion applies.

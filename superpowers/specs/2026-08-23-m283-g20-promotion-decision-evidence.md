# M283 G20 Promotion Decision Evidence

Date: 2026-08-23
Decision: **PROMOTED — G20 `dflowfm_longrun_10000` becomes an active gate
(`ci_gate:true`)**

## Stability-protocol basis (represented-in-CI + repeated green)

G20 executed green in the enforcing self-hosted gateway on three consecutive
master runs:

- 32005608524 (M279 merge) — 8/8 goldens;
- 32140995554 (M280 merge) — 8/8 goldens;
- 32633906848 (M281 merge) — 8/8 goldens.

## Content basis

G20 locks the long-run policy required by the completion plan: 10,000 x 60 s
real-engine advance with a per-step actual-dt audit (exact), native
water-balance monotonicity + closure at every 1,000-step sample (cumulative
volume error -2.7e-9 m3 measured), restart replay from the native 300,000 s
checkpoint (counters reset per the M274 contract; storage agreement <= 1e-9
m3), and a documented wall-clock budget (measured 1.84 s vs 1,800 s).

## Consequence

GoldenSuite active-gate set now includes G19, G20, and G27 alongside the
previously promoted gates; the remaining non-gating registry entries are G9
(pending: CUDA solver port per M266/M280) and G15 (real-SWMM shared cell,
historical non-gating record).

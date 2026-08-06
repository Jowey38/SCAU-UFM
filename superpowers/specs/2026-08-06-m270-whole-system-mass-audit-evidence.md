# M270 Whole-System Mass Audit Evidence

Date: 2026-08-06
Base: `origin/master@5f983e2`

## Delivered

- `surface2d/audit/mass`: production Neumaier-compensated
  `total_physical_surface_volume`, canonical `phi_t * h * A`, fail-closed on
  invalid fields and shape mismatch. `Phi_c` never enters storage accounting.
- `coupling/driver/whole_system_mass_audit`: baseline/current DTO, physical
  storage/external-net closure equation, canonical `epsilon_deficit`,
  deterministic vs real tolerance policy, scope-complete gate, and
  `DeficitAgeObservation`.
- SimDriver integration at the M269 commit boundary: t0 baseline and every
  post-replay/post-write-back epoch sample Surface2D, SWMM storage, D-Flow
  `vol1`, Surface2D external terms, and optional complete 1D external-net
  providers. Scope incomplete or residual drift refuses rollback (engines
  already advanced), freezes the commit count, and stops in
  `review_required`.
- Run summary carries per-epoch storage, residual, tolerance, verdict, deficit
  volumes/ages and final/max residual evidence.

## Corrected accounting semantics

1. `mass_deficit_account` is an unfulfilled exchange obligation, not physical
   stored water. `v_unmet` remains on Surface2D; adding deficit to physical
   storage double-counts it. Deficit remains a parallel volume/age audit.
2. Whole-system physical storage counts all `h >= 0` water. The configured
   `h_wet` threshold is used only for a separate `M_ref` diagnostic feeding
   `epsilon_deficit = max(1e-10, 1e-12 * M_ref)`. A diagnostic run showed that
   using `h_wet` in physical storage caused a 2.09e-7 m3 discontinuity when a
   coupled cell crossed near-dry; threshold 0 restores machine-precision
   closure.

## G24 deterministic complete-scope gate

G24 `whole_system_mass_audit` is registered
`implemented, ci_gate:true, Phase 2+` and golden-labeled. It uses a strict STCF
mixed-minimal case and two tracking engines:

- SWMM storage increases by accepted lateral volume (minus modeled overflow);
- D-Flow `vol1` increases by accepted lateral volume;
- both expose complete zero cumulative external-net observations.

Across 20 committed epochs:

- every epoch is scope-complete and `conserved` under strict
  `epsilon_deficit`;
- max absolute residual <= 1e-10 m3 (observed near machine precision);
- endpoint deficits are created and `deficit_age_steps >= 3` is observed
  without write-off, proving the M271 trigger is visible;
- internal transfers never appear as external source/sink terms.

## G19 real-engine scope evidence

G19 remains `implemented, ci_gate:false`. A governed local real-runtime run
measured the following over 180 s:

- Surface2D: 2.76 -> 0.864275 m3;
- SWMM node+link storage: 0.000651938 -> 89.7717 m3;
- D-Flow `sum(vol1)`: 5500 -> 5553.93 m3;
- raw storage residual under a zero-external assumption: +141.801 m3
  (epochs: +61.2922, +127.566, +141.801 m3).

This is orders of magnitude larger than numerical tolerance and confirms
missing real-engine external source/sink observations, not a floating-point
closure error. Existing governed APIs do not expose complete terms:

- SWMM internal `TRoutingTotals` contains external inflow, flooding, outflow,
  evaporation, seepage, initial/final storage, but the wrapper exposes only
  current storage; public `swmm_getMassBalErr` is final percentage error, not
  an epoch cumulative-volume API.
- D-Flow BMI `q1` lacks governed external boundary IDs/orientation;
  `qext`/`vextcum` returned null in M258 evidence.

G19 therefore asserts `scope_complete=false`, raw residual > 100 m3, and
`verdict=review_required`. It is not promoted. No tolerance was widened to
hide the missing scope.

## Verification

MSVC Debug, clean `origin/master@5f983e2` plus this change only:

- full CTest: **154/154 passed**;
- golden label: **25/25 passed** (23 baseline + G24 fixture/test);
- manifest: **1/1 passed**;
- governed real-runtime gateway: **6/6 passed**, including G19 explicit
  scope-incomplete `REVIEW_REQUIRED` evidence.

## Non-claims / release consequence

- No governed complete SWMM or D-Flow external-net runtime provider yet.
  Real whole-system conservation remains `REVIEW_REQUIRED`.
- No deficit write-off mutation/accounting/replay; M271 owns that work.
- No roof storage path in the SimDriver loop.
- G24 gates the deterministic complete-scope audit framework; it does not
  replace missing real-engine external-flux evidence.

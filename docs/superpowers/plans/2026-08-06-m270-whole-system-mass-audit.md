# M270 Plan: Whole-System Mass Audit

Date: 2026-08-06
Base: `origin/master@5f983e2`

## Goal

Assemble the first production whole-system physical-storage audit across:

- Surface2D physical water (`sum(phi_t * h * A)`);
- concrete embedded SWMM whole-domain node + link storage;
- D-Flow FM whole-domain `sum(vol1)`;
- complete cumulative external source/sink terms;
- the parallel `mass_deficit_account` obligation ledger and age evidence.

Wire the audit into the M269 committed epoch boundary and fail closed to
`review_required` when residual exceeds the applicable tolerance OR when an
enabled engine's external flux scope is incomplete.

## Critical accounting decisions

### Deficit is not physical storage

`v_unmet` remains physically in Surface2D; `mass_deficit_account` records an
unfulfilled exchange obligation. Adding deficit to physical storage would
count the same water twice. M270 therefore reports deficit volume and age as a
parallel audit, but the physical closure sum is:

`S = surface + swmm_storage + dflowfm_storage + depression_storage_delta`.

### Near-dry water remains physical water

The symbols reference defines `M_ref` over cells with `h >= h_wet` for
`epsilon_deficit`, but excluding `0 <= h < h_wet` water from the physical
closure creates a real mass discontinuity when a coupled cell crosses the
threshold. The production Surface2D helper supports a threshold; the
whole-system physical sample always uses threshold 0, while a separate
`surface_reference_volume` uses configured `h_wet` for the canonical
`epsilon_deficit = max(1e-10, 1e-12 * M_ref)`.

### Complete external scope is mandatory

A storage delta alone cannot prove conservation in an open engine. Each 1D
engine must provide a complete cumulative external net-volume observation
(inflow positive; outflow/loss negative). Missing observations do not throw
away the raw residual; they make `scope_complete=false` and force
`REVIEW_REQUIRED` regardless of tolerance.

Current real wrappers expose complete storage (`SwmmEngine::total_stored_volume`,
D-Flow `vol1`) but not complete cumulative external source/sink terms:

- SWMM internal `TRoutingTotals` has the required categories, but no governed
  wrapper currently exposes them mid-run;
- D-Flow BMI has no governed boundary ID/orientation/cumulative contract;
  `qext`/`vextcum` were null in M258 evidence and `q1` cannot be summed without
  a case-owned boundary map.

Therefore M270 adds external-net provider seams and a deterministic
complete-scope G24 gate. G19 records the real raw residual and explicitly
asserts scope-incomplete `REVIEW_REQUIRED`; it remains non-gating. No tolerance
is inflated to hide missing terms.

## Runtime gate

At the post-replay/post-write-back boundary, after checkpoint coordination but
before incrementing the committed step:

1. sample Surface2D physical/reference volumes;
2. sample SWMM storage via injected concrete provider;
3. sample D-Flow `vol1` via the existing provider;
4. sample complete external engine net terms when available;
5. evaluate the closure against t0;
6. if scope incomplete or drifted, refuse rollback (engines advanced), freeze
   commit count, and stop in `review_required`;
7. only a conserved, complete-scope report becomes a committed epoch record.

## Golden strategy

- G24 `whole_system_mass_audit`: active deterministic Phase 2+ gate. Tracking
  test engines expose complete physical storage and zero external-net scope;
  20 epochs close under strict `epsilon_deficit`, including deficit age >= 3.
- G19 `surface2d_tri_coupling_real`: remains `ci_gate:false`. It proves real
  three-model execution and now proves that real storage without complete
  external flux observations produces scope-incomplete `REVIEW_REQUIRED`.

## Write-off

M270 observes `deficit_age_steps` but does not mutate the sovereign core
ledger. `N_writeoff_steps=3`, explicit write-off volume accounting, replay,
and a dedicated Golden remain M271. Release-level conservation claims remain
`REVIEW_REQUIRED` until that evidence exists.

# M268 Plan: SimDriver Minimal Executable Tri-Model Run Loop

Date: 2026-08-03
Base: `origin/master@8eafd23`

## Goal

Turn the M267 SimDriver lifecycle skeleton into the first executable
end-to-end tri-model path: strict STCF case -> Surface2D CPU solver ->
CouplingLib tri-coupling with SWMM and D-Flow FM -> machine-readable run
summary. G22 already proves `case -> Surface2D stepping`; G17 already proves
`CouplingState -> real SWMM + real D-Flow FM`. The missing piece this
milestone adds is the adapter/orchestrator that makes `surface2d::SurfaceState`
the actual 2D state inside the coupled loop.

## Architecture

- `libs/coupling/driver/surface2d_coupling_map`: the single seam where the two
  state representations meet.
  - `build_exchange_cells`: projects the CURRENT surface state into fresh
    exchange cells (`V = phi_t * h * A`). `CouplingState` has no public cell
    mutator by design, so the loop rebuilds exchange cells each coupling epoch
    and carries the aggregate and endpoint-owned deficit ledgers over from the
    previous epoch. `RuntimeCounters` therefore become per-epoch diagnostics.
  - `apply_exchange_write_back`: post-replay storage update
    `h = V / (phi_t * A)`, `eta = z_b + h`, applied unconditionally after the
    coupled substep (never through the next surface step's
    `SourceTermFields.exchange_volume`, which could CFL-roll-back and discard
    volume an engine already consumed).
- `apps/sim_driver`: RuntimeConfig v2 (initial_eta, CFL knobs, engine mode,
  head-driven link configs), strict fail-closed key=value config parser,
  `run_simulation` epoch loop, hand-rolled JSON run summary, real CLI.

## Epoch ordering (fail-closed invariant)

1. All `dt_surface` substeps first; a CFL rollback stops the run in
   `review_required` BEFORE any engine advances (SWMM cannot rewind).
2. Project surface -> exchange cells (deficit carry-over).
3. One `advance_tri_coupling_step` with head-driven links,
   `dt_sub = dt_couple`.
4. Unconditional conservative write-back.
5. `record_committed_coupling_step()`; append epoch record to the summary.

## Momentum convention at the write-back (decision record)

Two candidate conventions were considered for the drain direction:

1. **Velocity-preserving scaling (chosen)**: `hu, hv` scale by
   `h_new / h_old`, so drained water leaves at the cell velocity and
   `u = hu/h` is invariant. Prevents unbounded velocity growth as depth drops
   (`Q_limit` only guarantees `h_new >= 0.1 * h_old`).
2. Momentum-invariant removal: only `h` decreases and `hu, hv` stay, which
   conserves momentum exactly but amplifies velocity near dryness and can
   trigger spurious CFL rollbacks.

Return direction: returned water enters with ZERO momentum (`hu, hv`
unchanged, velocity dilutes), consistent with the DischargeInflow source and
the roof overflow path. This convention bakes into G19 references; changing it
later requires a new golden decision.

## dt contract

`dt_swmm = dt_dflowfm = dt_couple` is enforced fail-closed: the real D-Flow FM
BMI `update(dt)` must equal the engine's `get_time_step()` and engine
sub-stepping is out of scope for the minimal loop.

## Gate

G19 `surface2d_tri_coupling_real` is registered `implemented, ci_gate:false`
with the `candidate_non_gating` label (G15 precedent) and runs in the
self-hosted real-D-Flow gateway. Promotion to `ci_gate:true` is planned with
the M270 whole-system mass audit assertion.

## Non-claims

- No engine sub-stepping; no dt-halving retry after CFL rollback.
- No roof path in the loop (roof + tri-coupling would double-write SWMM
  lateral inflows; arbitration is a separate milestone).
- No checkpoint/rollback integration (M269) and no whole-system engine-storage
  mass audit (M270).
- The mock integration closure covers the surface + coupling ledger only.

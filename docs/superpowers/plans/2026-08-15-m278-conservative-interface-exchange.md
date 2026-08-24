# M278: Volume-conservative 1D-1D interface + backwater mass governance

Status: IMPLEMENTED 2026-08-24 (both legs governed; G19 real-engine
re-inclusion pending a fresh self-hosted gateway promotion run — see the
evidence section at the end)

## Why

Fresh scope-complete G19 evidence (M277) proved two ungoverned mass paths in
the current explicit interface design:

- bug-208: stage-driven outfall backwater lets SWMM import volume from its
  boundary with no routing-totals class and no CouplingLib debit from D-Flow
  (+37.79 m3/epoch on the G19 fixture) — bypasses the ledger invariant.
- bug-210: outfall->river injection built from a sampled instantaneous rate
  is not volume conservative (injected 0.6207 vs emitted 0.5243 m3 across
  two epochs).

Both are currently excluded from the G19 conservation fixture; any config
re-enabling them trips the armed whole-system audit into REVIEW_REQUIRED.

## Target design (to be spiked first)

1. Emitted-volume injection: per substep N measure SWMM outfall emitted
   volume from cumulative massbal outflow deltas (post-step), inject into the
   river at substep N+1 through a CouplingLib interface buffer ledger
   (capacity-clamped; unmet volume stays in the buffer, aged like deficit).
   The buffer is a driver-owned storage term for the audit (in-flight
   volume), closing bug-210 exactly.
2. Backwater as governed reverse exchange: driving outfall stage must pair
   with measuring reverse outfall inflow volume (requires an extern/swmm5
   massbal bridge extension for reverse boundary volume) and debiting D-Flow
   through a negative lateral at the interface location, all through the
   CouplingLib ledger. Until the bridge exposes reverse outfall volume,
   stage-driving remains audit-guarded.

## Exit criteria

- Failure-revealing candidate goldens for both paths (interface conservation
  and backwater debit), then implementation, then G19-fixture re-inclusion
  of the interface leg with the audit still conserved.
- No tolerance widening; the in-flight buffer and reverse-volume terms are
  ledger-owned quantities, not tolerances.

## Implementation evidence (2026-08-24)

- Bridge: `massbal_getNodeTotalInflow/Outflow` expose the per-node cumulative
  boundary registers (ft3). For an outfall, positive register deltas are the
  emitted boundary volume; stage-driven backwater appears as negative deltas
  of the SAME register (verified against updateRoutingTotals: outfalls
  accumulate `Node[j].inflow * tStep` into NodeOutflow).
- Engine seam: `ISwmmEngine::get_node_cumulative_outflow_volume` (m3); real
  engine converts with the locked 0.02832 factor; mock carries a standing
  register fixture.
- Driver: `DrainageRiverInjectionMode::emitted_volume` + caller-owned
  `DrainageRiverInterfaceState` (emitted/reverse buffers with deficit-style
  epoch aging). Post-step register delta accrues; the NEXT substep releases
  through `evaluate_engine_interface_exchange` (capacity-clamped; unmet
  volume stays buffered); backwater is debited as a negative river lateral
  through the same core primitive. The legacy sampled-rate mode remains
  default and audit-guarded; the legacy entry point fail-closes when a link
  requests the governed mode.
- Audit ownership: `WholeSystemMassSample.interface_inflight_volume`
  (signed, emitted minus reverse) is included in the storage total; the run
  loop owns the ledger, ages it at every committed epoch, and mirrors
  reverse-debit decisions out of the lateral/internal-return accumulators.
- Failure-revealing goldens (locked BEFORE the fix on the legacy path, then
  locking the governed path):
  - G28 `interface_emitted_volume_conservation`: legacy injects 4.0 m3
    against 2.6 m3 actually emitted (1.4 m3 fabricated, bug-210); governed
    path: injected + buffered == emitted exactly (1e-9), capacity clamp only
    delays volume, first substep injects nothing (next-substep timing).
  - G29 `backwater_reverse_debit`: legacy drives the stage but never debits
    (bug-208); governed path: debited + buffered == imported exactly, debit
    is a negative lateral (q = -0.225 m3/s lock), in-flight term negative.
  Both are deterministic mock goldens, `ci_gate:true`.
- Full suite: 163/163 (29 golden-label, manifest green, LNK1168=0).

## Remaining follow-up

- G19 re-inclusion of the interface leg with `emitted_volume_injection: true`
  needs a fresh REAL-engine gateway run on the self-hosted runner (this
  golden runs real SWMM + D-Flow FM); until that promotion evidence exists
  the G19 fixture keeps the leg excluded and the audit stays armed against
  the legacy path.

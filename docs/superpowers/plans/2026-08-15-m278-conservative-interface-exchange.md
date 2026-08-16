# M278: Volume-conservative 1D-1D interface + backwater mass governance

Status: PLANNED (blocked capabilities recorded by M277; audit guards them)

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

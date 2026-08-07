# M271 Plan: Deficit Write-off Ledger

Date: 2026-08-07
Base: `origin/master@8f4eafc`

## Goal

Implement the main Spec §6.3 / stability-protocol requirement that an unpaid
`mass_deficit_account` persisting for `N_writeoff_steps=3` committed coupling
epochs is explicitly written off, counted in
`count_writeoff_volume_total`, logged with endpoint identity, snapshotted, and
replayed deterministically. Silent discard is forbidden.

## Sovereign state model

- `MassDeficitAccount` gains `age_steps`, the consecutive committed coupling
  epochs for which volume remains non-zero.
- `RuntimeCounters` gains `count_writeoff_events` and
  `count_writeoff_volume_total` (m3).
- `CouplingState::apply_deficit_writeoff` is an explicit epoch-end core API.
  It requires an empty pending-event queue and runs after replay/write-back but
  before M269 checkpoint snapshot/commit.
- Aggregate accounts and shared drainage/river endpoint accounts age and write
  off independently in deterministic cell/vector order.
- Zero/full repayment resets age to zero. Partial repayment with residual
  obligation preserves age continuity.
- Third consecutive epoch writes off the complete residual obligation,
  generates a `DeficitWriteoffRecord`, increments event/volume counters, and
  resets account volume/age to zero.
- Physical Surface2D storage is unchanged: write-off mutates an obligation
  ledger, not water storage (M270 decision).

## Snapshot / hash / run evidence

Account age and write-off counters are part of CouplingSnapshot and M269
checkpoint hash. SimDriver carries RuntimeCounters across its per-epoch
CouplingState rebuild, applies write-off before snapshot, and records epoch +
run totals and endpoint IDs in JSON summary (`WARN`/operator evidence shape).

## Gate

G25 `deficit_writeoff_replay` is Phase 1+, implemented, `ci_gate:true`:
aggregate plus independent shared endpoints age 1/2/3, write off 14 m3 total,
leave physical storage unchanged, then rollback to the pre-writeoff snapshot
and replay to an identical report and checkpoint hash.

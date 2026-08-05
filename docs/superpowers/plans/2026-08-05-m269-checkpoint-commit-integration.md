# M269 Plan: Checkpoint Coordinator Wired Into Real Commit/Rollback/Replay

Date: 2026-08-05
Base: `origin/master@1c1514b`

## Goal

Wire the M267 `coordinate_checkpoint_commit` verdict into the M268 SimDriver
run loop so that "committed coupling step" becomes a real, evidence-backed
atomic boundary instead of an unconditional counter increment, and give the
loop real recovery semantics around it.

## Design

- `libs/coupling/driver/checkpoint_payloads`: FNV-1a 64-bit deterministic
  hashing over exact double bit patterns (`std::bit_cast`), container sizes,
  and enum tags — no new dependency; one-ULP sensitive. Builders produce
  `PreparedModuleCheckpoint` records for surface2d ("surface2d-state-v1"),
  coupling ("coupling-snapshot-v1", covering cells, deficits, counters, and
  the pending event queue), sim_driver progress, SWMM ("swmm-noreload-v1",
  metadata-only: SWMM exposes no hotstart/state-reload API), and D-Flow FM
  ("dflowfm-elapsed-v1", or "dflowfm-restart-file-v1" when a validated
  `DFlowFMCheckpoint` restart file is supplied).
- Epoch commit protocol (replaces M268 step 5): after the write-back, all
  module records are prepared and `coordinate_checkpoint_commit` decides.
  Only a committed verdict advances `record_committed_coupling_step` and the
  rolling in-memory window (`LastCommit`: SurfaceState copy +
  CouplingSnapshot + cumulative ledgers) that keeps exactly the last
  committed boundary.
- Failure classification (the key invariant):
  - BEFORE any engine advanced this epoch (CFL rollback in the surface
    substeps, exchange projection failure): restore the last committed
    boundary exactly and stop in `review_required`; the recorded rollback
    decision is `memory_only`.
  - AT/AFTER engine advancement (any failure inside the coupled substep,
    write-back failure, coordinator abort): SWMM cannot rewind, so rollback
    across the engine boundary is REFUSED; the
    `decide_dflowfm_rollback_action` evidence is recorded and the run stops
    in `review_required`.
- Run summary gains per-epoch `checkpoint_status` + content hashes and
  run-level `recovery_action` / `dflowfm_rollback_decision` /
  `final_surface_state_hash` so restores are reproducible in evidence.

## Restore correctness proof shape

The integration test trips a mid-run CFL rollback (rising-water scenario),
then re-runs a fresh simulation that ENDS at the last committed boundary and
asserts the restored state hash and every committed epoch hash match
bit-exactly.

## Non-claims

- No on-disk checkpoints; the rolling window is in-memory and holds one epoch.
- No D-Flow FM restart-file production (the existing consume path is carried
  as a record schema only).
- No SWMM state restore (no hotstart exposure; refusal IS the contract).
- No automatic retry/replay-forward after a restore: the run always lands in
  `review_required` for the operator.

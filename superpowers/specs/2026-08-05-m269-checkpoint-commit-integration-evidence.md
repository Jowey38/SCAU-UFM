# M269 Checkpoint Commit / Rollback / Replay Integration Evidence

Date: 2026-08-05
Base: `origin/master@1c1514b`

## Delivered

- `libs/coupling/driver/checkpoint_payloads` adds deterministic FNV-1a
  64-bit payload hashes over exact double bit patterns, container sizes, and
  enum tags. Surface2D state and CouplingSnapshot hashes are deterministic and
  one-ULP sensitive; the coupling hash covers cells, aggregate and
  endpoint-owned deficits, runtime counters, and pending events.
- Prepared checkpoint builders now exist for all modules required by the M267
  coordinator:
  - Surface2D: `surface2d-state-v1`, in-memory payload;
  - CouplingLib: `coupling-snapshot-v1`, in-memory payload;
  - SimDriver: `sim-driver-progress-v1`, in-memory progress record;
  - SWMM: `swmm-noreload-v1`, elapsed-time metadata only (SWMM exposes no
    hotstart/state-reload API);
  - D-Flow FM: `dflowfm-elapsed-v1`, or
    `dflowfm-restart-file-v1` when a validated existing restart checkpoint is
    supplied.
- The SimDriver run loop now calls `coordinate_checkpoint_commit` after the
  coupled substep and conservative write-back. Only a committed verdict
  advances `record_committed_coupling_step()` and the rolling in-memory
  `LastCommit` boundary.
- Run summary evidence now includes per-epoch checkpoint status and surface /
  coupling content hashes, plus run-level recovery action, D-Flow rollback
  decision, and final SurfaceState hash.

## Recovery boundary

The failure classification is explicit and fail-closed:

1. **Before engine advancement** (Surface2D CFL rollback or exchange
   projection failure): restore the last committed SurfaceState, coupling
   ledger snapshot, and cumulative ledgers; record
   `recovery_action=restored_to_last_commit` and the existing D-Flow decision
   `memory_only`; stop in `review_required`.
2. **At or after engine advancement** (coupled-substep exception, write-back
   failure, or checkpoint coordinator abort): refuse rollback because SWMM
   cannot rewind; record `recovery_action=refused_engine_rollback` and the
   existing D-Flow abort decision; freeze the committed counter and stop in
   `review_required`.

The integration test proves the restore path by triggering a mid-run CFL
rollback, then running a fresh deterministic simulation ending at the last
committed boundary. The restored final SurfaceState hash and every committed
surface/coupling checkpoint hash match bit-exactly. A delegating SWMM fixture
throws during the post-step overflow read to prove the refusal path: both
engines have advanced, zero epochs commit, rollback is refused, and the
operator-facing reason preserves the injected failure.

## Verification

MSVC Debug, clean `origin/master@1c1514b` plus this change only:

- Build: no C/C++/link/MSBuild errors.
- Full CTest: **149/149 passed** (147 baseline + checkpoint payload unit test
  + checkpoint rollback/refusal integration test).
- `ctest -L golden`: **23/23 passed**.
- `ctest -L manifest`: **1/1 passed**.
- Governed real-runtime gateway:
  `tools/dflowfm/run_real_goldens.sh H:/scau-b-m269 Debug` -> **6/6 passed**,
  including G19 with a committed checkpoint record for each real tri-model
  epoch.

## Non-claims

- The rolling checkpoint window is in-memory and retains one committed epoch;
  no on-disk Surface2D/CouplingLib checkpoint is produced.
- D-Flow FM restart-file production is not implemented; the existing
  validated reload input can only be represented when supplied externally.
- SWMM state restore is not implemented because no governed hotstart/reload
  API exists. Refusing rollback after engine advancement is the contract, not
  a degraded restore claim.
- No automatic retry or replay-forward occurs after restoration; all recovery
  paths stop in `review_required` for operator action.

# M240-ROOF GoldenSuite Entry: Roof->SWMM Transfer Conservation Evidence

## Scope

Promotes the delivered roof -> SWMM chain (PRs #33, #35, #36) to a GoldenSuite gate on the real embedded SWMM 5.2.4 engine. Base: `master` `ba87dba`.

Manifest entry `M240-ROOF` (`tests/golden/suite_manifest/goldensuite.json`): name `roof_swmm_transfer`, `ci_gate: true`, `status: implemented`, Phase 1+; enforced by `check_manifest.py` (registration + labels `golden;roof;swmm;m240`), which runs as `test_goldensuite_manifest`.

## Golden contract (`test_golden_roof_swmm_transfer`)

10-substep deterministic schedule (dt_sub = 60 s) through `RoofSwmmStepDriver` -> `RoofExchangeGate` (live `CouplingState` endpoint, V_limit = 45 m3, Q_limit = 0.75 m3/s) -> `SwmmRoofDrainageAcceptanceAdapter` -> real routing; includes under-limit, over-limit, zero-request, and one rolled-back substep:

1. per intent: `accepted + rejected == requested` (1e-12)
2. per substep: `accepted <= V_limit` exactly; over-limit intents clamp to exactly `V_limit` with `CapacityLimited`
3. after each advanced substep: routed lateral inflow == `accepted / dt_sub` (1e-6)
4. rolled-back substep routes 0 into the real solver
5. totals close: `sum(requested) == sum(accepted) + sum(rejected)` (1e-9); delivered volume excludes exactly the rolled-back grant
6. network sanity: outfall receives inflow, head above invert, no unexpected surcharge

## Bugs the golden caught (fixed in this change)

- **bug-019** — zero-volume intent returned `CapacityLimited`. Gate now forwards `requested_volume <= 0` downstream unchanged (trivially-full accept). Regression: `RoofExchangeGate.ZeroVolumeIntentPassesThroughAsFullAccept`.
- **bug-020** — stale inflow leak: `begin_step()` only cleared the ledger while the SWMM API lateral-inflow buffer persists until overwritten, so a node not re-written in a substep kept routing the previous substep's flow. `begin_step()` now zeroes the engine-side inflow of every node written last substep. Regression: `SwmmRoofDrainageAcceptanceAdapter.BeginStepZeroesStaleNodesNotRewritten`.

This is exactly the class of cross-substep semantics error the GoldenSuite exists to catch; neither bug was visible to the single-substep unit tests.

## Verification

Worktree `H:/githubcode/SCAU-UFM/.worktrees/m240-roof-golden`, preset `windows-msvc`, `SCAU_EMBED_SWMM=ON`:

- `test_golden_roof_swmm_transfer`: passed
- `test_goldensuite_manifest`: passed (M247-EF + M240-ROOF)
- Full suite: **66/66 passed**, no link errors

## CI note

Per the stability protocol, this gate becomes merge/release evidence once CI runs the golden labels on this base; the manifest marks `ci_gate: true` and the manifest checker enforces registration.

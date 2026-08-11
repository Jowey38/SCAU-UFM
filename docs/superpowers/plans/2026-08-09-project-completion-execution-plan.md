# Project Completion Gated Execution Plan

## Objective

Execute the remaining project work in dependency order until every claim is either promoted with fresh evidence or formally blocked with failure evidence. Never bypass a missing external scope contract and never infer external flux from storage delta.

## Current baseline

- M272 SWMM external-net scope: complete and independently sampled.
- M273 D-Flow FM external-boundary scope: FAILED/BLOCKED.
- G19: implemented, non-gating, scope-incomplete, REVIEW_REQUIRED; real promotion blocked.
- G27: required future external-flux Golden, not registered.
- G24/G25 and prior completed gates remain valid and unaffected.
- Phase 2 capability work is planning-only until G19 scope closure.

## Execution phases

### Phase A: Close the D-Flow external contract

1. Extend the standalone D-Flow spike only; do not modify production adapters.
2. Obtain or derive a governed boundary map from engine/runtime evidence, including boundary IDs, native locations, link adjacency, orientation, units and boundary classes.
3. Exercise inflow-only, outflow-only, mixed boundary, lateral plus boundary, variable timestep and restart/replay cases.
4. Determine whether an engine cumulative boundary-volume API exists. If not, prove an external mapped/integrated observation without circular use of `sum(vol1)`.
5. Produce G27 failure-revealing evidence. If the contract remains unavailable, stop and keep G19 blocked; do not create a provider.

### Phase B: Implement and verify G27 only after Phase A passes

1. Add the smallest driver-owned external-flux observation/provider preserving `IDFlowFMEngine` lifecycle/state-only boundaries.
2. Add a non-gating G27 candidate test covering all proven scope.
3. Verify independent closure against `sum(vol1)` across source, sink, mixed and restart cases.
4. Run hosted and self-hosted CI. Promote G27 only with complete reproducible evidence.

### Phase C: Re-run real G19

1. Bind both M272 SWMM and verified D-Flow external scopes.
2. Run the fresh real three-model whole-system audit at committed epoch boundaries.
3. Confirm `scope_complete=true` before interpreting residual.
4. Compare residual against existing legal tolerance without widening it.
5. Decide G19 promotion; if failed, record residual and keep non-gating. If passed, update manifest/stability protocol atomically and run all CI lanes.

### Phase D: Phase 2 capabilities

After G19 is formally unblocked, explore and implement in this order:

1. G9 CUDA deterministic parity: bounded fixture, device snapshot/replay, fixed reduction, full matrix, GPU CI evidence.
2. G20 long-run policy: 10,000-step case, per-step dt audit, restart/checkpoint, resource/time policy.
3. Ponded-h infiltration: pure kernel and failure-revealing storage/wet-dry closure before production seam.
4. High-order CVC/wet-dry: numerical sandbox, positivity, near-dry, arbitrary wet/dry, replay; keep default-off until proven.
5. Real city-data import: obtain authorized upstream sample and format contract, then importer spike/topology/field validation; do not invent schemas.

For each capability use: design spike -> failure-revealing candidate -> pure/sandbox implementation -> dedicated GoldenTest -> production seam -> manifest/protocol update with fresh CI.

## Stop conditions

- Missing or contradictory scope evidence: record FAILED/BLOCKED and stop that workstream.
- Any test/build/CI failure: record bug, fix only the relevant scope, rerun required evidence.
- No promotion based solely on local tests, storage deltas, mocks or pending manifest entries.
- Project completion means all remaining workstreams are either promoted with complete evidence and green CI or explicitly blocked with an accepted decision record.

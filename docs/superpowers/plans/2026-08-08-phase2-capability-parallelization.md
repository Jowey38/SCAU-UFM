# Phase 2 Capability Parallelization After G19 Scope Closure

## Gate before implementation

Phase 2 implementation starts only after G19 external scope is converged:

1. M272 SWMM external-net scope remains complete and independently sampled.
2. A governed D-Flow FM external-flux contract exists, including boundary identity, orientation, units, variable-step integration, all external source/sink classes, and restart/replay semantics.
3. A fresh real three-model whole-system audit is scope-complete.
4. The residual is compared with the pre-existing legal tolerance without tolerance inflation.
5. G19 promotion decision is recorded before any manifest or stability-protocol promotion.

Until then, this document is planning-only. No production implementation or gate promotion is authorized by this phase map.

## Parallel workstreams

| Workstream | First evidence slice | Current dependency / no-go |
|---|---|---|
| G9 CUDA | CPU/CUDA deterministic contract on a bounded fixture, backend parity, failure diagnostics | Do not promote G9 or alter Phase 2 manifest until CUDA backend has reproducible device evidence and existing gate matrix is updated atomically. |
| G20 long-run policy | 10,000-step policy spike with per-step dt audit, checkpoint/restart observation, and resource envelope | Do not call a long run a reliability gate without authored case, runtime policy, timeout/resource limits, and reproducible evidence. |
| Ponded-h infiltration | Failure-revealing ponded storage/infiltration case with `phi_t*h*A` closure and nonnegative water | Do not wire into the production step seam before closure, wet/dry transitions, and rollback evidence exist. |
| High-order CVC/wet-dry | Pure numerical sandbox for side-specific physical-storage flux, positivity, near-dry and replay | Keep default-off and non-gating until arbitrary wet/dry positivity and conservation evidence exists; do not overclaim M263 first-order CVC. |
| Real city-data import | Authorized upstream GIS/UGRID sample and format contract, importer spike, topology/field validation | Do not invent a JSON/YAML schema or implement importer against unauthenticated/unspecified data. |

## Ordering

The workstreams may be explored in parallel after G19 is formally unblocked, but each must remain isolated from production promotion. Recommended sequence within each stream is:

1. evidence/design spike;
2. failure-revealing candidate;
3. pure function or sandbox implementation;
4. dedicated GoldenTest candidate;
5. production seam only after review;
6. manifest/stability-protocol update only with fresh CI evidence.

## Current decision

G19 is still `BLOCKED` because M273 did not prove D-Flow external-net scope. Therefore this milestone records the parallel plan only and intentionally changes no production code, GoldenSuite entry, or stability-protocol rule.

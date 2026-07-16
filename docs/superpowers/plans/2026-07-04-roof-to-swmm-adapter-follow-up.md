# Roof-to-SWMM Adapter Follow-up Plan

## Status

M247-D introduced the `RoofDrainageAcceptanceFn` port in `surface2d` and deliberately kept `surface2d` independent of SWMM/CouplingLib adapter code. M247-F is now merged into `origin/master`.

The next desired integration is the real roof -> SWMM lateral inflow adapter. This cannot be implemented correctly on the current clean `origin/master` base because `libs/coupling/drainage/` is not present on that base yet. The only SWMM material currently on `origin/master` is spike/spec documentation, not a compiled drainage adapter interface.

## Blocking facts

Current clean base: `origin/master` at `694a430`.

Present:

- `libs/coupling/core/`
- `spikes/swmm/`
- architecture/spec references to `libs/coupling/drainage/`
- `surface2d` roof acceptance port: `RoofDrainageAcceptanceFn`

Missing from current clean base:

- `libs/coupling/drainage/`
- `ISwmmEngine` / `SwmmEngine` compiled interface
- `SWMMAdapter` compiled target
- a project-owned DTO/API for applying node lateral inflow from roof drainage acceptance

## Reason for deferral

A real adapter must not make `Surface2DCore` depend on SWMM or any 1D engine ABI. It must live on the drainage/coupling side and plug into `RoofDrainageAcceptanceFn` from outside `surface2d`.

Implementing that adapter before the drainage adapter base exists would either:

- invent a duplicate SWMM interface, or
- violate the established boundary by pulling SWMM knowledge into `surface2d`.

Both options are worse than deferring until the drainage base is present.

## Recommended next implementation once M240 drainage base lands

1. Add a drainage-side adapter function or small callable object that converts a `surface2d::RoofDrainageIntent` into a SWMM node lateral inflow request.
2. Keep the callable type compatible with `surface2d::RoofDrainageAcceptanceFn`.
3. Convert accepted volume to flow using `q = accepted_volume / dt_sub` at the adapter boundary.
4. Write to the SWMM engine through the project-owned drainage interface, not directly through a SWMM C API from `surface2d`.
5. Return `RoofDrainageAcceptance` with accepted/rejected volumes and fail-closed rejection reason when the node or engine is unavailable.
6. Add mock-engine tests that verify:
   - accepted volume is converted to lateral inflow exactly once
   - rejection preserves roof pending/overflow semantics
   - invalid node mappings fail closed
   - no SWMM/D-Flow FM native objects see each other

## Acceptance criteria

- No `surface2d` dependency on `libs/coupling/drainage` or SWMM ABI.
- Adapter lives outside `surface2d`.
- `RoofDrainageAcceptanceFn` remains the only surface2d-facing seam.
- Unit tests cover accept, reject, missing node, and zero/negative capacity cases.
- Full suite and relevant runoff/roof tests pass.

## Current completed work before this follow-up

- M247-F merged into `origin/master` via PR #9.
- M247 runoff golden promoted to explicit GoldenSuite CTest gate on branch `feat/m247-runoff-golden-gate`.
- M247 post-merge checks pass:
  - `test_runoff_ground`
  - `test_runoff_step`
  - `test_runoff_golden_urban_block`
- GoldenSuite label gate passes:
  - `test_goldensuite_manifest`
  - `test_golden_runoff_urban_block`
- Full suite after gate promotion: 55/55 passed.

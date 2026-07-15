# M240 Real Roof-to-SWMM Adapter Evidence

## Scope

This evidence records the replacement of the mock-only drainage baseline with a real embedded SWMM 5.2.4 engine behind `ISwmmEngine`, so the roof drainage acceptance seam now has a real roof -> SWMM path.

Base: `master` `26d6cce` (M240 drainage adapter baseline, PR #32).

Added:

- vendored SWMM 5.2.4 solver at `extern/swmm5/` (unmodified upstream), built by `cmake/third_party/swmm.cmake` as static target `scau::extern_swmm5`, gated by `SCAU_EMBED_SWMM` (default ON)
- third-party governance files at `third_party/manifest/swmm5.version` and `third_party/licenses/swmm5-LICENSE-NOTE.md`
- real `SwmmEngine final : ISwmmEngine` in `libs/coupling/drainage/src/swmm_adapter/swmm_engine.cpp`; `swmm5.h` stays private to that translation unit
- header split: `coupling/drainage/swmm_boundary.hpp` now owns `SwmmEngineError` + `ISwmmEngine`; `coupling/drainage/swmm_engine.hpp` declares the real engine (matches the layout used by the G8/G11 lineage for future reconciliation)

## Interface conformance

`SwmmEngine` implements the master `ISwmmEngine` (`std::size_t` ids, `core::Real` values):

- `initialize` opens + starts the project; fail-closed on empty/missing `.inp`; process-wide atomic single-project guard (SWMM keeps global solver state)
- `step(dt)` advances via repeated `swmm_step` until the target time; detects end-of-simulation (elapsed reset to 0)
- `set_node_lateral_inflow` writes the API inflow buffer (`swmm_NODE_LATFLOW`); read-back is routed value after a step
- `is_surcharged` = node depth within 1e-9 of max depth
- extras (not on the interface): `get_node_inflow`, `get_node_overflow`, `set_outfall_stage` (outfall-type checked), `node_index`/`link_index`/`node_count`, `elapsed_time`

`SwmmRoofDrainageAcceptanceAdapter` is unchanged: it now runs against the real engine purely through `ISwmmEngine`.

## Tests

### test_coupling_swmm_engine (real engine unit)

- lifecycle initialize/step/finalize; node/link name lookup
- lateral inflow routes toward outfall (routed LATFLOW read-back ~0.05, head above invert, outfall inflow > 0)
- outfall stage setter rejects non-outfall nodes and non-finite stage
- manhole overflow case shows real overflow and surcharge within 100 x 10 s steps
- fail-closed on uninitialized use, out-of-range ids, dt<=0, missing names, non-finite inflow
- second concurrent engine fails closed; solver reusable after finalize

### test_coupling_roof_swmm_real (roof -> real SWMM integration)

- accepted roof intent (30 m3 over 600 s) becomes q = 0.05 m3/s at the target node inside the real solver and reaches the outfall
- two intents on the same node accumulate (18 + 12 m3 -> 0.05 m3/s), confirming per-node accumulation semantics against the real engine
- genuinely surcharged manhole (driven by the overloaded case) rejects a roof intent fail-closed with `NodeSurcharged` (`accepted=0`, `rejected=requested`)
- out-of-range node id on the real engine rejects fail-closed with `InvalidTargetNode`

## Verification

Executed in clean worktree `H:/githubcode/SCAU-UFM/.worktrees/m240-real-swmm` (base `26d6cce`), preset `windows-msvc`, `SCAU_EMBED_SWMM=ON`.

- Focused: `test_coupling_mock_swmm_engine`, `test_coupling_roof_drainage_adapter`, `test_coupling_swmm_engine`, `test_coupling_roof_swmm_real` — 4/4 passed
- Full suite: 59/59 passed, 0 failed
- Link check: no LNK1168 / compile errors in the build log

## Boundary result

- `surface2d` remains free of SWMM/drainage includes; the roof seam is still `surface2d::RoofDrainageAcceptanceFn`
- `swmm5.h` is included only by `libs/coupling/drainage/src/swmm_adapter/swmm_engine.cpp`
- third-party code compiles without project warnings-as-errors (`/W0`), per third-party governance

## Assessment

The roof -> SWMM path is now real end-to-end at the drainage boundary: roof intents emitted by `Surface2DCore` can be accepted into an embedded SWMM 5.2.4 solver and are routed by its hydraulics, with fail-closed rejection preserved for invalid nodes and genuinely surcharged manholes. Remaining follow-ups: CouplingLib-mediated `Q_limit`/deficit arbitration in front of this adapter, and step-driver wiring for dt_sub scheduling.

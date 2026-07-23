# G15 Real-SWMM Shared-Cell Arbitration Golden Evidence

## Scope

Base: `master` `952f217` (G12 dual_engine_shared_cell mock first landing, PR #39). Adds `G15 dual_engine_shared_cell_real_swmm`: the real-SWMM companion to G12.

SWMM (drainage) and a mock D-Flow FM (river) share ONE 2D exchange cell through `advance_tri_coupling_step`. The drainage arbitration decision drives a **real** embedded SWMM 5.2.4 node write and is routed by the real solver; the river side stays mocked.

Manifest entry `G15` (`tests/golden/suite_manifest/goldensuite.json`): `candidate_non_gating`, `ci_gate: false`, Phase 2+, enforced by `check_manifest.py` (name/path/status + `add_test(NAME test_dual_engine_shared_cell_real_swmm` token).

## Why river stays mocked

A real dual-engine gate needs a real D-Flow FM runtime, which requires an external BMI kernel DLL — that is G11, still `pending`. Per the GoldenSuite plan, `ci_gate` stays `false` for G12/G15 until G11 lands. G15 is the intermediate step that makes the **SWMM half** of the shared-cell contract real, satisfying the CLAUDE.md reliability requirement for real shared-cell arbitration evidence on the drainage engine.

## What G15 exercises beyond G12 (all-mock)

Shared cell: `V_limit = 0.9 * 0.4 * 2.0 * 50 = 36 m3`, `Q_limit = 9 m3/s`; priority weights 1:2 -> `V_limit_k` = 12 (drainage) / 24 (river).

### Test 1 — RealSwmmRoutesArbitratedSharedCellGrant

- weighted request `1*4 + 2*4 = 12 m3/s > Q_limit 9`: spec 6.1 scale-then-cut. Drainage scaled grant `4*0.75 = 3 m3/s` meets its k-limit exactly (12 m3), 4 m3 unmet; river grant caps at its own 4 m3/s (16 m3).
- **the arbitrated 3 m3/s was written into and routed by the real SWMM solver**: `get_node_lateral_inflow(J1) == 3`, `head(J1) > 10 m` (above invert), `inflow(O1) > 0` (reaches outfall).
- ledger after replay: cell volume 12 m3; 4 m3 deficit on the drainage endpoint account only.

### Test 2 — DeficitRepaysThroughRealSwmmAcrossSubsteps

- substep 2 (intents drop to 0, link persists): cell drained to volume 12 -> h 0.6 -> `V_limit 10.8`, drainage 1/3 share -> `Q_limit_k 0.9`, so `q_repay = min(4/4, 0.9) = 0.9` -> `v_repay 3.6`, 0.4 residual. **Real solver routes exactly 0.9 m3/s.**
- substep 3: residual 0.4 m3 clears in full (`v_repay 0.4`, routed 0.1 m3/s), ledger settles to 0.

Confirms the core deficit repayment schedule survives real SWMM stepping bit-for-bit against the mock G12 expectations.

## Verification

Worktree `H:/githubcode/SCAU-UFM/.worktrees/g12b-real-swmm`, build at short path `H:/scau-g12b` (avoids Windows MAX_PATH / MSB3491 in deep worktree paths), `SCAU_EMBED_SWMM=ON`:

- `test_dual_engine_shared_cell_real_swmm`: passed
- `test_goldensuite_manifest`: passed (G1-G15)
- Full suite: **114/114 passed**, no link errors

## Follow-ups

- G11 real D-Flow FM runtime (external kernel DLL) -> then promote G12/G15 to `ci_gate: true` real dual-engine gate
- Optionally fold the real D-Flow FM side into a G15 successor once G11 evidence exists

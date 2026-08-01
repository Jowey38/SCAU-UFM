# M264 Phase / GoldenSuite Gate Alignment Evidence

Date: 2026-07-30
Base: `origin/master@7f24a77`

## Decision

The release-phase gate is aligned with the capability roadmap:

- Phase 1 minimum GoldenSuite: G1-G8 + G10.
- Phase 2 minimum GoldenSuite: Phase 1 set + G9 + G11 + G12.
- G13 and later registered tests remain applicable enhancement gates; they do
  not replace the normative minimum for a release phase.

This resolves the prior contradiction where main Spec section 2.2 assigned the
CUDA backend to Phase 2 while section 10 assigned GPU-only G9 to Phase 1.
CUDA and GPU infrastructure are now consistently Phase 2 requirements.

## Gate promotion

- G10 `snapshot_replay_mass_deficit` is promoted to `ci_gate:true` and label
  `golden`. Its deterministic in-memory Golden covers aggregate/shared endpoint
  deficit, pending-event fidelity, runtime counters, system mass audit, and
  repeated rollback/replay zero drift.
- G12 `dual_engine_shared_cell` is promoted to `ci_gate:true` and label
  `golden`. It is the hosted deterministic Phase 2 shared-cell path. G16 remains
  the self-hosted real-both-engines complement.
- G9 remains `pending, ci_gate:false, Phase 2+` until a real CUDA deterministic
  implementation and blocking GPU runner exist. A skipped or unavailable GPU
  path must not be reported as passing G9.
- G15 remains implemented but non-gating supplementary real-SWMM evidence.

## G19 / G20 reservation

G19 `surface2d_tri_coupling_real` and G20 `dflowfm_longrun_10000` remain reserved
for the post-G18 completion line. Historical worktree files are not promoted by
this decision. They require reconstruction on the current master, manifest
registration, and fresh evidence before they may be called implemented.

## Baseline verification

A clean short-path Windows MSVC Debug baseline was configured at
`H:/scau-b-m264` from `7f24a77`:

- build completed with no C/C++/link/MSBuild errors;
- full CTest: 135/135 passed;
- Golden labels before this change: 20/20 passed;
- manifest completeness: passed;
- `SCAU_DFLOWFM_LIBRARY` was not configured in this shell, so no fresh real
  D-Flow runtime claim is made here.

Network fetch was unavailable at the start of the run. The base is the locally
recorded `origin/master@7f24a77`; remote freshness must be reconfirmed when
network access returns.

## Files synchronized

- `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md`
- `superpowers/specs/2026-04-14-scau-ufm-stability-reliability-protocol.md`
- `tests/golden/suite_manifest/goldensuite.json`
- `tests/golden/suite_manifest/check_manifest.py`
- G10 and G12 per-test CMake labels

## Scope

This is a governance and gate-alignment change. It does not implement G7, G9,
CUDA, a real GIS importer, or a new physical model.

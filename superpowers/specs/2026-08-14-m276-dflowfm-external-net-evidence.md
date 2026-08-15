# M276 D-Flow FM External-Net Provider + G27 Implementation Evidence

Date: 2026-08-14
Base: origin/master fc67467 (M274 Phase A contract evidence merged, PR #72)
Plan: docs/superpowers/plans/2026-08-14-m276-dflowfm-external-net-provider.md

## What landed

1. `DFlowFMEngine::observe_native_water_balance()` (concrete-engine-only):
   reads the M274 bridge ABI `dflowfm_get_water_balance_v1`, fail-closed on
   missing symbol / ABI mismatch / read error; value-level validity is
   reported via `scope_complete`. `IDFlowFMEngine` is untouched (lifecycle +
   state read/write only). ABI layout mirrored privately in the adapter TU
   per the established bmi.h typedef policy; authored contract snapshot at
   `extern/dflowfm/include/scau_dflowfm_water_balance_v1.h`.
2. `derive_dflowfm_external_net` / `observe_dflowfm_external_net`
   (driver-owned): audit external net = boundary_in - boundary_out; ALL
   lateral volume is CouplingLib-mediated API exchange and is removed exactly
   once (M272 dedup precedent); source/qext/rain/evaporation/groundwater are
   unproven forcing classes and fail closed when nonzero. 6 unit tests.
3. `apps/sim_driver/main.cpp` real branch binds
   `hooks.dflowfm_external_net_volume` and `hooks.dflowfm_storage_volume`;
   with M272 SWMM scope this makes the real-path whole-system audit
   scope-complete for the first time.
4. G27 `dflowfm_external_net` golden registered `ci_gate:false` +
   `candidate_non_gating`; executed by the self-hosted gateway
   (`tools/dflowfm/run_real_goldens.sh`, now 7 goldens) and built by the
   real-dflowfm-golden CI job.

## bug-207: full sum(vol1) overcounts on open-boundary models

First G27 execution FAILED and revealed a real storage-scope defect:

- open case at initialize: full `sum(vol1)` = 6500 m3 vs native `vol1tot`
  = 5500 m3 (exactly the two boundary ghost-node volumes);
- 10-step storage delta via full sum = 21.0603 m3 vs boundary-flux delta
  = 17.9196 m3 (ghost dynamics pollute deltas too).

The M258/M259 "sum(vol1) is whole-domain storage" evidence holds only for
closed models (no ghost entries). Fix:

- `DFlowFMEngine::internal_cell_count()` reads BMI scalar `ndxi` fail-closed;
- `observe_dflowfm_internal_volume` sums `vol1[0:ndxi]` (Neumaier);
- `RunLoopHooks.dflowfm_storage_volume` optional override; real mode binds
  native `storage_m3` (vol1tot). Absent the hook, behavior is unchanged
  (mock/closed paths bit-identical).

## Local verification (this machine == self-hosted runner)

- Full suite: 160/160 pass (G27 skips without env), LNK1168 = 0.
- Manifest checker: OK (G27 tuple + JSON + `candidate_non_gating` label).
- Real G27 against the bridged DLL: PASS —
  - Leg 1 (open, mixed boundary): boundary_in/out both nonzero monotonic;
    |delta sum(vol1[0:ndxi]) - delta external_net| <= 1e-6 m3; native
    storage == internal vol1 sum; full sum > internal + 100 m3 (discovery
    locked).
  - Leg 2 (closed + API lateral 0.125 x 600 s): lateral class integrates to
    75 m3; audit external net stays 0 (dedup); storage grows by exactly the
    injected volume; internal == full sum on the closed model.
  - Leg 3: re-initialize resets every native cumulative counter to zero
    (per-initialize re-baseline contract).

## Boundaries and non-claims

- G27 stays non-gating until the real gateway runs green on master
  (stability protocol: represented-in-CI before gate promotion).
- G19 promotion is NOT claimed here; it requires a fresh scope-complete real
  audit run and a separate decision record (Phase C).
- qext/rain/groundwater forcing remains engine-blocked and guarded
  observed-zero; no provider support is claimed.

# M276: D-Flow FM external-net provider + G27 (Phase B of the completion plan)

Date: 2026-08-14
Base: origin/master 49cde6d + M274 Phase A contract evidence (PR #72)

## Objective

Productionize the M274 native water-balance contract as the driver-owned
D-Flow FM external-flux observation required by the G19 whole-system audit,
and register G27 `dflowfm_external_net` as a non-gating real-engine golden.

## Scope decisions

1. `DFlowFMEngine::observe_native_water_balance()` is concrete-engine-only
   (mirrors `SwmmEngine::total_stored_volume` / `observe_external_net_volume`):
   `IDFlowFMEngine` keeps lifecycle/state-only semantics; the bridge ABI never
   reaches the interface, mocks, or public DTO surface.
2. The ABI layout is mirrored privately in `dflowfm_engine.cpp` following the
   established bmi.h typedef policy; the authored contract snapshot lives at
   `extern/dflowfm/include/scau_dflowfm_water_balance_v1.h`.
3. Audit semantics are driver-owned (`derive_dflowfm_external_net`, pure +
   unit-tested):
   - external net = boundary_in - boundary_out (genuinely external);
   - ALL lateral volume is CouplingLib-mediated API exchange, removed from the
     audit value exactly once (M272 SWMM api-lateral dedup precedent);
   - source/qext/rain/evaporation/groundwater are unproven forcing classes
     (M273/M274): any nonzero value fails closed
     (`dflowfm_external_unproven_class_nonzero`).
4. Cumulative counters reset on every engine initialize (M274 restart A/B):
   the run-loop audit already consumes current-minus-baseline within one
   initialize span; reload flows re-baseline by construction. Documented on
   the DTO and enforced by G27 leg 3.
5. `apps/sim_driver/main.cpp` real branch binds
   `hooks.dflowfm_external_net_volume`; with M272 this makes the real-path
   whole-system audit scope-complete for the first time.
6. G27 registers as `ci_gate:false` + `candidate_non_gating` (2026-08-08
   manifest labeling rule) and is executed by the self-hosted real gateway
   (`tools/dflowfm/run_real_goldens.sh`, now 7 goldens). Promotion to
   `ci_gate:true` follows the stability protocol only after the gateway runs
   green on master.

## G27 test matrix

- Leg 1 (open case, mixed boundary): both boundary directions nonzero and
  monotonic; independent closure |delta sum(vol1) - delta external_net| <=
  1e-6 m3; native storage == sum(vol1).
- Leg 2 (closed case + API lateral 0.125 m3/s x 600 s): lateral class
  integrates to 75 m3; audit external net stays 0 (dedup); storage grows by
  exactly the injected volume.
- Leg 3 (re-initialize): every native cumulative counter reads zero again.

## Explicitly out of scope

- G19 promotion (Phase C; separate fresh-evidence decision).
- Restart-file continuity re-proof (spike-level evidence in M274; G27 proves
  the per-initialize reset that the audit actually depends on).
- Any qext/rain/groundwater forcing support.

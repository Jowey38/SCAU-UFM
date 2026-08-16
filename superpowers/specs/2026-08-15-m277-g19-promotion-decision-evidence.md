# M277 G19 Promotion Decision Evidence

Date: 2026-08-15
Decision: **PROMOTED — G19 `surface2d_tri_coupling_real` becomes an active
gate (`ci_gate:true`) with a scope-complete, engine-gap-decomposed real audit**

Supersedes the 2026-08-08 BLOCKED decision
(`superpowers/specs/2026-08-08-g19-promotion-decision-evidence.md`) after its
required next evidence landed: M274 native water-balance contract (PR #72),
M276 external-net provider + G27 (PR #73), and this fresh real-audit run.

## Fresh scope-complete evidence

The reworked G19 binds both engine external scopes (M272 SWMM + M276 D-Flow)
plus the driver ledger, enables the run-loop whole-system audit at every
committed epoch, and completes 3/3 epochs with verdict `conserved`:

- coupling residual: fp-exact 0 (assertion cap 1e-6 m3; strict M270
  tolerance 5.6e-3 m3 NOT widened)
- SWMM internal continuity gap: -0.0269 m3 (documented bound 0.05 m3)
- D-Flow internal gap: ~0 (native volume-error scale)
- total residual: -0.0269 m3 vs the historical 141.8 m3 missing-scope signal

## Three defects found by the fresh audit (the gate worked)

1. **bug-208 — stage-driven outfall backwater imports untracked mass.**
   Writing the river stage onto O1 makes SWMM fill from the outfall boundary:
   +37.79 m3 in one epoch with no routing-totals class and no CouplingLib
   debit from D-Flow. This path bypasses the CouplingLib ledger invariant
   entirely. Excluded from the G19 fixture; governed reverse exchange is
   M278 scope. The audit stays armed against any config that re-enables it.
2. **bug-209 — SWMM dry-start wetting front creates volume.** Standalone
   probe (no coupling): constant API lateral into dry pipes physically
   delivers +0.067 m3 more than q*dt once, while massbal records only q*dt;
   constant beyond 60 s -> startup artifact. Wet-start fixture (conduit
   InitFlow) removes it; residual recession error is the remaining
   documented engine gap (-0.03 m3 scale).
3. **bug-210 — rate-sampled 1D-1D interface injection is not volume
   conservative.** Injected-to-river 0.6207 m3 vs SWMM-emitted 0.5243 m3
   over two epochs; the seam creates the difference. Interface leg excluded
   from the G19 conservation fixture (G17 keeps behavioral coverage);
   volume-conservative ledger-backed interface exchange is M278 scope.

## M277 audit decomposition (design change, fail-closed preserved)

`WholeSystemMassSample` gains CouplingLib-ledger lateral terms and the audit
decomposes each real engine's OWN internal continuity gap:

    engine_gap = delta storage - ledger_lateral - external_net_delta
    coupling_residual = residual - sum(engine gaps)

Verdict `conserved` requires scope_complete AND |coupling_residual| <=
applied_tolerance (strict M270 path, unchanged) AND every decomposable gap
within `engine_internal_gap_absolute` (new documented bound; 0 by default,
0.05 m3 for the G19 fixture from measured evidence). Without ledger terms
the audit behaves exactly as before (G24 unchanged). Engine error can no
longer hide inside — nor mask — coupling drift: a seam leak shows up in
coupling_residual regardless of any gap allowance (unit-tested).

Also landed: `cumulative_engine_internal_return_volume` (driver-returned
in-system volume: interface v_granted + drainage overflow returns) and an
optional in-run `dflowfm_storage_volume` hook binding native vol1tot.

## Gate consequence

- G19 `ci_gate:true`, `LABELS golden`; manifest + checker updated together.
- Stability protocol text unchanged (G19 is a manifest gate like G13-G25;
  the Phase-minimal release sets are untouched).
- Real gateway now enforces G19 in its scope-complete form on every CI run.
- Remaining REVIEW_REQUIRED scope moved from "missing external flux" to the
  explicit M278 capabilities (backwater governance + conservative 1D-1D
  interface), each guarded fail-closed by the armed audit.

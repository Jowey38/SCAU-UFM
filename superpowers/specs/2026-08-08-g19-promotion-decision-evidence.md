# G19 Promotion Decision Evidence

Date: 2026-08-08
Decision: **BLOCKED — no promotion and no real-audit rerun**

## Basis

M272 provides a complete SWMM external-net observation at the concrete adapter boundary. Its raw routing components and cumulative API lateral are converted to SI units, and the audit value removes the CouplingLib-owned API lateral exactly once.

M273 is a real D-Flow FM open-boundary spike, not a production provider. The authored case uses an upstream `dischargebnd=0.125 m3/s` and downstream `waterlevelbnd=1.0 m` over 10 x 60 s, with a zero-discharge control. Both cases advance with exact 60 s time traces. Follow-up probes identify `kcu=-1` boundary links and their `ln` node pairs in this case, but caller `dt=30` is ignored in favor of the engine's internal 60 s, so requested-dt integration is not a valid production assumption.

The candidate D-Flow external variables are insufficient:

- `qext`, `qextreal`, and `vextcum` are unavailable after initialization and after every update in both cases.
- `q1` is readable, but only as native flow-link values. The BMI surface does not provide a governed mapping from q1 indices to authored boundary IDs, an external-inflow-positive orientation convention, a cumulative integration contract, or checkpoint/replay semantics.
- `delta sum(vol1)` is a storage response, not an independently observed external-net term. In the forced case, 75 m3 is prescribed over 600 s while final storage gain is 21.06034046053 m3; the difference crosses the downstream boundary. Reusing storage delta as external flux would be circular.

## Decisions

1. **Residual tolerance:** not decidable. The real audit input is not scope-complete, so no residual can be legally compared with the existing numerical tolerance.
2. **G19 promotion:** denied. Keep G19 `implemented, ci_gate:false`, with `scope_complete=false` and `REVIEW_REQUIRED` behavior.
3. **G27:** required as a future external-flux Golden prerequisite, but not registered now. Registering a pending/mock G27 before the runtime contract exists would overstate evidence.
4. **Stability protocol:** no change now. Existing fail-closed rules already require complete external scope and prohibit tolerance inflation. Update only together with a proven G27 contract and fresh G19 evidence.
5. **Manifest:** no change now. Existing G19 non-gating status is correct; no new GoldenSuite entry is warranted.

## Gate consequence

Real whole-system conservation remains `REVIEW_REQUIRED`; any release or promotion claim depending on real G19 is `BLOCK_RELEASE`. Deterministic G24 and unrelated completed gates are unaffected.

## Required next evidence

Before a future G27/G19 promotion attempt, prove boundary identity, orientation, units, variable-step integration, all external source/sink classes, restart/replay cumulative state, and independent closure against `sum(vol1)` across inflow, outflow, mixed-boundary, and restart cases.

No production D-Flow external-net provider was added by this decision.

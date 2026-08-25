# M286 CVC Wet-Dry Sandbox Evidence (M282 Phase D-4 execution)

Date: 2026-08-25
Scope: numerical-sandbox execution of the M282 plan on the CURRENT
first-order CVC closure and wet/dry treatment. Sandbox-only; no solver
behavior changed; `enable_cvc_spatial_phi_t_correction` stays default-off and
G23 remains the only active CVC gate.

## Method

`sandbox/numerical/src/cvc_wetdry.py` ports the production arithmetic 1:1 to
a 1D quad strip (edge normal +x, unit edge length): Audusse
`reconstruct_hydrostatic_pair`, the full HLLC kernel including the dry gate
(zero edge flux when either reconstructed depth <= h_min), the WB
pressure/source pairing, `cvc_side_fluxes` upwind storage closure, the
`max(0, h + dh)` positivity clamp, the dry-cell momentum limit, and the
step.cpp update ordering. The sandbox additionally records the storage the
clamp silently truncates (`clamp_storage_error`), which production does not.

Failure-revealing candidates were registered before any fix
(`tests/test_m286_cvc_wetdry_candidates.py`, 14 tests) with executable sympy
derivations (`proofs/cvc_wetdry_derivations.py`, 8 checks).

## Verdicts against the M282 exit criteria

| # | Criterion | Verdict | Evidence |
|---|---|---|---|
| 1 | positivity | HOLDS only by truncation | Updated `h >= 0` always (clamp). Pre-clamp negatives occur on an overdraw fixture; truncated storage is macroscopic (2.1e-2 on the fixture). With CVC ON the wall-closed storage drift EQUALS `clamp_storage_error` exactly (closure identity, derivation 3 + test); with CVC OFF the identity breaks (baseline jump residual mixes in). Donor-invariance lemma (derivations 2a/2b): CVC never amplifies the donor side, so CVC does not worsen overdraw. |
| 2 | near-dry | REFUTED | Filling a just-wet cell (h = 1.5e-8) across a phi_t jump with CVC ON produces a spurious back-jet, max abs(u) > 0.8 * sqrt(g*h_wet) (observed ~2.83-3.03 vs celerity 3.13); CVC OFF keeps abs(u) < 1e-12 on the same fixture. Root cause (derivation 5): the receiver momentum fluctuation carries the extra term `(phi_up/phi_R - 1) * F_m` (19x baseline at phi_up=1, phi_R=0.05) while the WB pressure/source pairing is not CVC-rescaled. |
| 3 | arbitrary wet/dry | REFUTED (dynamic) / EXACT (static) | Dam-break onto a dry porous bed: the front NEVER advances (dry gate zeroes the wet/dry edge; 200 steps, downstream bitwise 0.0), so the error against any physical reference is O(1) at every resolution — no convergence study is meaningful until a wetting-capable closure exists. Submerged-step flood (eta_wet > z_b_dry) also stalls. Lake-at-rest around a dry island with a phi_t jump is EXACT (bitwise at rest over 50 steps; derivation 4: the Audusse S_topo term at an h_bar = 0 edge closes the cell pressure polygon). |
| 4 | replay | HOLDS | 400-step mixed wet/dry/porous strip with CVC ON: bitwise identical h and hu across snapshot/replay. |
| 5 | candidates before fix | DONE | All refutations are registered as `*_candidate_*` tests that assert the defect at quantified magnitude; a future closure must flip them consciously. |

## Runs (local, 2026-08-25)

- `py -3 sandbox/numerical/proofs/cvc_wetdry_derivations.py` — 8/8 PASS.
- `py -3 -m unittest discover -s sandbox/numerical/tests -p "test_m286*.py"`
  — 14/14 OK.
- Full sandbox discovery (`test_*.py`, includes legacy G1-G3) — 17/17 OK.

## Consequence for M282 / Phase D-4

The sandbox phase is complete with counterexamples, which the M282 plan
explicitly admits ("proof or counterexample"). The current first-order
closure must stay default-off for wet/dry-active domains: criteria 2 and 3
are refuted, and the quantified candidates now define the acceptance bar for
any future high-order CVC / wetting-capable closure (positivity without
truncation, no spurious near-dry jets, an advancing wet/dry front, exactness
preserved for the static island case, bitwise replay). Wiring work remains
BLOCKED until a closure passes all five criteria in this sandbox first.

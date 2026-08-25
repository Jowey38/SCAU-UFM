# M286: CVC wet-dry numerical sandbox execution (M282 Phase D-4 entry)

Date: 2026-08-25
Status: EXECUTED (sandbox-only; no solver change; see the M286 evidence doc)

## Scope

Execute the M282 sandbox plan on the CURRENT first-order kernels before any
solver wiring: prove or refute each M282 exit criterion with failure-revealing
candidates registered first (M253/G-candidate pattern).

## Deliverables

1. `sandbox/numerical/src/cvc_wetdry.py` — faithful 1D-strip port of the
   production kernels (Audusse reconstruction, HLLC with the dry gate, WB
   pairing, CVC side fluxes, positivity clamp, dry momentum limit, step.cpp
   ordering), plus a `clamp_storage_error` diagnostic production lacks.
2. `sandbox/numerical/tests/test_m286_cvc_wetdry_candidates.py` — 14 tests:
   donor-invariance lemma, storage-closure pair, lake-at-rest dry island
   (exactness proof), dam-break dry front stall (refutation), near-dry
   spurious back-jet (refutation, CVC on vs off), positivity clamp truncation
   + drift identity, 400-step bitwise replay.
3. `sandbox/numerical/proofs/cvc_wetdry_derivations.py` — executable sympy
   derivations (8 checks) backing the numerical fixtures.
4. Evidence: `superpowers/specs/2026-08-25-m286-cvc-wetdry-sandbox-evidence.md`.

## Boundaries

- Sandbox-only. `enable_cvc_spatial_phi_t_correction` stays default-off in
  production; G23 remains the only active CVC gate.
- The refuted criteria define the requirements for any future high-order
  CVC / wetting-capable closure; its implementation must flip the
  failure-revealing candidates consciously.

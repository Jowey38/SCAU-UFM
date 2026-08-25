# M282: High-order CVC / arbitrary wet-dry positivity — numerical sandbox plan (Phase D-4 entry)

Date: 2026-08-18
Status: EXECUTED 2026-08-25 via M286 (sandbox counterexamples recorded; see
`superpowers/specs/2026-08-25-m286-cvc-wetdry-sandbox-evidence.md`; solver
wiring stays BLOCKED until a closure passes all five criteria)

## Scope and current baseline

- G23 (M263) landed the FIRST-ORDER opt-in CVC spatial `phi_t` side-fluctuation
  closure: exact `phi_t*h` / Cartesian momentum closure on internal WB edges,
  default-off, machine-epsilon replay. It does NOT claim high-order accuracy,
  arbitrary wet/dry positivity, or near-dry robustness under CVC.
- M250 remap covers dynamic `phi_t` time jumps as an explicit seam only.

## Sandbox exit criteria (per the numerical-sandbox precedent)

Following `2026-05-08-numerical-sandbox-design.md` methodology, a standalone
sandbox (sympy derivation + minimal mixed tri/quad fixtures) must prove,
before ANY solver wiring:

1. positivity: reconstructed depths and updated `h` remain >= 0 for arbitrary
   wet/dry interfaces with CVC correction active (proof or counterexample);
2. near-dry: no spurious velocities beyond the documented h_wet diagnostics
   as cells cross wetting thresholds with `phi_t` jumps;
3. arbitrary wet/dry: dam-break onto dry porous bed against a derived
   reference; lake-at-rest with partially dry porous cells stays exact;
4. replay: bitwise snapshot/rollback/replay with the correction on;
5. failure-revealing candidates registered BEFORE the fix (G-candidate
   pattern), quantifying the current first-order closure's error on each
   fixture.

## Boundaries

- Default-off remains mandatory until every criterion passes and a dedicated
  GoldenTest locks each fixture (G23 stays the only active CVC gate).
- This is a research-scale workstream; it proceeds independently of the
  release-gate criticals (all Phase 1/2 gates are green without it).

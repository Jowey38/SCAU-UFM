# GoldenSuite Reference Tolerances

Authoritative source: `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` §11.

- `u_hydro_tol = 1e-12 m/s`
- `eta_tol = 1e-12 m`
- `conservation_near_tol = 1e-9` (engineering anti-flaky tolerance for recomposed floating-point aggregates; not a Spec §11 constant)

Deterministic path requirement (§10.3): CPU double precision, fixed reduction order.

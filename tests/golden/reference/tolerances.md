# GoldenSuite Reference Tolerances

Authoritative source: `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` §11.

- `u_hydro_tol = 1e-12 m/s`
- `eta_tol = 1e-12 m`
- `conservation_near_tol = 1e-9` (engineering anti-flaky tolerance for recomposed floating-point aggregates; not a Spec §11 constant)
- G24 whole-system deterministic audit: `epsilon_deficit = max(1e-10, 1e-12 * M_ref)` m3, with complete Surface2D/SWMM/D-Flow storage and external-net scope. Missing engine external-net scope is `REVIEW_REQUIRED` regardless of raw residual or tolerance.

- G19 real whole-system audit (M277): coupling residual vs `max(epsilon_deficit, 1e-4 + 1e-6 * max(1, S0))` (M270 real-engine tolerance, unchanged); per-engine internal continuity gaps decomposed via CouplingLib ledger laterals and bounded by the documented `engine_internal_gap_absolute` (G19 fixture: 0.05 m3 from measured SWMM recession error; D-Flow gap at native volume-error scale). Engine gaps never absorb or mask coupling drift.

Deterministic path requirement (§10.3): CPU double precision, fixed reduction order.

# M252 CVC Spatial `phi_t` Discontinuity Design

## Purpose

This document defines the next research/development path after M250/M251.

M250/M251 covers dynamic time-jump remapping of `phi_t` at a sub-step boundary. It conserves `phi_t*h*A` and `phi_t*hu/hv*A` but deliberately leaves the existing HLLC/WB hot path unchanged.

M252 addresses the higher-risk blank zone identified by the main spec §5.5.2:

> `phi_t` spatial step + dynamic perturbation (nonzero velocity) is not guaranteed to be O(dx^2) conservative by standard HLLC scaled with `phi_e_n`; Cea-Vazquez-Cendon augmented HLLC is the registered upgrade path.

## Current Topology

Implemented and green:

- static spatial `phi_t` jump at rest: Audusse single-sided WB pressure/source pairing;
- sloping-bed lake-at-rest: Audusse hydrostatic reconstruction + square-difference topo pairing;
- dynamic time-jump remap: explicit `remap_state_for_phi_t_change(...)`;
- candidate gate: `test_dpm_phi_t_remap` labeled `golden_candidate;cvc;surface2d`.

Open PR topology at time of this design:

- PR #8: GoldenSuite manifest gate, base `feat/m230-stage-record`, state `CLEAN/OPEN`;
- PR #9: M247-F ponded-h infiltration, base `master`, state `CLEAN/OPEN`.

Therefore M252 must be designed before implementation and must not be mixed into the currently green M250/M251 code path.

## Non-goals

- Do not change HLLC wave speeds in this design task.
- Do not introduce runtime mutation of `Phi_c`, `phi_e_n`, or `omega_edge`.
- Do not use `phi_e_n` to compensate storage gaps; storage sovereignty remains `phi_t` only.
- Do not overload G2. G2 remains static hydrostatic spatial jump. Dynamic/CVC tests should be separate candidates, e.g. G13/G14.
- Do not claim release-grade high-order conservation until augmented-HLLC tests and replay evidence exist.

## Physical Problem

For a 1D edge-normal reduction of the DPM equations, storage is:

```text
U_storage = [phi_t*h, phi_t*h*u_n]
```

but the current implementation stores:

```text
U_impl = [h, h*u_n]
```

and applies `phi_t` through pressure/source pairing for static WB and through audits/coupling storage. This is practical and efficient when `phi_t` is static or continuous enough. At a sharp spatial jump with nonzero velocity, however, the edge Riemann problem has a coefficient discontinuity in `phi_t` that cannot be fully represented by standard HLLC plus post-scaled `phi_e_n`.

The CVC-style upgrade should treat the coefficient jump as an augmented source/fluctuation at the interface, not as a post-hoc storage correction.

## Proposed Development Path

### Phase A: Failure-revealing Golden candidate

Add a disabled or candidate-only test that demonstrates the blank zone without changing solver behavior.

Candidate name:

```text
cvc_spatial_phi_t_dynamic_perturbation
```

Suggested future ID:

```text
G14
```

Scenario:

- two or minimal mixed cells with a sharp `phi_t` step, e.g. `1.0 -> 0.4`;
- nonzero normal velocity perturbation crossing the interface;
- no bed slope, no rainfall/infiltration/exchange/friction;
- compare two forms of conservation:
  - implementation depth/momentum state evolution;
  - physical storage/momentum `phi_t*h*A`, `phi_t*hu*A`.

Expected current behavior:

- static WB remains fine at rest;
- dynamic perturbation may show an interface storage/momentum mismatch beyond strict tolerance.

Acceptance for Phase A:

- test is present but not a blocking release gate unless explicitly marked `golden_candidate`;
- evidence records current residual magnitude and confirms no claim of full CVC support.

### Phase B: Pure augmented fluctuation prototype

Add a pure function module before wiring it into `step.cpp`:

```text
libs/surface2d/dpm/cvc_augmented_flux.hpp
libs/surface2d/dpm/cvc_augmented_flux.cpp
```

Inputs:

- left/right reconstructed states;
- left/right `phi_t`;
- edge normal;
- baseline HLLC flux;
- `dt`, local length/area only if needed for bounded correction.

Outputs:

```cpp
struct CvcAugmentedFluxCorrection {
    core::Real mass_correction{0.0};
    core::Real momentum_n_correction{0.0};
    core::Real storage_residual_estimate{0.0};
    core::Real momentum_residual_estimate{0.0};
    bool applied{false};
};
```

Rules:

- correction acts in edge-normal form only;
- correction must be conservative across the edge (left and right receive opposite signed integrated fluxes);
- correction must preserve lake-at-rest exactly when normal velocity is zero;
- correction must be zero when `phi_t_left == phi_t_right`;
- correction must not touch `phi_e_n`/`omega_edge` semantics;
- fail-closed if either `phi_t` is nonfinite or nonpositive.

### Phase C: Step wiring behind an explicit config gate

Add an opt-in configuration flag, default off:

```cpp
bool enable_cvc_spatial_phi_t_correction{false};
```

Default-off is required because §5.5.2 currently declares this as a validation blank zone, not a stable production capability.

Wiring requirements:

- apply only on regular/soft-block edges where WB pairing is assembled;
- do not apply on hard-block edges;
- keep static WB contributions unchanged;
- diagnostics must count and audit the active events:
  - `count_phi_t_jump_events` (required by main spec §5.5.1/§5.5.2);
  - `cvc_mass_correction_volume`;
  - `cvc_momentum_correction_x/y`;
  - max residual estimate.

### Phase D: Promotion to GoldenSuite

After PR #8 lands, use independent Golden candidates:

- `G13 cvc_dynamic_phi_t_remap` (M251; time-jump remap);
- `G14 cvc_spatial_phi_t_dynamic_perturbation` (M252; spatial jump + nonzero velocity).

Do not merge either into `G2 phi_t_jump_hydrostatic`.

## Validation Matrix

| Test | Purpose | Gate |
|---|---|---|
| `test_dpm_phi_t_remap` | time-jump storage/momentum remap | `golden_candidate` now; future G13 |
| `test_well_balanced_phi_t_jump_at_rest` | static spatial jump at rest | current unit/G2 family |
| `test_cvc_spatial_phi_t_dynamic_perturbation` | reveal dynamic spatial blank zone | candidate first |
| future augmented-flux unit tests | prove pure correction invariants | required before step wiring |
| future replay test | deterministic replay with correction enabled | required before release gate |

## Recommended Next Implementation Task

M253 should be **Phase A only**:

1. add a candidate/failure-revealing test for spatial `phi_t` step + dynamic perturbation;
2. record current residual magnitude;
3. do not change solver behavior;
4. decide from evidence whether augmented correction is needed immediately or can remain a designed future path.

This keeps the current green M250/M251 work stable while creating the precise evidence needed for an eventual CVC augmented-HLLC implementation.

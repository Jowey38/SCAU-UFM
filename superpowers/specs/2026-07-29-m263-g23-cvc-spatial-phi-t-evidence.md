# M263 / G23 CVC Spatial `phi_t` Fluctuation Evidence

Date: 2026-07-29
Base: master `98ec373`

## Design correction

M252's draft proposed one common correction flux applied with opposite signs
to implemented `h` states. That cannot conserve physical storage when
`phi_t_left != phi_t_right`, because physical storage is `phi_t*h`:

```text
Delta V = dt*L*F_h*(phi_t_right - phi_t_left)
```

M263 therefore implements side-specific augmented fluctuations. A shared
physical storage flux `F_storage = phi_upwind * F_HLLC` is divided by each
side's own storage porosity:

```text
F_h_left  = F_storage / phi_t_left
F_h_right = F_storage / phi_t_right
```

Thus `-phi_t_left*F_h_left + phi_t_right*F_h_right = 0` exactly. Cartesian
advective momentum uses the same transport porosity and side division,
conserving `phi_t*hu/hv` at the interface. Hydrostatic WB pressure/topography
pairing is unchanged.

## Implementation

- pure `dpm/cvc_augmented_flux.*` returns left/right mass and Cartesian
  momentum fluctuations, baseline/after residual estimates, and applied flag;
- fails closed for non-finite/non-positive phi_t;
- zero correction for uniform phi_t and zero-advective-flux lake at rest;
- `StepConfig::enable_cvc_spatial_phi_t_correction` defaults false;
- only internal non-hard-block (`wb_pairing_assembled`) edges are eligible;
- diagnostics: event count, signed physical mass/momentum correction, max
  after-correction storage residual; all reset to zero on rollback;
- default-off path retains the baseline flux values exactly.

## G23 `cvc_spatial_phi_t_dynamic`

Registered `implemented, ci_gate:true`, independent of G2/G21/G22.

- failure-revealing baseline remains: one-step `phi_t 1.0 -> 0.4` with
  nonzero velocity has storage residual >1e-6;
- opt-in one-step residual <=1e-12;
- opt-in 100-step physical storage conserved <=1e-12;
- 100-step replay is bitwise deterministic for all h/hu/hv/eta states and
  final physical totals;
- event count is exactly one per active step.

Additional unit evidence:

- positive/negative flux, mass and Cartesian momentum physical closure;
- uniform phi returns baseline bitwise;
- invalid phi fail-closed;
- G2-style static phi_t jump at rest with flag on remains balanced 1e-12 and
  records zero dynamic events.

## Verification

- windows-msvc Debug full suite: **135/135 passed**.
- manifest completeness: OK.
- final build `LNK1168 == 0` (transient locked exes were removed and rebuilt;
  no compile/test error).

## Scope boundary

This is an opt-in first-order side-specific storage/momentum fluctuation
correction that closes the M253 conservation blank zone. It does not claim a
complete high-order Cea-Vazquez-Cendon augmented-HLLC implementation,
positivity proof under arbitrary wet/dry fronts, or default production
activation. Those require larger real-mesh and wet/dry Golden evidence.

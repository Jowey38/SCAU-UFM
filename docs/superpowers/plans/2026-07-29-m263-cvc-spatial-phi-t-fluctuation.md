# M263 CVC Spatial `phi_t` Side-Specific Fluctuation Plan

Date: 2026-07-29
Base: master `98ec373`

## Problem correction to M252

M252 proposed one conservative correction flux applied with opposite signs to
left/right `h` states. That cannot conserve physical storage when
`phi_t_left != phi_t_right` because the implemented state is `h`, while the
physical state is `phi_t*h`:

```text
Delta V_physical = dt*L*F_h*(phi_t_right - phi_t_left)
```

Any common corrected `h` flux retains the same mismatch. The augmented
interface must therefore output side-specific fluctuations.

## Adopted fluctuation

For baseline HLLC advective flux `F` and upwind storage porosity
`phi_upwind` (left if mass flux > 0, right if < 0; arithmetic mean for zero
mass flux):

```text
F_storage = phi_upwind * F_mass
F_h_left  = F_storage / phi_left
F_h_right = F_storage / phi_right
```

The step applies `-F_h_left` to the left cell and `+F_h_right` to the right,
so physical storage is exactly conservative:

```text
-phi_left*F_h_left + phi_right*F_h_right = 0
```

Cartesian advective momentum uses the same scaling per side, conserving
`phi_t*hu` and `phi_t*hv`. Hydrostatic pressure/topography WB pairing stays
unchanged and is not included in the CVC correction.

## Invariants

- correction zero when phi_left == phi_right;
- correction zero at lake at rest (baseline advective flux is zero);
- fail closed for non-finite/non-positive phi;
- no mutation of phi_e_n, omega_edge, or Phi_c;
- only internal regular/soft-block edges with WB pairing assembled;
- hard-block edges never apply it;
- default-off `StepConfig::enable_cvc_spatial_phi_t_correction`;
- default-off path bitwise unchanged.

## Diagnostics

- `count_phi_t_jump_events`;
- signed `cvc_mass_correction_volume` (physical volume added by corrections,
  equal and opposite to baseline storage residual);
- signed `cvc_momentum_correction_x/y` in physical momentum-volume units;
- `max_cvc_storage_residual_after`.

## Tests / promotion

- pure function: uniform phi, positive/negative/zero flux, invalid phi,
  physical mass/momentum closure;
- step: M253 baseline still reveals >1e-6 residual; opt-in path <=1e-12;
- static phi jump at rest remains balanced with flag on;
- hard-block correction count stays zero;
- default full suite unchanged;
- independent Golden uses next free ID after G22 (G23), never overloads G2.

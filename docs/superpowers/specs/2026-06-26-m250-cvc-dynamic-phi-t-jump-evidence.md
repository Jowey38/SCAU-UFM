# M250 CVC Dynamic `phi_t` Jump Consistency Evidence

## Scope

This milestone implements the first conservative CVC slice for dynamic storage-porosity changes at sub-step boundaries.

The implemented seam is explicit and opt-in:

- `surface2d/dpm/phi_t_remap.hpp`
- `surface2d/dpm/phi_t_remap.cpp`
- `remap_state_for_phi_t_change(...)`

It remaps `SurfaceState` from an old `phi_t` field to a new `phi_t` field while preserving the physically stored quantities:

- water storage: `sum(phi_t * h * A)`
- stored x momentum: `sum(phi_t * hu * A)`
- stored y momentum: `sum(phi_t * hv * A)`
- bed elevation: `z_b = eta - h`

## Boundary

This is not a full CVC augmented-HLLC implementation for nonzero-velocity spatial `phi_t` discontinuities. Static spatial `phi_t` jumps remain owned by the existing Audusse well-balanced pressure/source pairing and tests.

This slice covers the dynamic/time-jump reconstruction step: when `phi_t(t)` changes at a sub-step boundary, the solver can explicitly convert old storage variables into new storage variables before the next normal `advance_one_step_cpu` call.

## Invariants

For each cell with positive area and positive finite old/new `phi_t`:

```text
V_old = phi_t_old * h_old * A
h_new = V_old / (phi_t_new * A)
z_b   = eta_old - h_old
eta_new = z_b + h_new
u_new = u_old, v_new = v_old when wet
```

This implies:

```text
phi_t_new * h_new  * A == phi_t_old * h_old  * A
phi_t_new * hu_new * A == phi_t_old * hu_old * A
phi_t_new * hv_new * A == phi_t_old * hv_old * A
```

Invalid `phi_t` is fail-closed: non-finite or non-positive old/new values throw `std::invalid_argument` before the original state is committed.

## Tests

`tests/unit/surface2d/test_dpm_phi_t_remap.cpp` covers:

1. no-op remap when `phi_t` is unchanged;
2. per-cell storage conservation under nonuniform `phi_t` jumps;
3. velocity preservation and stored-momentum conservation;
4. bed elevation preservation via `eta - h`;
5. fail-closed invalid `phi_t` with no state mutation;
6. compatibility with the existing static WB path for a uniform dynamic `phi_t` jump followed by one step.

## Status

Implementation is isolated from the existing hot path. Callers must invoke `remap_state_for_phi_t_change(...)` explicitly when a dynamic `phi_t` update is accepted at a sub-step boundary.

# M16 CouplingLib Core Q_limit Design

**Date:** 2026-05-20

## Goal

Add the minimal pure `CouplingLib` core calculation for the coupling hard-gate flow limit:

- `V_limit = 0.9 * phi_t * h * A`
- `Q_limit = V_limit / dt_sub`

M16 keeps this logic inside `libs/coupling/core` and does not connect it to `Surface2DCore`, SWMM, D-Flow FM, shared-cell arbitration, or deficit accounting.

## Scope

M16 extends the existing `ExchangeCellState` with the fields needed to compute the canonical limit:

- `phi_t`
- `h`
- `area`

It adds a small `FlowLimit` value type and a `compute_flow_limit(...)` function that returns both `V_limit` and `Q_limit` for one exchange cell and one positive `dt_sub`.

## API

The public core API gains:

```cpp
struct FlowLimit {
    double v_limit{0.0};
    double q_limit{0.0};
};

[[nodiscard]] FlowLimit compute_flow_limit(const ExchangeCellState& cell, double dt_sub);
```

`compute_flow_limit(...)` validates only the external boundary of the calculation:

- `dt_sub` must be positive.
- `phi_t`, `h`, and `area` must be non-negative.

## Behavior

For valid inputs:

```cpp
v_limit = 0.9 * cell.phi_t * cell.h * cell.area;
q_limit = v_limit / dt_sub;
```

A dry cell or closed storage state with `phi_t == 0.0`, `h == 0.0`, or `area == 0.0` produces zero `V_limit` and zero `Q_limit` without special cases.

## Tests

Add focused `test_coupling_flow_limit` coverage for:

- Canonical formula and field names.
- Zero-limit behavior when `phi_t`, `h`, or `area` is zero.
- Rejection of non-positive `dt_sub`.
- Rejection of negative `phi_t`, `h`, or `area`.

## Boundaries

- Do not introduce `Q_max_safe` or any alias for `Q_limit`.
- Do not scale by `CFL_safety`.
- Do not add `mass_deficit_account`, arbitration, scheduler, fault states, adaptive timestep selection, or engine adapters.
- Do not add dependencies from `libs/coupling/core` to `libs/surface2d`, SWMM, D-Flow FM, or 1D engine ABIs.

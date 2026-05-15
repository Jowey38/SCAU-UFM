# M6 Surface2D Boundary Conditions Evidence

**Date:** 2026-05-16
**Plan:** `superpowers/plans/2026-05-14-plan-m6-surface2d-boundary-conditions.md`
**M5 Basis:** `superpowers/specs/2026-05-13-plan-m5-surface2d-wetting-boundary-evidence.md`

## Scope

M6 adds explicit Wall and Open boundary-edge semantics for `Surface2DCore` through a new `BoundaryConditions` container and a four-argument `advance_one_step_cpu` overload. Default kind is Wall; the existing three-argument DPM overload forwards to the new overload with a default Wall container so M5 callers retain prior semantics.

## Local Validation

- `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build build/windows-msvc`: PASS
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "test_(surface_state|surface_step|dpm_fields|hydrostatic_reconstruction|hllc_flux|phi_t_source|surface2d_golden_minimal|cfl_rollback|conservative_update|wetting_drying|boundary_flux|boundary_conditions)$"`: PASS, 12/12 tests passed
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure`: PASS, 16/16 tests passed

## Coverage

- `test_boundary_conditions`: verifies Wall produces no residual, Open accumulates inside-cell residual under non-orthogonal inside velocity, and mismatched boundary sizes are rejected.
- `test_boundary_flux`: retains the M5 default-Wall documentation contract and adds a default-Wall regression against the new four-argument overload.
- Existing M3/M4/M5 Surface2D tests remain passing.

## Plan Adjustment

Plan-level self-review missed that HLLC's `left_un == 0 && right_un == 0` short-circuit fires when the first boundary edge normal (E0 along x-axis, normal along y-axis) is orthogonal to the inside cell's chosen velocity vector. The Open test was adjusted to set both `hu=1.0` and `hv=1.0` on the inside cell so the normal-velocity projection is non-zero. The HLLC path was left unchanged.

## Boundaries

- `Surface2DCore` remains independent of SWMM, D-Flow FM, `CouplingLib`, and 1D engine ABIs.
- Prescribed (h, u_n) boundaries and momentum-flux completion remain out of scope.
- This evidence does not claim GoldenSuite release readiness.

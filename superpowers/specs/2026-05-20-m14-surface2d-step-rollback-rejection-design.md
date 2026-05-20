# M14 Surface2D Step Rollback/Rejection Design

## Scope

M14 adds the smallest testable `Surface2DCore` CPU reference slice for rejected-step semantics at the existing `advance_one_step_cpu(...)` boundary.

When raw `max_cell_cfl > C_rollback`, `StepDiagnostics::rollback_required` must be `true` and the CPU step must leave the supplied `SurfaceState` unchanged. The step may still compute CFL, edge diagnostics, and cell residual diagnostics needed by callers, but it must not commit depth or momentum updates for a rejected step.

## Normative Basis

- `superpowers/specs/2026-04-22-symbols-and-terms-reference.md` defines `max_cell_cfl` as raw physical CFL, not scaled by `CFL_safety`.
- `superpowers/specs/2026-04-22-symbols-and-terms-reference.md` defines `C_rollback` as the rollback trigger threshold compared directly with raw `max_cell_cfl`.
- `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` keeps `libs/surface2d/` responsible for 2D numerical advancement and CFL diagnostics, while reserving full `snapshot / rollback / replay` orchestration for `CouplingLib`.

## Design

M14 keeps the rollback boundary inside `Surface2DCore` minimal:

1. `advance_one_step_cpu(...)` continues to compute `StepDiagnostics` with raw `max_cell_cfl` and `rollback_required`.
2. The DPM-aware CPU step still fills edge and cell residual diagnostics for the attempted step.
3. If `diagnostics.rollback_required` is `true`, the function returns diagnostics before calling state-update routines.
4. If `diagnostics.rollback_required` is `false`, the existing depth and momentum update path remains unchanged.

This establishes a state mutation contract without adding a `SurfaceSnapshot` type, replay API, deficit ledger, adaptive timestep selector, or coupling orchestration.

## Acceptance Criteria

- A rejected CPU step reports `rollback_required == true` and preserves every cell's conserved depth, conserved momentum, and `eta`.
- The rejected step still exposes attempted-step diagnostics so callers can see `max_cell_cfl` and residuals that caused the rejection.
- An accepted CPU step with the same mesh/state shape and a permissive `C_rollback` still mutates state through the existing update path.
- Existing M9-M13 Surface2D tests remain passing.

## Out of Scope

M14 does not add `CouplingLib`, global `snapshot / rollback / replay` APIs, `mass_deficit_account`, adaptive timestep selection, CUDA behavior, SWMM, D-Flow FM, 1D engine interfaces, or release-level GoldenSuite claims.

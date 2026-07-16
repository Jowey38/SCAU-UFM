# M240 Roof Q_limit/Deficit Arbitration + Step-Driver Evidence

## Scope

Puts CouplingLib arbitration in front of the real roof -> SWMM adapter and adds the dt_sub scheduling seam. Base: `master` `fd82b44` (real SwmmEngine, PR #33).

New library `libs/coupling/driver/` (`scau::coupling_driver`), which sees only DTOs, `ISwmmEngine`, and coupling-core primitives (project-layout-design driver boundary).

## Components

### RoofExchangeGate (`coupling/driver/roof_exchange_gate.hpp`)

Arbitration decorator wrapping a downstream `surface2d::RoofDrainageAcceptanceFn` (the SWMM adapter):

- `Q_limit` hard gate via canonical two-step expression (`core::compute_flow_limit`): `V_limit = 0.9 * phi_t * h * A`, `Q_limit = V_limit / dt_sub`
- deficit repayment reservation via `core::evaluate_exchange`: `q_repay = min(deficit/dt_sub, Q_limit)` shrinks roof headroom; roof water itself never repays or accrues `mass_deficit_account` (M247 invariant: rejected roof water stays roof pending/overflow on the surface2d side)
- within-substep grant ledger per source cell so successive intents cannot jointly exceed `Q_limit`; `begin_substep()` resets
- missing endpoint state -> full fail-closed reject `HealthBlocked`, downstream never called
- clamped intents forward only `v_granted` downstream; acceptance is composed against the ORIGINAL request (downstream rejection reason wins; clamp shortfall reports `CapacityLimited`)
- invalid volumes forward unchanged so the downstream adapter stays the single fail-closed authority for them

### RoofSwmmStepDriver (`coupling/driver/roof_swmm_step_driver.hpp`)

dt_sub scheduling seam: owns the acceptance adapter + gate; `begin_substep()` resets both per-substep ledgers; `acceptance_fn()` returns the arbitrated `RoofDrainageAcceptanceFn` for `advance_one_step_cpu`; `advance_engine()` steps SWMM by the same `dt_sub`.

## Tests

- `test_coupling_roof_exchange_gate` (mock, 9 cases): passthrough under headroom; clamp to `Q_limit` with `CapacityLimited` and clamped downstream write; deficit reservation shrinks headroom (20 m3 deficit -> 2 m3/s reserved of 9 m3/s); missing endpoint fail-closed without downstream write; downstream surcharge reason wins with original-volume accounting; zero-headroom (dry endpoint) skips downstream; within-substep grant accumulation with substep reset; invalid volume forwarded fail-closed; invalid dt at construction
- `test_coupling_roof_swmm_step_driver` (mock, 4 cases): arbitrate-then-write; substep reset of both ledgers; engine advanced by dt_sub; invalid dt fails closed (drainage-layer `SwmmEngineError`)
- `test_coupling_roof_qlimit_swmm_real` (real engine, gated `SCAU_EMBED_SWMM`): over-limit intent (0.1 m3/s vs Q_limit 0.075 m3/s) clamped by CouplingLib BEFORE the adapter writes; real solver routes exactly the granted 0.075 m3/s; 3-substep dt_sub schedule with matching engine elapsed time

## Fix folded in

Latent namespace bug in drainage headers: unqualified `core::Real` inside `scau::coupling::drainage` resolves to `scau::coupling::core` once any TU also includes `coupling/core/state.hpp`. All drainage headers/sources now spell `scau::core::Real` (logged as bug-018).

## Verification

Worktree `H:/githubcode/SCAU-UFM/.worktrees/m240-qlimit`, preset `windows-msvc`, `SCAU_EMBED_SWMM=ON`:

- TDD: gate and driver tests written first and observed failing (missing library / wrong error type) before implementation
- Full suite: **62/62 passed**, no LNK1168/compile errors

## Non-goals / follow-ups

- Shared-cell two-engine arbitration (needs D-Flow FM river adapter on master)
- Endpoint-state provider backed by live `CouplingState` cells (currently injected as a std::function seam)
- Snapshot/rollback/replay integration for the roof path

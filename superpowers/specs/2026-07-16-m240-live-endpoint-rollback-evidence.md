# M240 Live CouplingState Endpoint Provider + Roof Rollback Evidence

## Scope

Base: `master` `c65137f` (roof Q_limit arbitration, PR #35). Two increments:

1. `CouplingStateEndpointProvider` — binds the gate's endpoint-state seam to LIVE `core::CouplingState` cells (replaces caller-frozen lambda copies).
2. Roof-path rollback semantics — a rolled-back surface substep's roof writes never enter SWMM routing.

## CouplingStateEndpointProvider

`coupling/driver/coupling_state_endpoint_provider.hpp`:

- `RoofEndpointMap` = roof source cell index -> exchange cell index (PreProc-style static mapping)
- read-only observer over `core::CouplingState`; satisfies `ExchangeEndpointStateFn`
- unmapped roof cell or out-of-range exchange index -> `std::nullopt` -> gate fail-closes `HealthBlocked`
- reads through `state.cells()` on every call, so canonical enqueue -> `replay_pending()` mutations (deficit updates, volume changes) are visible to arbitration without rebuilding anything

## Roof rollback

Rollback happens BEFORE `advance_engine()` in the coupling order, so undo means zeroing the engine-side API inflow buffer:

- `SwmmRoofDrainageAcceptanceAdapter::rollback_step()` — writes `set_node_lateral_inflow(node, 0)` for every node written since `begin_step()`, then clears the ledger
- `RoofSwmmStepDriver::rollback_substep()` — adapter rollback + gate grant-ledger reset; engine time untouched; `substep_count` unchanged (the substep will be replayed)

Intentionally NOT included: enqueueing roof volumes as `CouplingEvent`s. Per the M247 invariant, roof water never mutates exchange-cell volume or `mass_deficit_account` — the exchange cell is a capacity gauge for the roof path, so there is nothing to replay in the ledger. System-boundary accounting of roof->SWMM transfers belongs to the future tri-coupling mass audit.

## Tests (TDD)

- `test_coupling_state_endpoint_provider` (5): resolves mapped live cell; unmapped -> nullopt; out-of-range -> nullopt; sees post-replay deficit mutation; end-to-end gate arbitration against live deficit (20 m3 deficit reserves 2 of 9 m3/s), dry live cell zero headroom, unmapped -> HealthBlocked
- `test_coupling_roof_rollback` (3): adapter rollback zeroes written nodes and restarts ledger; driver rollback discards substep without advancing engine and restores FULL Q_limit headroom for the replayed substep; live-provider + rollback composition
- `test_coupling_roof_rollback_swmm_real` (1, `SCAU_EMBED_SWMM`): accepted write rolled back, engine then stepped -> routed lateral inflow 0 (nothing entered real routing); replayed substep routes normally (0.05 m3/s)

## Verification

Worktree `H:/githubcode/SCAU-UFM/.worktrees/m240-live-endpoint`, preset `windows-msvc`:

- Full suite: **65/65 passed** (62 prior + 3 new targets)
- No LNK1168 / compile errors

## Follow-ups

- Live provider wiring into a future tri-coupling driver config (map production from PreProc/STCF)
- Roof->SWMM transfer accounting in the system mass audit at the tri-coupling boundary

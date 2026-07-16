# M240 Roof Q_limit/Deficit Arbitration + Step-Driver Wiring Plan

Base: master `fd82b44` (real SwmmEngine, PR #33). Branch: `feat/m240-roof-qlimit-arbitration`.

## Goal

Put CouplingLib arbitration in front of `SwmmRoofDrainageAcceptanceAdapter` and wire dt_sub scheduling, without moving arbitration semantics into any engine adapter (spec firewall).

## Design

New library `libs/coupling/driver/` (`scau::coupling_driver`), per project-layout-design: driver sees DTOs and abstract interfaces only.

### RoofExchangeGate (arbitration decorator)

`surface2d::RoofDrainageAcceptance operator()(const surface2d::RoofDrainageIntent&)` wrapping a downstream `RoofDrainageAcceptanceFn` (the SWMM adapter):

1. Invalid volume (non-finite/negative): forward unchanged; downstream fail-closes (single source of truth).
2. Resolve endpoint state via injected `ExchangeEndpointStateFn` = `std::function<std::optional<core::ExchangeCellState>(const RoofDrainageIntent&)>`. Missing state -> full fail-closed reject `HealthBlocked` (arbitration cannot vouch), downstream never called.
3. Arbitrate with core semantics: `q_request = requested_volume / dt_sub`; `core::evaluate_exchange` gives `q_granted = min(q_request, Q_limit - q_repay)` where `V_limit = 0.9*phi_t*h*A`, `Q_limit = V_limit/dt_sub` (canonical two-step expression) and `q_repay` is the outstanding-deficit repayment reservation.
4. Forward CLAMPED intent (`requested_volume = v_granted`) downstream; if `v_granted == 0` skip downstream.
5. Compose acceptance against the ORIGINAL request: `accepted = downstream.accepted`, `rejected = original - accepted`; reason = downstream reason if downstream rejected, else `CapacityLimited` if clamped, else `None`.

M247 invariant preserved: the gate is a PURE READ of exchange state. Roof unmet volume is never enqueued as `mass_deficit_account` (rejected roof water returns to roof pending/overflow on the surface2d side); `v_repay` is a capacity reservation only, never drawn from roof water.

The endpoint-state provider decides what the exchange cell represents (mapped surface cell or roof buffer endpoint); the gate is agnostic.

### RoofSwmmStepDriver (dt_sub scheduling)

Holds `ISwmmEngine&`, an owned `SwmmRoofDrainageAcceptanceAdapter`, gate config, fixed `dt_sub`:

- `begin_substep()` — clears adapter per-node accumulation (delegates `begin_step()`), increments substep counter
- `acceptance_fn()` — returns `RoofDrainageAcceptanceFn` = gate over adapter (pass to `advance_one_step_cpu`)
- `advance_engine()` — `engine.step(dt_sub)`; the SWMM engine advances on the same dt_sub schedule as the surface substep
- `dt_sub()`, `substep_count()` accessors

## Tasks (TDD)

1. RED tests `test_coupling_roof_exchange_gate.cpp` (MockSwmmEngine): passthrough under headroom; Q_limit clamp -> partial accept `CapacityLimited` with clamped q written downstream; deficit reservation shrinks headroom; missing endpoint state -> `HealthBlocked`, no downstream write; surcharged downstream reason passes through with original-volume accounting; zero-headroom skips downstream.
2. GREEN: `roof_exchange_gate.hpp/cpp`.
3. RED tests `test_coupling_roof_swmm_step_driver.cpp` (mock): substep lifecycle clears accumulation; engine advanced by dt_sub per `advance_engine`; acceptance fn arbitrates then writes.
4. GREEN: `roof_swmm_step_driver.hpp/cpp`.
5. Real-engine integration (gated `SCAU_EMBED_SWMM`): clamped intent writes clamped inflow into real SWMM and routes; driver substep loop against real engine.
6. Evidence doc + full suite + PR.

## Non-goals

- No shared-cell two-engine arbitration (needs D-Flow FM on master).
- No changes to `SwmmRoofDrainageAcceptanceAdapter` or `surface2d`.
- No deficit write-off/replay policy changes.

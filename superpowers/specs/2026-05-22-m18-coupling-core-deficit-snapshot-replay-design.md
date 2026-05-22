# M18 CouplingLib Core Deficit Snapshot Replay Design

**Date:** 2026-05-22

## Goal

Add the minimal pure `CouplingLib` core state integration for `mass_deficit_account` so snapshot, rollback, and replay preserve deficit ledger consistency.

## Scope

M18 builds directly on M17's standalone deficit ledger functions. It embeds one `MassDeficitAccount` in each `ExchangeCellState` and extends pending `CouplingEvent` replay with non-negative unmet and repayment volumes.

This keeps the ledger in the same persistence path as existing coupling cell volume state:

- `CouplingState::snapshot()` captures cell volumes and their `mass_deficit_account` values.
- `CouplingState::rollback(...)` restores both volume and deficit ledger values from the snapshot.
- `CouplingState::replay_pending()` reapplies pending exchange events to both volume and deficit ledger state.

## API

Move `MassDeficitAccount` before `ExchangeCellState` and embed it in each cell:

```cpp
struct MassDeficitAccount {
    double volume{0.0};
};

struct ExchangeCellState {
    double volume{0.0};
    MassDeficitAccount mass_deficit_account{};
    double phi_t{0.0};
    double h{0.0};
    double area{0.0};
};
```

Extend `CouplingEvent` with optional deficit ledger deltas while preserving existing event defaults:

```cpp
struct CouplingEvent {
    std::size_t exchange_cell_index{0U};
    double volume_delta{0.0};
    double unmet_volume{0.0};
    double repayment_volume{0.0};
};
```

## Behavior

For each pending event in `CouplingState::replay_pending()`:

1. Apply `volume_delta` to the exchange cell volume exactly as before.
2. Roll `unmet_volume` into the cell's `mass_deficit_account` with `roll_deficit(...)`.
3. Apply `repayment_volume` with `apply_repayment(...)`.
4. Clear pending events after replay, exactly as before.

The event defaults keep existing M15/M16/M17 tests valid: events that only set `volume_delta` do not change deficit state.

## Validation

Focused `test_coupling_core_state` coverage should prove:

- snapshots capture deficit ledger values and remain immutable after replay
- rollback restores deficit ledger values from the snapshot
- rollback preserves pending deficit events for replay
- existing volume replay behavior remains intact

Existing `test_coupling_mass_deficit_account` continues to cover pure ledger update and repayment edge cases.

## Boundaries

- Do not add SWMM, D-Flow FM, `Surface2DCore`, or any 1D engine ABI coupling.
- Do not add shared-cell arbitration, `priority_weight`, scheduler behavior, or fault orchestration.
- Do not add write-off logic.
- Do not redefine `Q_limit`, `V_limit`, or `epsilon_deficit` semantics.
- Do not add `epsilon_deficit` tolerance to this correctness path.

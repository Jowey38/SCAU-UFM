# M15 CouplingLib Core Snapshot/Rollback/Replay Design

**Date:** 2026-05-20

## Goal

M15 introduces the first minimal `CouplingLib` core slice: a pure C++ state container that can take named `snapshot`s, `rollback` to a saved snapshot, and `replay` pending core events after rollback.

## Scope

This milestone creates `libs/coupling/core` as an independent library target and adds focused unit tests under `tests/unit/coupling`. The implementation is intentionally small: it stores scalar exchange-cell state and a replay queue of core events, then proves rollback restores the saved state and replay reapplies queued events in order.

M15 does not connect to `Surface2DCore`, SWMM, D-Flow FM, or any 1D engine ABI. It does not implement `Q_limit`, `V_limit`, `mass_deficit_account`, shared-cell arbitration, adaptive timestep selection, fault states, or production coupling orchestration.

## Architecture

`CouplingState` owns a vector of `ExchangeCellState` values and a replay queue of `CouplingEvent` values. Each event records a target exchange-cell index and a volume delta; applying an event mutates only that cell's `volume`.

A `CouplingSnapshot` is an immutable copy of the exchange-cell state at a point in time. `CouplingState::snapshot()` returns this copy. `CouplingState::rollback(snapshot)` replaces current cell state with the snapshot contents and preserves queued events so callers can replay attempted exchange work after restoring state.

`CouplingState::replay_pending()` applies queued events in insertion order and clears the queue after successful replay. Invalid event target indices are rejected at enqueue time, keeping replay deterministic and free from partial failure for the M15 slice.

## Public API

The initial header lives at `libs/coupling/core/include/coupling/core/state.hpp` and exposes:

```cpp
namespace scau::coupling::core {

struct ExchangeCellState {
    double volume{0.0};
};

struct CouplingEvent {
    std::size_t exchange_cell_index{0U};
    double volume_delta{0.0};
};

class CouplingSnapshot {
public:
    [[nodiscard]] const std::vector<ExchangeCellState>& cells() const noexcept;

private:
    friend class CouplingState;
    explicit CouplingSnapshot(std::vector<ExchangeCellState> cells);
    std::vector<ExchangeCellState> cells_;
};

class CouplingState {
public:
    explicit CouplingState(std::vector<ExchangeCellState> cells);

    [[nodiscard]] const std::vector<ExchangeCellState>& cells() const noexcept;
    [[nodiscard]] CouplingSnapshot snapshot() const;

    void enqueue_event(CouplingEvent event);
    void rollback(const CouplingSnapshot& snapshot);
    void replay_pending();

private:
    std::vector<ExchangeCellState> cells_;
    std::vector<CouplingEvent> pending_events_;
};

}  // namespace scau::coupling::core
```

## Validation

Unit tests should prove:

1. `snapshot()` captures the current exchange-cell volumes and remains immutable after later state changes.
2. `rollback(snapshot)` restores all exchange-cell volumes from the snapshot.
3. `rollback(snapshot)` preserves pending events and `replay_pending()` reapplies them in enqueue order.
4. `enqueue_event(...)` rejects out-of-range exchange-cell indices.

## Boundaries

- Do not add SWMM, D-Flow FM, or 1D engine references.
- Do not add Surface2D dependencies.
- Do not add deficit ledger semantics or `mass_deficit_account`.
- Do not implement `Q_limit` or `V_limit` in M15.
- Do not claim GoldenSuite or release-level evidence.

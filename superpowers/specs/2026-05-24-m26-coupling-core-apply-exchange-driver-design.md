# M26 CouplingLib Core Apply Exchange Single Donor Driver Design

**Date:** 2026-05-24

## Goal

Close the single-donor exchange → event → counter full-chain by adding one method on `CouplingState` that atomically composes the M22 pipeline evaluation, the M15 event enqueue, and the M24 counter record. This gives adapters a single entry point instead of three separate calls.

## Scope

M26 adds one public method to `CouplingState`:

```cpp
ExchangePipelineDecision apply_exchange(std::size_t cell_index, const ExchangeRequest& request);
```

The method:

1. Reads `cells_[cell_index]` to get the current `ExchangeCellState`.
2. Calls `evaluate_exchange_pipeline(cell, request)`.
3. Constructs a `CouplingEvent` from the decision and calls `enqueue_event(...)`.
4. Calls `record_pipeline_decision(decision)` to update `RuntimeCounters`.
5. Returns the `ExchangePipelineDecision` so the caller (adapter) can read the granted flow, unmet volume, drain split plan, and engagement flags.

M26 does **not** call `replay_pending()` — the caller (scheduler) decides when to replay.

### CouplingEvent field mapping

```cpp
CouplingEvent{
    .exchange_cell_index = cell_index,
    .volume_delta = decision.exchange.v_granted,
    .unmet_volume = decision.exchange.v_unmet,
    .repayment_volume = decision.exchange.v_repay,
};
```

`volume_delta` is set to `v_granted` (positive = flow from 2D surface to 1D engine). This is consistent with the existing `enqueue_event` / `replay_pending` semantics where `volume_delta` adds to `cell.volume`.

## Normative Anchors

- `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` §2.1: Phase 1 requires `exchange` capability in `libs/coupling/core/`.
- §6.1: adapter calls core to request exchange; core returns `ExchangeDecision`.
- §6.3: `mass_deficit_account` update via replay; deficit roll and repayment happen inside `replay_pending()` when consuming the enqueued event.
- §14.1: counter update driven by M23 engagement flags via M24 `record_pipeline_decision`.

## Tests

Create `tests/unit/coupling/test_coupling_apply_exchange.cpp` with:

- `ApplyExchangeReturnsDecisionConsistentWithPipeline` — the returned decision matches what `evaluate_exchange_pipeline(cells[i], request)` would return for the same cell state and request.
- `ApplyExchangeEnqueuesEventButDoesNotMutateVolume` — after `apply_exchange`, `cells()[i].volume` is unchanged; after `replay_pending()`, it changes by `v_granted`.
- `ApplyExchangeUpdatesRuntimeCounters` — after `apply_exchange` with a decision where `drain_split_engaged == true`, `runtime_counters().count_drain_split` is incremented.
- `ApplyExchangeReplayUpdatesMassDeficitAccount` — after `apply_exchange` + `replay_pending`, the cell's `mass_deficit_account` is updated by `v_unmet` and `v_repay`.
- `ApplyExchangeRejectsOutOfRangeCellIndex` — `apply_exchange(999, request)` throws `std::out_of_range`.

Register `test_coupling_apply_exchange` in `tests/unit/coupling/CMakeLists.txt`.

## Boundaries

- `apply_exchange(...)` does not call `replay_pending()`.
- `apply_exchange(...)` does not introduce multi-donor arbitration, `priority_weight`, `V_limit_k`, or vector-form DTOs.
- `apply_exchange(...)` does not introduce FaultController, scheduler, or mapping logic.
- `apply_exchange(...)` does not connect to SWMM, D-Flow FM, `Surface2DCore`, or any 1D engine ABI.
- `apply_exchange(...)` does not introduce snapshot persistence or IO.
- The event's `volume_delta` is `v_granted` (positive = extraction from 2D); sign convention is inherited from M15 existing semantics.

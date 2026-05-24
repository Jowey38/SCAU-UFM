# M24 CouplingLib Core Runtime Counters Snapshot Design

**Date:** 2026-05-24

## Goal

Lift the M23 per-invocation engagement flags into a `RuntimeCounters` aggregate owned by `CouplingState`, and bring those counters into the existing `snapshot` / `rollback` coverage so the snapshot schema required by §6.5 and the strict zero-diff replay invariant in §8.3 hold for `count_drain_split` and `count_negative_depth_fix`.

## Scope

M24 is the smallest stateful slice that satisfies the §14.1 contract for the two coupling-side counters and the §6.5 snapshot coverage clause. It only touches `libs/coupling/core` and its existing tests.

M24 introduces:

- `RuntimeCounters` value type with `count_drain_split` and `count_negative_depth_fix` (`std::size_t`).
- `CouplingState::runtime_counters()` accessor returning a const reference.
- `CouplingState::record_pipeline_decision(const ExchangePipelineDecision&)` that increments counters strictly from M23 flags.
- `CouplingSnapshot::runtime_counters()` accessor symmetrical to `cells()`.
- `CouplingState::snapshot()` captures the counters; `rollback()` restores them.

M24 deliberately excludes:

- `apply_exchange(...)` style driver that combines pipeline + event enqueue + counter update — reserved for ≥ M25.
- Multi-donor arbitration, `priority_weight`, `V_limit_k`, vector-form DTOs.
- Any other §14.1 counter (`count_velocity_clamp`, `count_flux_cutoff`, `count_drag_matrix_degenerate`, `count_ai_prior_conflict`, `count_mem_pressure_events`, `max_cell_cfl`) — these belong to other subsystems.
- `epsilon_deficit` tolerances; counters are integers so equality is exact.
- Schema serialization of snapshots to disk; M24 stays in-memory.

## Normative Anchors

- `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` §6.5: snapshot must cover `RuntimeCounters` (in-memory coverage in M24; persistent schema fields out of scope).
- `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` §8.3: deterministic replay requires field-by-field diff exactly 0 on counters — integer accumulators meet this trivially when sourced from the same M23 flags.
- `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` §14.1: `count_drain_split` and `count_negative_depth_fix` are mandated runtime diagnostics.
- `superpowers/specs/2026-04-22-symbols-and-terms-reference.md`: counter names are part of machine-facing naming constraints.

## API

```cpp
struct RuntimeCounters {
    std::size_t count_drain_split{0};
    std::size_t count_negative_depth_fix{0};
};

class CouplingSnapshot {
public:
    [[nodiscard]] const std::vector<ExchangeCellState>& cells() const noexcept;
    [[nodiscard]] const RuntimeCounters& runtime_counters() const noexcept;

private:
    friend class CouplingState;
    CouplingSnapshot(std::vector<ExchangeCellState> cells, RuntimeCounters counters);
    std::vector<ExchangeCellState> cells_;
    RuntimeCounters runtime_counters_;
};

class CouplingState {
public:
    explicit CouplingState(std::vector<ExchangeCellState> cells);

    [[nodiscard]] const std::vector<ExchangeCellState>& cells() const noexcept;
    [[nodiscard]] const RuntimeCounters& runtime_counters() const noexcept;
    [[nodiscard]] CouplingSnapshot snapshot() const;

    void enqueue_event(CouplingEvent event);
    void rollback(const CouplingSnapshot& snapshot);
    void replay_pending();
    void record_pipeline_decision(const ExchangePipelineDecision& decision);

private:
    std::vector<ExchangeCellState> cells_;
    RuntimeCounters runtime_counters_{};
    std::vector<CouplingEvent> pending_events_;
};
```

`record_pipeline_decision(...)` is the only counter mutation entry point. It inspects only the two M23 booleans and increments the matching counter by 1 per call.

## Behavior

```cpp
void CouplingState::record_pipeline_decision(const ExchangePipelineDecision& decision) {
    if (decision.drain_split_engaged) {
        ++runtime_counters_.count_drain_split;
    }
    if (decision.negative_depth_fix_engaged) {
        ++runtime_counters_.count_negative_depth_fix;
    }
}

CouplingSnapshot CouplingState::snapshot() const {
    return CouplingSnapshot{cells_, runtime_counters_};
}

void CouplingState::rollback(const CouplingSnapshot& snapshot) {
    cells_ = snapshot.cells_;
    runtime_counters_ = snapshot.runtime_counters_;
}
```

`replay_pending()` is intentionally left unchanged. It consumes `CouplingEvent`s, not `ExchangePipelineDecision`s, and updates only `cells_` plus the deficit ledger. M24 locks "replay does not mutate runtime counters" as an invariant: subsequent pipeline invocations after replay must explicitly call `record_pipeline_decision(...)` to update counters, mirroring the §8.3 strict-equality contract on the replay path.

## Tests

Extend `tests/unit/coupling/test_coupling_core_state.cpp` with:

- `RuntimeCountersStartAtZero` — fresh state reports both counters as 0.
- `RecordPipelineDecisionIncrementsDrainSplitCounter` — drain_split flag only increments `count_drain_split`.
- `RecordPipelineDecisionIncrementsNegativeDepthFixCounter` — negative_depth_fix flag only increments `count_negative_depth_fix`.
- `RecordPipelineDecisionIncrementsBothCountersWhenBothFlagsEngaged` — both flags true increment both counters by 1.
- `RecordPipelineDecisionLeavesCountersUnchangedWhenNoFlagsEngaged` — both flags false yield no increment.
- `SnapshotCapturesRuntimeCountersAndRemainsImmutable` — snapshot.runtime_counters() reflects values at snapshot time and is unaffected by subsequent state mutations.
- `RollbackRestoresSnapshotRuntimeCounters` — rollback brings counters back to snapshot values.
- `ReplayPendingDoesNotMutateRuntimeCounters` — enqueue + replay leaves counters unchanged (only deficit and volume change).

Existing 7 tests remain untouched.

## Boundaries

- `record_pipeline_decision(...)` only reads M23 flags; it must not re-derive engagement from `decision.drain_split.micro_steps` or `decision.exchange.v_granted`.
- `replay_pending()` must not touch counters in M24.
- No new dependency on `evaluate_exchange_pipeline(...)`; counters consume decisions provided by callers.
- No multi-donor logic, `priority_weight`, `V_limit_k`, or DTO refactor.
- No persistence, schema, or IO change; in-memory only.
- No connection to `Surface2DCore`, SWMM, D-Flow FM, or any 1D engine ABI.

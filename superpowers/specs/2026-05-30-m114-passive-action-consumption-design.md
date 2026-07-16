# M114 Passive Action Consumption Design

## Scope

M114 defines a design-only boundary for a future passive action-consumption stage after M111-M113.

This document does not add code, tests, scheduler control, runtime isolation, reconnect, abort, real adapter integration, release-gate behavior, CI evidence, GoldenSuite evidence, or merge/release readiness claims.

## Motivation

M111-M112 established that a mock scheduler can observe proposed FaultController actions without changing exchange scheduling, replay, or audit behavior. The next safe design step is to define how a future mock scheduler could consume proposed actions as records while still not executing runtime control.

The desired future chain remains mock-only and passive:

```text
EngineReport
→ classify_engine_health(...)
→ aggregate_engine_health(...)
→ make_fault_controller_diagnostic(...)
→ propose_fault_controller_action(...)
→ observe_mock_scheduler_fault_action(...)
→ consume_mock_scheduler_fault_action(...)
→ run_mock_coupling_scheduler_step(...)
```

The proposed consumption stage must be a DTO/reporting boundary only. It must not become scheduler control.

## Proposed DTO Boundary

A future implementation may add a core-owned DTO similar to:

```text
MockCouplingSchedulerFaultConsumption
```

Recommended fields:

- preserved `FaultControllerProposedAction` input;
- preserved `MockCouplingSchedulerFaultObservation` input or equivalent observation summary;
- `consumed_state` copied from the proposed action state;
- `review_required_consumed` boolean derived from `review_required`;
- `exchange_scheduling_allowed` boolean that remains true in the passive stage;
- `replay_allowed` boolean that remains true in the passive stage;
- `audit_allowed` boolean that remains true in the passive stage;
- `executed_isolation = false`;
- `executed_reconnect = false`;
- `executed_abort = false`.

## Proposed Helper Boundary

A future implementation may add a side-effect-free helper similar to:

```text
consume_mock_scheduler_fault_action(...)
```

Recommended behavior:

- accept a `FaultControllerProposedAction` or `MockCouplingSchedulerFaultObservation`;
- preserve the input record;
- copy the proposed/observed state into the consumption record;
- set `review_required_consumed = true` only for `review_required`;
- keep exchange scheduling, replay, and audit allowed;
- keep all executed action flags false;
- avoid mutating `CouplingState`;
- avoid calling adapter lifecycle APIs;
- avoid changing scheduler request, arbitration, replay, or audit ordering.

## Required Tests For Future Implementation

A future M115-style implementation should include focused tests proving:

1. `continue_run` actions are consumed as passive records.
2. `review_required` actions are consumed as passive records.
3. Consumption preserves diagnostic, health, and proposed action fields.
4. Consumption does not promote proposed execution flags into executed results.
5. Consumption before mock scheduler exchange does not change exchange decisions.
6. Consumption before replay does not change replayed cell volume, depth, or deficit account.
7. Consumption before audit does not change system-mass baseline, current total, residual, or conservation flag.

## Confirmed Negative Boundary

The future passive action-consumption boundary must not add:

- FaultController state machine;
- running, isolated, reconnecting, recovered, or failed states;
- state transition table;
- consecutive-failure counters;
- timeout, recovery, or persistence thresholds;
- runtime isolation execution;
- runtime reconnect execution;
- runtime abort execution;
- scheduler process control;
- scheduler lifecycle control;
- skipped exchange scheduling;
- reordered request/arbitration/replay/audit behavior;
- mutation of coupling state from fault consumption;
- native SWMM process, handle, or C API calls;
- native D-Flow FM / BMI process, handle, or API calls;
- protocol release-gate actions such as `BLOCK_MERGE` or `BLOCK_RELEASE`;
- runtime evidence writing;
- adapter-owned fault policy.

## Acceptance Criteria For Future Implementation

A future implementation is acceptable only if:

- the new helper is side-effect free;
- focused tests prove both nominal and review-required consumption paths;
- ordering tests prove exchange scheduling, replay, and audit remain unchanged;
- full local Debug CTest passes;
- evidence documents explicitly state that the stage is not executable fault handling;
- CI evidence remains pending unless an actual pushed CI run is recorded.

## Decision

Passive action consumption should be designed and implemented as a reporting seam before any scheduler control work. Real isolation, reconnect, abort, state-machine, temporal-policy, and adapter-health integration work must remain separate future milestones with explicit design and evidence.

# M117 Executable FaultController State Machine Requirements Design

## Scope

M117 is a design-only requirements document for a future executable FaultController state machine.

This document does not add code, tests, scheduler control, runtime isolation, reconnect, abort, real adapter integration, release-gate behavior, CI evidence, GoldenSuite evidence, or merge/release readiness claims.

## Motivation

M107-M116 established a passive chain from engine reports through diagnostics, policy proposal, scheduler observation, and passive consumption. Before implementing executable behavior, the project needs explicit state-machine requirements and negative boundaries so scheduler control is not introduced accidentally.

Current passive chain:

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

A future executable stage must be designed separately from this passive chain.

## Proposed Future State Machine Concepts

A future executable FaultController may define states such as:

- `running`
- `review_required`
- `isolating`
- `isolated`
- `reconnecting`
- `recovered`
- `failed`
- `aborted`

These names are design candidates only. They are not implemented by M117.

## Required Inputs For Future Execution

Before execution can be implemented, the future design must define inputs for:

- current passive consumption record;
- current engine health aggregate;
- previous FaultController state;
- consecutive unhealthy count;
- consecutive healthy count after a fault;
- elapsed time since fault detection;
- retry count;
- configured isolation threshold;
- configured reconnect threshold;
- configured abort threshold;
- operator override or manual review state, if applicable.

## Required Outputs For Future Execution

Before execution can be implemented, the future design must define output DTOs for:

- next FaultController state;
- requested isolation action;
- requested reconnect action;
- requested abort action;
- requested operator review action;
- reason code;
- preserved diagnostic and health context;
- audit event fields;
- explicit non-execution markers when action is only proposed.

## Required Transition Table

A future implementation must include a transition table that covers at least:

1. `running` + nominal diagnostic -> remain `running`.
2. `running` + transient fault below threshold -> `review_required` or remain `running` with passive warning, depending on policy.
3. `running` + persistent fault above isolation threshold -> request isolation state, if isolation is supported.
4. isolated state + healthy recovery evidence below reconnect threshold -> remain isolated or review-required.
5. isolated state + healthy recovery evidence above reconnect threshold -> request reconnect state, if reconnect is supported.
6. reconnecting state + successful recovery -> `recovered` or `running`.
7. reconnecting state + repeated failure above abort threshold -> request abort state, if abort is supported.
8. any state + invalid transition input -> review-required or failed-safe state, with explicit audit evidence.

The table must explicitly state whether each transition is passive, proposed, or executable.

## Required Negative Boundaries

A future state-machine implementation must not implicitly add:

- adapter-owned fault policy;
- native SWMM C API calls without a dedicated adapter boundary;
- native D-Flow FM / BMI calls without a dedicated adapter boundary;
- scheduler process control without scheduler-specific action-consumption tests;
- runtime abort without audit and operator evidence requirements;
- release-gate behavior such as `BLOCK_MERGE` or `BLOCK_RELEASE` without stability-protocol evidence;
- GoldenSuite release/merge claims without CI and required gate evidence.

## Required Test Categories For Future Implementation

Future tests must cover:

1. Nominal state remains non-executing.
2. Single transient fault does not execute isolation, reconnect, or abort unless explicitly configured.
3. Persistent fault crosses a defined threshold and produces a proposed or executable transition as designed.
4. Recovery evidence crosses a defined reconnect threshold before reconnect can be requested.
5. Abort threshold requires explicit evidence and audit fields.
6. Invalid or contradictory inputs fail safe and preserve diagnostic context.
7. Scheduler exchange scheduling remains unchanged until an explicit scheduler action boundary is implemented.
8. Replay and mass audit remain deterministic across passive and executable decision records.
9. Native adapter integration remains absent unless a dedicated adapter milestone implements it.

## Evidence Requirements For Future Implementation

A future implementation must include:

- focused build/test evidence for each state-machine target;
- ordering evidence showing how decisions relate to scheduler phases;
- audit evidence for any action that can affect runtime behavior;
- negative-boundary evidence showing no accidental real adapter calls;
- full local Debug CTest evidence;
- CI evidence only after a real pushed run exists;
- no GoldenSuite merge/release claims unless required GoldenSuite gate evidence exists.

## Decision

M117 defines requirements only. The next safe step is still non-executing: either refine the state-machine transition table as a design artifact or implement a passive state-classification helper that labels future states without controlling scheduler behavior.

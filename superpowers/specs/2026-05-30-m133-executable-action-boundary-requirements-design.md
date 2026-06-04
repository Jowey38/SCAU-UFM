# M133 Executable Action Boundary Requirements Design

## Scope

M133 defines design-only requirements for future executable FaultController action boundaries.

This document does not add code, tests, scheduler control, runtime isolation, reconnect, abort, real adapter integration, release-gate behavior, CI evidence, GoldenSuite evidence, or merge/release readiness claims.

## Motivation

M129-M132 established passive action-outcome records and ordering evidence without controlling the scheduler. Before any executable action boundary can be implemented, the project needs explicit requirements for how future action boundaries are separated from passive DTO creation, scheduler phases, adapter calls, replay, rollback, and mass audit.

Current passive chain:

```text
EngineReport
→ classify_engine_health(...)
→ aggregate_engine_health(...)
→ make_fault_controller_diagnostic(...)
→ propose_fault_controller_action(...)
→ observe_mock_scheduler_fault_action(...)
→ consume_mock_scheduler_fault_action(...)
→ classify_fault_controller_passive_state(...)
→ make_fault_controller_passive_transition(...)
→ make_fault_controller_passive_action_audit_record(...)
→ make_fault_controller_passive_action_outcome(...)
→ run_mock_coupling_scheduler_step(...)
```

Future executable action boundaries must remain separate from this passive chain until explicitly implemented and tested.

## Design Principles

A future executable action boundary must:

1. Consume a fully preserved passive action outcome record.
2. Require an explicit execution capability flag or boundary object.
3. Require complete transition, threshold, and audit evidence.
4. Separate scheduler control from adapter control.
5. Separate action requests from action execution.
6. Preserve pre-action state and post-action state evidence.
7. Define rollback, replay, and mass-audit consequences before execution.
8. Produce machine-readable outcome and audit records.
9. Fail closed when required evidence or boundaries are absent.
10. Avoid release-gate claims unless CI and GoldenSuite evidence are available.

## Candidate Boundary Types

Future executable work should be split into separate boundaries:

| Boundary | Responsibility | Must not own |
|---|---|---|
| Action request boundary | Converts transition/outcome records into requested actions. | Runtime execution |
| Scheduler action boundary | Controls scheduler-level behavior such as pause/skip/order only if explicitly allowed. | Native adapter calls |
| Adapter action boundary | Calls adapter-specific isolate/reconnect/abort hooks only through adapter APIs. | Coupling arbitration |
| Audit emission boundary | Persists or emits runtime action evidence. | Scheduler or adapter control |
| Recovery boundary | Handles rollback/replay/mass-audit requirements after execution. | Initial fault policy |

M133 does not implement any of these boundaries.

## Required Inputs For Future Action Boundaries

A future executable action boundary must define input DTOs containing at least:

- preserved `FaultControllerPassiveActionOutcome`;
- requested action kind;
- current passive/executable state;
- target engine identifier;
- target component or endpoint identifier, if applicable;
- scheduler phase;
- adapter boundary availability;
- scheduler control boundary availability;
- threshold evidence completeness;
- pre-action snapshot or equivalent rollback context;
- replay policy;
- mass-audit policy;
- operator approval or override, if applicable;
- maximum retry/reconnect policy, if applicable.

## Required Outputs For Future Action Boundaries

A future executable action boundary must return an output DTO containing at least:

- preserved input action outcome;
- attempted action kind;
- executed action kind;
- action result status;
- result reason code;
- target engine/component metadata;
- scheduler phase before action;
- scheduler phase after action;
- whether scheduler control was used;
- whether adapter call was attempted;
- whether adapter call succeeded;
- whether adapter call failed;
- rollback requirement;
- replay requirement;
- mass-audit requirement;
- post-action health/audit summary;
- non-execution reason when execution did not occur.

## Candidate Action Result Statuses

A future implementation should define stable statuses such as:

- `not_requested`;
- `blocked_boundary_absent`;
- `blocked_missing_evidence`;
- `blocked_operator_review`;
- `requested_only`;
- `attempted`;
- `succeeded`;
- `failed`;
- `rolled_back`;
- `requires_replay`;
- `requires_mass_audit`.

Statuses must not imply execution unless execution fields also confirm it.

## Required Boundary Rules

A future executable boundary must obey these rules:

1. Passive action outcomes never execute runtime behavior by themselves.
2. Operator-review outcomes require explicit operator or policy approval before any requested executable action.
3. Isolation execution requires scheduler control boundary and adapter/action boundary availability.
4. Reconnect execution requires prior isolation/recovery evidence and adapter/action boundary availability.
5. Abort execution requires explicit abort policy evidence and audit requirements.
6. Missing threshold evidence blocks execution.
7. Missing pre-action rollback context blocks mutating runtime execution.
8. Any action that mutates runtime state requires replay and mass-audit evidence.
9. Adapter calls must only occur through dedicated adapter boundaries.
10. Release-gate actions must remain separate from runtime action execution.

## Scheduler Control Requirements

Before any scheduler-control implementation, future work must define:

- whether scheduler control can pause exchange scheduling;
- whether scheduler control can skip an engine request;
- whether scheduler control can reorder health collection relative to exchange phases;
- whether scheduler control can block replay;
- whether scheduler control can force a mass audit;
- how each scheduler change is audited;
- how scheduler changes interact with rollback/replay.

Until those requirements are implemented and tested, all passive records must leave scheduling, replay, and audit unchanged.

## Adapter Boundary Requirements

Before any adapter action implementation, future work must define:

- adapter-owned action methods, if any;
- target identifiers accepted by adapter actions;
- precondition checks;
- idempotency guarantees;
- failure modes;
- timeout behavior;
- recovery behavior;
- audit fields;
- native API boundaries;
- absence of cross-engine native object sharing.

SWMM and D-Flow FM actions must remain separate. Their native objects must not see each other.

## Required Negative Tests For Future Implementation

A future executable boundary implementation must include tests proving:

1. Passive outcome records do not execute scheduler control.
2. Requested isolation is blocked when scheduler control boundary is absent.
3. Requested isolation is blocked when adapter boundary is absent.
4. Requested reconnect is blocked without recovery evidence.
5. Requested abort is blocked without abort policy evidence.
6. Missing rollback context blocks mutating execution.
7. Missing threshold evidence blocks execution.
8. Adapter calls are not made by core DTO helpers.
9. Scheduler ordering is unchanged until explicit scheduler-control tests enable changes.
10. Release-gate actions are not emitted by runtime action boundaries.

## Required Ordering Tests For Future Implementation

A future implementation must prove any executable boundary interaction with scheduler phases. Required comparisons include:

- baseline mock scheduling versus action-request-only path;
- baseline mock scheduling versus blocked-action path;
- baseline replay versus blocked-action replay;
- baseline mass audit versus blocked-action mass audit;
- executable action path with explicit scheduler control enabled, if implemented;
- failure path with rollback/replay evidence, if implemented.

## Required Evidence For Future Implementation

A future executable boundary implementation must include:

- focused unit tests for every action boundary;
- focused negative tests for absent boundaries and missing evidence;
- ordering tests against mock scheduling, replay, and audit;
- audit evidence for requested, attempted, succeeded, failed, blocked, and skipped actions;
- full local Debug CTest evidence;
- CI evidence only after a pushed CI run exists;
- no GoldenSuite release/merge claims without required gate evidence.

## Confirmed Negative Boundary For M133

M133 does not add:

- action boundary code;
- scheduler control code;
- adapter action code;
- executable FaultController state machine;
- transition-table evaluator;
- threshold counters;
- runtime isolation execution;
- runtime reconnect execution;
- runtime abort execution;
- skipped exchange scheduling;
- reordered request/arbitration/replay/audit behavior;
- mutation of coupling state;
- real SWMM C API integration;
- real D-Flow FM / BMI integration;
- protocol release-gate actions such as `BLOCK_MERGE` or `BLOCK_RELEASE`;
- runtime evidence writing;
- adapter-owned fault policy.

## Decision

M133 defines the minimum executable action boundary requirements needed before scheduler control or adapter action work can begin. The next safe step is still non-executing: implement a blocked-action DTO skeleton that records why execution is not allowed, or roll up M129-M133, without scheduler control.

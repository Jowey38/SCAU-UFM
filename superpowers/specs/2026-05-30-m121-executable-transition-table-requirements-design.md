# M121 Executable Transition Table Requirements Design

## Scope

M121 defines design-only requirements for a future executable FaultController transition table.

This document does not add code, tests, scheduler control, runtime isolation, reconnect, abort, real adapter integration, release-gate behavior, CI evidence, GoldenSuite evidence, or merge/release readiness claims.

## Motivation

M117-M120 established requirements and passive state-label evidence for future FaultController state concepts without controlling the scheduler. Before any executable implementation, the project needs a transition-table design that distinguishes passive labels, proposed actions, and executable actions.

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
→ run_mock_coupling_scheduler_step(...)
```

A future executable transition table must remain separate from this passive chain until explicitly implemented and tested.

## Design Principles

A future transition table must:

1. Be data-driven and auditable.
2. Preserve the input diagnostic, proposed action, observation, consumption, and passive state label.
3. Separate state classification from scheduler control.
4. Separate proposed actions from executable actions.
5. Require explicit thresholds for temporal or persistent fault decisions.
6. Require explicit audit records before any action can alter runtime behavior.
7. Keep real adapter integration behind dedicated adapter boundaries.
8. Avoid release-gate claims unless CI and GoldenSuite evidence are available.

## Candidate State Set

A future executable state machine may use the following candidate states:

| State | Meaning | Executable in this design? |
|---|---|---|
| `running` | Normal operation label. | No |
| `review_required` | Fault evidence requires operator or policy review. | No |
| `isolation_requested` | Policy has requested isolation but has not executed it. | No |
| `isolating` | Isolation is being executed by an explicit future control boundary. | Future only |
| `isolated` | A component is isolated after successful future action execution. | Future only |
| `reconnect_requested` | Policy has requested reconnect but has not executed it. | No |
| `reconnecting` | Reconnect is being executed by an explicit future control boundary. | Future only |
| `recovered` | Recovery evidence has been accepted. | Future only |
| `abort_requested` | Policy has requested abort but has not executed it. | No |
| `aborted` | Abort has been executed by an explicit future control boundary. | Future only |
| `failed_safe` | Invalid or contradictory input forced a conservative state. | Future only |

These names are requirements candidates only. M121 does not add them to code.

## Required Transition Inputs

A future transition-table evaluator must define an input DTO containing at least:

- current state;
- passive state classification;
- latest `FaultControllerDiagnostic`;
- latest `FaultControllerProposedAction`;
- latest scheduler observation record;
- latest scheduler consumption record;
- engine health aggregate;
- report count;
- unhealthy count;
- consecutive unhealthy step count;
- consecutive healthy recovery step count;
- elapsed time since first fault detection;
- elapsed time since last healthy report;
- isolation threshold;
- reconnect threshold;
- abort threshold;
- maximum reconnect attempts;
- current reconnect attempt count;
- operator review flag or manual override state, if applicable.

## Required Transition Outputs

A future transition-table evaluator must return an output DTO containing at least:

- previous state;
- next state;
- transition reason code;
- preserved input diagnostic context;
- preserved health aggregate;
- whether isolation is requested;
- whether reconnect is requested;
- whether abort is requested;
- whether operator review is requested;
- whether any runtime action was executed;
- audit fields required before any future runtime action is allowed;
- explicit flags showing scheduler control was or was not enabled.

## Required Reason Codes

A future transition-table design must define reason codes such as:

- `nominal_health`;
- `fault_detected_below_threshold`;
- `fault_detected_review_required`;
- `persistent_fault_isolation_threshold_met`;
- `recovery_below_reconnect_threshold`;
- `recovery_reconnect_threshold_met`;
- `reconnect_attempt_failed`;
- `abort_threshold_met`;
- `operator_review_requested`;
- `invalid_transition_input`;
- `contradictory_action_flags`;
- `missing_required_evidence`.

Reason codes must be machine-facing and stable once implemented.

## Required Transition Table

A future transition table must cover at least the following rows:

| Current state | Input condition | Required next state | Action class | Runtime execution allowed? |
|---|---|---|---|---|
| `running` | nominal health | `running` | passive | No |
| `running` | single fault below threshold | `review_required` or `running` per policy | passive/proposed | No |
| `running` | persistent fault meets isolation threshold | `isolation_requested` | proposed | No |
| `review_required` | nominal recovery below reconnect threshold | `review_required` | passive | No |
| `review_required` | operator accepts continue | `running` | proposed | No |
| `isolation_requested` | execution boundary absent | `isolation_requested` | proposed | No |
| `isolation_requested` | future isolation execution succeeds | `isolated` | executable future only | Future only |
| `isolated` | recovery below reconnect threshold | `isolated` | passive | No |
| `isolated` | recovery meets reconnect threshold | `reconnect_requested` | proposed | No |
| `reconnect_requested` | execution boundary absent | `reconnect_requested` | proposed | No |
| `reconnect_requested` | future reconnect execution succeeds | `recovered` or `running` | executable future only | Future only |
| `reconnecting` | reconnect attempt fails below abort threshold | `review_required` or `isolated` per policy | proposed | No |
| `reconnecting` | reconnect failures meet abort threshold | `abort_requested` | proposed | No |
| `abort_requested` | execution boundary absent | `abort_requested` | proposed | No |
| `abort_requested` | future abort execution succeeds | `aborted` | executable future only | Future only |
| any | invalid or contradictory input | `failed_safe` or `review_required` per policy | passive/proposed | No |

Rows marked “future only” must not be implemented as runtime execution until dedicated scheduler/action boundaries and audit evidence exist.

## Scheduler-Control Boundary Requirements

A future executable transition table must not directly call scheduler control. Instead, it must emit a proposed transition/action record. Any scheduler control must be implemented later through a separate boundary with its own tests for:

- action consumption;
- scheduler phase ordering;
- exchange scheduling impact;
- replay impact;
- mass audit impact;
- action audit records;
- rollback/replay behavior;
- operator review evidence.

## Required Negative Tests For Future Implementation

Before executable behavior is allowed, future tests must prove:

1. Passive classification never triggers scheduler control by itself.
2. `review_required` never executes isolation, reconnect, or abort by itself.
3. Proposed `isolation_requested` does not execute isolation when no execution boundary exists.
4. Proposed `reconnect_requested` does not execute reconnect when no execution boundary exists.
5. Proposed `abort_requested` does not execute abort when no execution boundary exists.
6. Invalid thresholds fail safe without scheduler mutation.
7. Contradictory flags preserve diagnostics and do not call adapters.
8. Real SWMM and D-Flow FM APIs remain absent unless a dedicated adapter milestone adds them.

## Evidence Requirements For Future Implementation

Any future transition-table implementation must include:

- focused unit tests for every transition row;
- focused negative tests for non-execution boundaries;
- ordering tests against mock scheduling, replay, and audit;
- full local Debug CTest evidence;
- CI evidence only after a pushed CI run exists;
- no GoldenSuite release/merge claims without required gate evidence.

## Confirmed Negative Boundary For M121

M121 does not add:

- executable FaultController state machine;
- transition-table code;
- threshold counters;
- runtime isolation execution;
- runtime reconnect execution;
- runtime abort execution;
- scheduler process control;
- skipped exchange scheduling;
- reordered request/arbitration/replay/audit behavior;
- mutation of coupling state;
- real SWMM C API integration;
- real D-Flow FM / BMI integration;
- protocol release-gate actions such as `BLOCK_MERGE` or `BLOCK_RELEASE`;
- runtime evidence writing;
- adapter-owned fault policy.

## Decision

M121 defines the minimum transition-table requirements needed before executable FaultController work. The next safe step is still non-executing: implement a passive transition-table DTO skeleton or add a design-only audit schema for future executable actions, without scheduler control.

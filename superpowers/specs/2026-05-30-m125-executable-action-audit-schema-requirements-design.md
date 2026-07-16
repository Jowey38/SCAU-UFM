# M125 Executable Action Audit Schema Requirements Design

## Scope

M125 defines design-only requirements for a future executable FaultController action audit schema.

This document does not add code, tests, scheduler control, runtime isolation, reconnect, abort, real adapter integration, release-gate behavior, CI evidence, GoldenSuite evidence, or merge/release readiness claims.

## Motivation

M121-M124 established passive transition records and ordering evidence without controlling the scheduler. Before any executable action can be implemented, the project needs an audit schema that records what was requested, why it was requested, whether it was executed, and what runtime evidence justifies the action.

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
→ run_mock_coupling_scheduler_step(...)
```

The future audit schema must remain separate from scheduler control until an explicit execution boundary is implemented and tested.

## Design Principles

A future executable action audit schema must:

1. Preserve the full passive provenance chain.
2. Separate requested actions from executed actions.
3. Record reason codes and thresholds used for decisions.
4. Record scheduler phase ordering without allowing the audit schema to control the scheduler.
5. Record adapter/action target metadata without owning native adapter calls.
6. Make non-execution explicit when an action is only proposed or requested.
7. Be deterministic and machine-readable.
8. Avoid release-gate claims unless CI and GoldenSuite evidence are available.

## Proposed Audit Record Boundary

A future implementation may add a core-owned DTO similar to:

```text
FaultControllerActionAuditRecord
```

Recommended top-level fields:

- `audit_id` or deterministic sequence number;
- `engine_id` or target identifier;
- `action_kind`;
- `action_stage`;
- `previous_state`;
- `next_state`;
- `transition_reason`;
- preserved `EngineHealthAggregate`;
- preserved `FaultControllerDiagnostic`;
- preserved `FaultControllerProposedAction`;
- preserved `MockCouplingSchedulerFaultObservation`;
- preserved `MockCouplingSchedulerFaultConsumption`;
- preserved `FaultControllerPassiveStateClassification`;
- preserved `FaultControllerPassiveTransition`;
- threshold evidence fields;
- scheduler phase evidence fields;
- execution result fields;
- non-execution flags;
- error/result code;
- human-readable note, if needed.

## Candidate Action Kinds

A future schema should define stable machine-facing action kinds such as:

- `none`;
- `operator_review`;
- `isolation_request`;
- `isolation_execute`;
- `reconnect_request`;
- `reconnect_execute`;
- `abort_request`;
- `abort_execute`.

Action kinds ending in `_execute` must not be emitted until an explicit execution boundary exists.

## Candidate Action Stages

A future schema should distinguish stages such as:

- `proposed`;
- `observed`;
- `consumed`;
- `classified`;
- `transition_recorded`;
- `requested`;
- `executed`;
- `failed`;
- `skipped`.

Passive stages must not be treated as runtime execution.

## Threshold Evidence Fields

Before executable behavior is allowed, audit records must preserve threshold evidence such as:

- consecutive unhealthy step count;
- consecutive healthy recovery step count;
- elapsed time since first fault detection;
- elapsed time since last healthy report;
- configured isolation threshold;
- configured reconnect threshold;
- configured abort threshold;
- current reconnect attempt count;
- maximum reconnect attempts;
- whether threshold evidence is complete;
- whether threshold evidence is stale or missing.

Missing or stale threshold evidence must prevent executable action and record a non-execution reason.

## Scheduler Phase Evidence Fields

A future audit schema must record scheduler phase context without controlling it. Candidate fields:

- `phase_before_observation`;
- `phase_before_consumption`;
- `phase_before_classification`;
- `phase_before_transition`;
- `phase_before_request`;
- `phase_before_execution`;
- `phase_after_execution`;
- whether exchange scheduling was allowed;
- whether replay was allowed;
- whether audit was allowed;
- whether the action changed request/arbitration/replay/audit ordering.

For passive stages, the ordering-change field must remain false.

## Execution Result Fields

If a future execution boundary exists, audit records must capture:

- whether runtime action was attempted;
- whether runtime action succeeded;
- whether runtime action failed;
- failure reason code;
- target adapter/component identifier;
- target endpoint identifier, if any;
- pre-action state summary;
- post-action state summary;
- rollback/replay impact summary;
- mass-audit impact summary;
- operator review requirement.

Without an execution boundary, these fields must record non-execution explicitly.

## Required Non-Execution Flags

The audit schema must include explicit flags such as:

- `scheduler_control_enabled`;
- `runtime_action_requested`;
- `runtime_action_executed`;
- `isolation_executed`;
- `reconnect_executed`;
- `abort_executed`;
- `adapter_call_executed`;
- `release_gate_action_executed`.

For passive stages, all execution flags must remain false.

## Required Reason Codes

A future audit schema must define stable reason codes such as:

- `nominal_no_action`;
- `review_required_only`;
- `requested_not_executed_boundary_absent`;
- `threshold_evidence_missing`;
- `threshold_evidence_incomplete`;
- `threshold_not_met`;
- `isolation_threshold_met`;
- `reconnect_threshold_met`;
- `abort_threshold_met`;
- `operator_review_required`;
- `invalid_transition_input`;
- `contradictory_action_flags`;
- `scheduler_control_boundary_absent`;
- `adapter_boundary_absent`;
- `execution_failed`.

Reason codes must not imply action execution unless execution fields also confirm it.

## Required Negative Boundaries

A future audit schema implementation must not implicitly add:

- executable FaultController state machine;
- scheduler control;
- runtime isolation execution;
- runtime reconnect execution;
- runtime abort execution;
- adapter native calls;
- release-gate behavior;
- runtime evidence writing outside the audit DTO;
- GoldenSuite merge/release claims.

Audit records describe or preserve evidence. They must not perform control actions.

## Required Tests For Future Implementation

A future implementation must include tests proving:

1. Passive no-action audits preserve nominal provenance and execute nothing.
2. Review-required audits preserve fault provenance and execute nothing.
3. Requested isolation audit records do not execute isolation when execution boundary is absent.
4. Requested reconnect audit records do not execute reconnect when execution boundary is absent.
5. Requested abort audit records do not execute abort when execution boundary is absent.
6. Missing threshold evidence produces a non-execution reason.
7. Contradictory action flags produce a non-execution or review-required reason.
8. Audit creation before mock scheduling does not alter scheduling, replay, or mass audit results.
9. Real SWMM and D-Flow FM APIs remain absent unless a dedicated adapter milestone adds them.

## Evidence Requirements For Future Implementation

A future audit-schema implementation must include:

- focused unit tests for all passive and requested-action audit rows;
- focused negative tests for non-execution boundaries;
- ordering tests against mock scheduling, replay, and audit;
- full local Debug CTest evidence;
- CI evidence only after a pushed CI run exists;
- no GoldenSuite release/merge claims without required gate evidence.

## Confirmed Negative Boundary For M125

M125 does not add:

- audit-schema code;
- executable FaultController state machine;
- transition-table evaluator;
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

M125 defines the minimum audit schema requirements needed before executable FaultController actions can be implemented. The next safe step is still non-executing: implement a passive action-audit DTO skeleton or roll up M121-M125, without scheduler control.

# M137 Executable Scheduler-Control Requirements Design

## Scope

M137 defines design-only requirements for a future executable scheduler-control boundary inside the FaultController path.

This document does not add code, tests, runtime scheduler control, runtime isolation, reconnect, abort, real adapter integration, release-gate behavior, CI evidence, GoldenSuite evidence, or merge/release readiness claims.

## Motivation

M133-M136 established that all current FaultController records remain passive: proposal, observation, consumption, state classification, transition, audit, outcome, and blocked-action records all preserve information while leaving exchange scheduling, replay, and audit unchanged.

Before any future implementation is allowed to alter scheduling behavior, the project needs an explicit scheduler-control design that separates:

- passive policy records;
- scheduler-control requests;
- scheduler-control execution;
- adapter execution;
- rollback/replay/mass-audit consequences.

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
→ make_fault_controller_blocked_action(...)
→ run_mock_coupling_scheduler_step(...)
```

Any future scheduler-control implementation must be added as a new explicit boundary after these passive records, never implied by them.

## Design Principles

A future scheduler-control boundary must:

1. Consume explicit scheduler-control requests, not passive records directly.
2. Define exactly which scheduler phases can be affected.
3. Preserve exchange ordering evidence before and after control.
4. Separate scheduler control from adapter calls and engine-native actions.
5. Preserve rollback/replay/mass-audit requirements before any control takes effect.
6. Default to no-op when required evidence, approval, or boundary support is absent.
7. Emit machine-readable control outcomes and audit evidence.
8. Remain same-process and project-owned until a later boundary explicitly changes that assumption.
9. Avoid release-gate behavior unless the stability protocol and gate evidence are explicitly implemented.
10. Never mutate coupling conservation semantics silently.

## Candidate Scheduler-Control Request DTO

A future implementation may add a DTO similar to:

```text
FaultControllerSchedulerControlRequest
```

Recommended fields:

- preserved `FaultControllerPassiveActionOutcome`;
- preserved `FaultControllerBlockedAction` when execution is not allowed;
- requested scheduler-control kind;
- scheduler phase at request time;
- target engine identifier;
- requested ordering effect;
- requested replay effect;
- requested audit effect;
- operator approval state, if applicable;
- threshold evidence completeness;
- rollback context available;
- mass-audit context available.

## Candidate Scheduler-Control Kinds

A future implementation should define stable scheduler-control kinds such as:

- `none`;
- `observe_only`;
- `pause_exchange_requests`;
- `skip_target_engine_request`;
- `hold_replay_until_review`;
- `force_mass_audit_before_continue`;
- `abort_scheduler_step`.

Only `none` and `observe_only` are consistent with the current passive stage. All other kinds are future-only and must not be implemented by M137.

## Scheduler Phases That Must Be Named Explicitly

Before any scheduler-control implementation, future work must define exact named phases, such as:

- health collection;
- diagnostic derivation;
- proposed action derivation;
- observation;
- consumption;
- passive state classification;
- passive transition creation;
- passive audit creation;
- passive outcome creation;
- blocked-action derivation;
- exchange request preparation;
- shared-cell arbitration;
- exchange acceptance;
- replay;
- mass audit;
- post-step reporting.

The scheduler-control boundary must state which of these phases can be delayed, skipped, forced, or blocked.

## Required Preconditions For Future Scheduler Control

Any future executable scheduler-control request must require at least:

- explicit scheduler-control boundary availability;
- complete passive provenance chain;
- explicit requested control kind;
- threshold evidence completeness;
- operator approval when required by policy;
- rollback context when control could affect state progression;
- replay policy when control could delay replay;
- mass-audit policy when control could affect conservation verification;
- a non-contradictory action/outcome combination.

If any precondition is absent, the boundary must return a blocked/no-op result and preserve the reason.

## Required Output DTO For Future Scheduler Control

A future implementation may add a DTO similar to:

```text
FaultControllerSchedulerControlResult
```

Recommended fields:

- preserved request DTO;
- control kind attempted;
- control status;
- result reason code;
- phase before control;
- phase after control;
- whether scheduler control was actually used;
- whether exchange requests were paused;
- whether a target engine request was skipped;
- whether replay was held;
- whether mass audit was forced;
- whether scheduler state was mutated;
- rollback required;
- replay required;
- mass-audit required;
- operator review required;
- release-gate action executed.

## Candidate Control Statuses

A future implementation should define stable statuses such as:

- `not_requested`;
- `blocked_boundary_absent`;
- `blocked_missing_evidence`;
- `blocked_operator_review`;
- `requested_only`;
- `executed_noop`;
- `executed_pause`;
- `executed_skip`;
- `executed_hold_replay`;
- `executed_force_mass_audit`;
- `failed`.

Statuses must not imply adapter or runtime action execution unless those fields explicitly confirm it.

## Required Result Reason Codes

A future scheduler-control boundary must define machine-facing reasons such as:

- `no_scheduler_control_requested`;
- `observe_only_stage`;
- `scheduler_control_boundary_absent`;
- `threshold_evidence_missing`;
- `operator_review_pending`;
- `rollback_context_missing`;
- `replay_policy_missing`;
- `mass_audit_policy_missing`;
- `target_engine_not_schedulable`;
- `scheduler_control_executed`;
- `scheduler_control_failed`;
- `contradictory_action_flags`.

## Required Negative Boundaries

A future scheduler-control implementation must not implicitly add:

- adapter isolate/reconnect/abort calls;
- engine-native object sharing across drainage and river engines;
- release-gate behavior such as `BLOCK_MERGE` or `BLOCK_RELEASE`;
- runtime evidence writing beyond the explicit result/audit DTOs;
- silent replay skipping;
- silent conservation bypass;
- scheduler-control execution from passive records alone.

Passive records must remain descriptive; only explicit control requests may reach the scheduler-control boundary.

## Required Negative Tests For Future Implementation

A future implementation must prove:

1. Passive action outcomes do not execute scheduler control by themselves.
2. Blocked-action records do not execute scheduler control by themselves.
3. Requested scheduler control is blocked when the scheduler-control boundary is absent.
4. Requested scheduler control is blocked when threshold evidence is missing.
5. Requested scheduler control is blocked when operator review is required but not approved.
6. Requested scheduler control is blocked when rollback context is missing.
7. Requested scheduler control does not call adapter APIs.
8. Requested scheduler control does not change replay or mass-audit behavior unless explicitly allowed and audited.
9. Contradictory action/control flags fail closed.
10. Release-gate actions are not emitted by scheduler-control results.

## Required Ordering Tests For Future Implementation

A future scheduler-control implementation must include ordering tests comparing:

- baseline mock scheduling versus observe-only control;
- baseline mock scheduling versus blocked control request;
- baseline replay versus blocked replay-hold request;
- baseline mass audit versus blocked force-audit request;
- explicit scheduler-control execution path versus baseline, if implemented;
- failure path with rollback/replay evidence, if implemented.

Each ordering test must explicitly state which phases changed and which remained unchanged.

## Required Evidence For Future Implementation

A future scheduler-control implementation must include:

- focused unit tests for each scheduler-control kind;
- focused negative tests for absent boundaries and missing evidence;
- ordering tests against request/arbitration/replay/audit phases;
- action-audit and outcome evidence for requested, blocked, no-op, and executed control paths;
- full local Debug CTest evidence;
- CI evidence only after a pushed CI run exists;
- no GoldenSuite release/merge claims without required gate evidence.

## Confirmed Negative Boundary For M137

M137 does not add:

- scheduler-control request DTO code;
- scheduler-control result DTO code;
- scheduler-control execution;
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

M137 defines the minimum scheduler-control requirements needed before any executable scheduler-control work can begin. The next safe step is still non-executing: implement a passive scheduler-control request DTO skeleton that records why control is unavailable, or roll up M133-M137, without scheduler control execution.

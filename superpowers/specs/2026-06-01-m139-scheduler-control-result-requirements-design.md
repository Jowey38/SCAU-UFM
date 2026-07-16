# M139 Scheduler-Control Result Requirements Design

## Scope

M139 defines design-only requirements for a future scheduler-control result boundary after the passive scheduler-control request skeleton introduced in M138.

This document does not add code, tests, executable scheduler control, scheduler phase mutation, skipped exchange scheduling, replay holds, forced mass audit, adapter calls, isolation/reconnect/abort execution, runtime evidence writers, CI evidence, GoldenSuite evidence, or release/merge readiness claims.

## Motivation

M137 established requirements for future executable scheduler-control work. M138 added a passive request skeleton that records when scheduler control is absent and fails closed before any runtime action can occur.

Before any scheduler-control execution is implemented, the project needs a result-side contract that separates:

- passive scheduler-control requests;
- blocked or no-op scheduler-control results;
- future executed scheduler-control results;
- adapter execution results;
- replay and mass-audit consequences;
- release-gate protocol actions.

The result boundary must make absence of execution explicit and auditable instead of letting passive request records imply runtime behavior.

## Current Passive Chain

Current chain through M138:

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
→ make_fault_controller_scheduler_control_request(...)
→ run_mock_coupling_scheduler_step(...)
```

Any future result DTO must remain passive unless a separate executable scheduler-control boundary is explicitly implemented.

## Candidate Result DTO

A future implementation may add a DTO similar to:

```text
FaultControllerSchedulerControlResult
```

Recommended preserved fields:

- preserved `FaultControllerSchedulerControlRequest`;
- preserved `FaultControllerBlockedAction` through the request;
- preserved `FaultControllerPassiveActionOutcome` through the request;
- control kind attempted;
- control status;
- result reason code;
- phase before control;
- phase after control;
- target engine identifier;
- scheduler-control boundary availability;
- threshold evidence completeness;
- operator approval state;
- rollback context availability;
- replay policy availability;
- mass-audit policy availability.

Recommended effect fields:

- scheduler control used;
- exchange requests paused;
- target engine request skipped;
- replay held;
- mass audit forced;
- scheduler state mutated;
- adapter call attempted;
- adapter call succeeded;
- adapter call failed;
- rollback required;
- replay required;
- mass audit required;
- operator review required;
- release-gate action executed.

## Required Passive Result Statuses

A future passive result implementation must at least support:

- `not_requested` — no scheduler control was requested;
- `blocked_boundary_absent` — scheduler control was requested but no executable boundary exists;
- `blocked_missing_evidence` — required threshold, rollback, replay, or mass-audit evidence is missing;
- `blocked_operator_review` — policy requires operator approval that is not present;
- `executed_noop` — future-only status for an explicit no-op execution boundary;
- `failed` — future-only status for an explicit failed execution boundary.

Only `not_requested` and `blocked_boundary_absent` are consistent with the current M138 passive skeleton.

## Required Result Reason Codes

A future passive result implementation must preserve machine-facing reason codes such as:

- `no_scheduler_control_requested`;
- `scheduler_control_boundary_absent`;
- `threshold_evidence_missing`;
- `operator_review_pending`;
- `rollback_context_missing`;
- `replay_policy_missing`;
- `mass_audit_policy_missing`;
- `target_engine_not_schedulable`;
- `contradictory_action_flags`;
- `scheduler_control_executed`;
- `scheduler_control_failed`.

The current passive stage may only emit no-requested or blocked-boundary-absent reasons unless a later milestone explicitly implements more validation.

## Required Fail-Closed Mapping

A future passive result helper should map M138 requests as follows:

1. Request `requested_kind = none`:
   - result status `not_requested`;
   - reason `no_scheduler_control_requested`;
   - all effect and execution flags false.

2. Request `requested_kind = observe_only` with absent boundary:
   - result status `blocked_boundary_absent`;
   - reason `scheduler_control_boundary_absent`;
   - all effect and execution flags false.

3. Request with missing evidence or contradictory flags:
   - blocked status and explicit reason;
   - preserved input provenance;
   - all scheduler mutation, adapter, replay-hold, mass-audit-forcing, and release-gate flags false.

4. Any future executable status:
   - must require a separate implementation milestone;
   - must include focused unit tests and ordering tests before it affects scheduling behavior;
   - must not be inferred from passive request, blocked action, outcome, audit, transition, classification, consumption, observation, or proposal records alone.

## Required Negative Boundaries

A future scheduler-control result implementation must not implicitly add:

- scheduler phase mutation;
- skipped exchange request preparation;
- altered shared-cell arbitration;
- replay skipping;
- replay holding without explicit replay policy and audit;
- mass-audit forcing without explicit mass-audit policy and audit;
- adapter isolate/reconnect/abort calls;
- native SWMM C API calls;
- native D-Flow FM / BMI calls;
- drainage and river native object sharing;
- runtime evidence writing beyond explicit result DTOs;
- protocol release-gate actions such as `BLOCK_MERGE` or `BLOCK_RELEASE`;
- mutation of coupling conservation semantics.

## Required Tests For Future Implementation

A future implementation must include focused tests proving:

1. A not-requested passive request produces a not-requested result.
2. An observe-only request with absent scheduler-control boundary produces a blocked-boundary-absent result.
3. Missing threshold evidence blocks execution.
4. Missing operator approval blocks execution when review is required.
5. Missing rollback context blocks any control that could affect progression.
6. Missing replay policy blocks any control that could hold replay.
7. Missing mass-audit policy blocks any control that could force or alter audit behavior.
8. Contradictory action/control flags fail closed.
9. Passive result creation does not call adapter APIs.
10. Passive result creation does not change exchange scheduling, replay, or mass audit.
11. Passive result creation does not emit release-gate actions.

## Required Ordering Evidence For Future Implementation

A future result implementation must include ordering evidence comparing:

- baseline mock scheduling versus not-requested result creation;
- baseline mock scheduling versus blocked-boundary-absent result creation;
- baseline replay versus blocked replay-hold result creation;
- baseline mass audit versus blocked force-audit result creation;
- future executable control path versus baseline, if execution is implemented.

Each ordering test must explicitly state which phases changed and which remained unchanged.

## Evidence Requirements For Future Implementation

A future implementation must include:

- focused unit tests for each passive result mapping;
- focused negative tests for missing boundary, missing evidence, missing approval, and contradictory flags;
- ordering tests against exchange request preparation, shared-cell arbitration, replay, and mass audit;
- local Debug build evidence;
- full local Debug CTest evidence;
- CI evidence only after a pushed CI run exists;
- no GoldenSuite release/merge claims without required gate evidence.

## Confirmed Negative Boundary For M139

M139 does not add:

- `FaultControllerSchedulerControlResult` code;
- result helper code;
- executable scheduler control;
- scheduler-control request changes;
- scheduler phase mutation;
- skipped exchange scheduling;
- replay holds;
- forced mass audit;
- adapter calls;
- runtime isolation execution;
- runtime reconnect execution;
- runtime abort execution;
- runtime evidence writing;
- release-gate behavior;
- CI evidence;
- GoldenSuite evidence.

## Decision

M139 defines the minimum scheduler-control result requirements needed before result DTO implementation. The next safe step is a passive scheduler-control result DTO skeleton that consumes the M138 request and records `not_requested` or `blocked_boundary_absent` without executing scheduler control or changing exchange/replay/audit behavior.

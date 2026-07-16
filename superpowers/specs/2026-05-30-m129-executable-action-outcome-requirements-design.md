# M129 Executable Action Outcome Requirements Design

## Scope

M129 defines design-only requirements for future executable FaultController action outcomes.

This document does not add code, tests, scheduler control, runtime isolation, reconnect, abort, real adapter integration, release-gate behavior, CI evidence, GoldenSuite evidence, or merge/release readiness claims.

## Motivation

M125-M128 established passive action-audit records and ordering evidence without controlling the scheduler. Before any executable action can be implemented, the project needs outcome requirements that distinguish requested actions, attempted actions, executed actions, failed actions, skipped actions, and non-executed passive records.

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
→ run_mock_coupling_scheduler_step(...)
```

Future action outcomes must remain separate from scheduler control until explicit execution boundaries, ordering evidence, and audit evidence exist.

## Design Principles

A future action-outcome schema must:

1. Preserve the passive audit record and full provenance chain.
2. Separate requested, attempted, executed, skipped, failed, and blocked outcomes.
3. Record why an outcome occurred using stable machine-facing reason codes.
4. Record whether scheduler control was available and used.
5. Record whether adapter calls were available and used.
6. Record rollback/replay and mass-audit impact for executable actions.
7. Default to non-executed outcomes when any required boundary or evidence is missing.
8. Avoid release-gate claims unless CI and GoldenSuite evidence are available.

## Proposed Outcome DTO Boundary

A future implementation may add a core-owned DTO similar to:

```text
FaultControllerActionOutcome
```

Recommended top-level fields:

- preserved `FaultControllerPassiveActionAuditRecord`;
- `outcome_kind`;
- `outcome_reason`;
- `requested_action_kind`;
- `attempted_action_kind`;
- `executed_action_kind`;
- `scheduler_control_available`;
- `scheduler_control_used`;
- `adapter_boundary_available`;
- `adapter_call_attempted`;
- `adapter_call_succeeded`;
- `adapter_call_failed`;
- `runtime_action_requested`;
- `runtime_action_attempted`;
- `runtime_action_executed`;
- `runtime_action_skipped`;
- `runtime_action_failed`;
- `operator_review_required`;
- `rollback_required`;
- `replay_required`;
- `mass_audit_required`;
- `release_gate_action_executed`;
- diagnostic and audit preservation fields.

## Candidate Outcome Kinds

A future schema should define stable outcome kinds such as:

- `not_requested`;
- `requested_only`;
- `blocked_boundary_absent`;
- `blocked_missing_evidence`;
- `skipped_policy`;
- `attempted`;
- `executed`;
- `failed`;
- `operator_review_required`.

Only a dedicated future execution boundary may produce `attempted`, `executed`, or `failed` outcomes for runtime actions.

## Candidate Outcome Reasons

A future schema should define stable reason codes such as:

- `nominal_no_action`;
- `review_required_only`;
- `action_requested_not_executed`;
- `scheduler_control_boundary_absent`;
- `adapter_boundary_absent`;
- `threshold_evidence_missing`;
- `threshold_not_met`;
- `threshold_met_execution_boundary_absent`;
- `operator_review_pending`;
- `execution_attempted`;
- `execution_succeeded`;
- `execution_failed`;
- `rollback_required_after_failure`;
- `mass_audit_required_after_execution`;
- `contradictory_action_flags`;
- `invalid_transition_input`.

Reason codes must not imply execution unless execution fields also confirm it.

## Outcome Rules For Passive Stages

For all current passive stages, a future action-outcome helper must produce non-executed outcomes:

| Passive input | Required outcome kind | Required reason | Execution flags |
|---|---|---|---|
| no-action audit | `not_requested` | `nominal_no_action` | all false |
| operator-review audit | `operator_review_required` | `review_required_only` | all false |
| requested isolation without execution boundary | `blocked_boundary_absent` | `scheduler_control_boundary_absent` or `adapter_boundary_absent` | all false |
| requested reconnect without execution boundary | `blocked_boundary_absent` | `scheduler_control_boundary_absent` or `adapter_boundary_absent` | all false |
| requested abort without execution boundary | `blocked_boundary_absent` | `scheduler_control_boundary_absent` | all false |
| missing threshold evidence | `blocked_missing_evidence` | `threshold_evidence_missing` | all false |
| contradictory flags | `operator_review_required` or `blocked_missing_evidence` | `contradictory_action_flags` | all false |

## Outcome Rules For Future Executable Stages

If a future execution boundary is explicitly implemented, outcome rules must require:

1. Scheduler control boundary available.
2. Adapter boundary available when an adapter action is needed.
3. Complete threshold evidence.
4. Preserved transition and audit provenance.
5. Pre-action state snapshot or equivalent rollback context.
6. Post-action state summary.
7. Replay requirement if runtime state changes.
8. Mass-audit requirement if runtime state changes.
9. Failure reason and recovery/rollback path if execution fails.

Without all required evidence, the outcome must remain non-executed.

## Required Non-Execution Defaults

A future outcome helper must default to:

- `scheduler_control_used = false`;
- `adapter_call_attempted = false`;
- `adapter_call_succeeded = false`;
- `adapter_call_failed = false`;
- `runtime_action_attempted = false`;
- `runtime_action_executed = false`;
- `runtime_action_failed = false`;
- `release_gate_action_executed = false`.

These defaults prevent accidental scheduler control through DTO construction.

## Required Ordering Evidence For Future Implementation

A future outcome implementation must include tests proving outcome creation before mock scheduling does not alter:

- shared exchange decision count;
- endpoint engine and node metadata;
- granted flow and volume;
- repayment flow and volume;
- unmet volume;
- allocated `Q_limit` and `V_limit` shares;
- priority weights;
- replayed cell volume;
- replayed cell deficit account;
- replayed cell depth;
- system-mass audit baseline total;
- system-mass audit current total;
- system-mass audit residual;
- system-mass audit conservation flag.

## Required Negative Tests For Future Implementation

A future outcome implementation must include tests proving:

1. No-action audit produces `not_requested` and executes nothing.
2. Operator-review audit produces review-required outcome and executes nothing.
3. Isolation request without execution boundary is blocked and executes nothing.
4. Reconnect request without execution boundary is blocked and executes nothing.
5. Abort request without execution boundary is blocked and executes nothing.
6. Missing threshold evidence blocks execution.
7. Contradictory flags block execution or require operator review.
8. Outcome creation does not mutate coupling state.
9. Real SWMM and D-Flow FM APIs remain absent unless a dedicated adapter milestone adds them.

## Evidence Requirements For Future Implementation

A future action-outcome implementation must include:

- focused unit tests for all passive and blocked outcome rows;
- focused negative tests for non-execution boundaries;
- ordering tests against mock scheduling, replay, and audit;
- full local Debug CTest evidence;
- CI evidence only after a pushed CI run exists;
- no GoldenSuite release/merge claims without required gate evidence.

## Confirmed Negative Boundary For M129

M129 does not add:

- outcome DTO code;
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

M129 defines the minimum action-outcome requirements needed before executable FaultController actions can be implemented. The next safe step is still non-executing: implement a passive action-outcome DTO skeleton or roll up M125-M129, without scheduler control.

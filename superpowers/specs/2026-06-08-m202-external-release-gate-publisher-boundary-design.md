# M202 External Release-Gate Publisher Boundary Design

## Scope

M202 designs the external release-gate publisher boundary after the M195-M201 release-gate action chain.

The boundary is intentionally outside CouplingLib core. CouplingLib core may prepare intent, record supplied emission results, and record completion, but it must not publish CI status, block release artifacts, call external services, or directly emit operator-facing release-gate states.

## Placement

The publisher boundary belongs in a future external orchestration or validation integration layer, not in:

- `libs/coupling/core/`;
- SWMM adapter code;
- D-Flow FM adapter code;
- scheduler mutation executor DTO helpers;
- numerical solver code.

The publisher may consume CouplingLib core completion records as input data, but it must own external side effects explicitly.

## Inputs

The future publisher must consume:

- `FaultControllerSchedulerReleaseGateActionCompletionRecord` or a stable serialized equivalent;
- explicit publisher boundary availability;
- operator approval for outward-facing publication;
- project policy reference for CI/release publication;
- release target reference;
- idempotency key;
- action transaction identifier;
- payload reference and payload content hash;
- dry-run or commit mode;
- supplied external sink outcome.

## Fail-Closed Preconditions

The publisher must fail closed when:

- completion is not complete;
- completion requires review;
- action failed or dry-run-only completion is supplied for commit;
- publisher boundary is absent;
- operator approval is missing;
- project policy reference is missing;
- release target reference is missing;
- idempotency key is missing;
- action transaction identifier is missing;
- payload reference or hash is missing;
- dry-run and commit modes conflict;
- the requested action is unsupported by the configured publisher sink;
- action flags are contradictory.

Fail-closed outcomes must not publish CI status, block release artifacts, or emit external operator states.

## Dry-Run Semantics

Dry-run mode may record what would be published, including:

- target sink;
- action kind (`REVIEW_REQUIRED`, `BLOCK_MERGE`, `BLOCK_RELEASE`, or `ABORT`);
- release target reference;
- idempotency key;
- payload reference/hash;
- policy reference.

Dry-run mode must not mutate CI systems, release stores, external dashboards, or operator state.

## Commit Semantics

Commit mode may publish only when all preconditions pass and the configured external sink reports success.

Commit result recording must distinguish:

- publication attempted;
- publication succeeded;
- publication failed;
- publication skipped;
- operator review required.

A failed commit must require operator review and must not be reinterpreted as merge/release readiness.

## Action Mapping

The publisher maps completion flags to external action kinds:

- `review_required` -> `REVIEW_REQUIRED`;
- `merge_blocked` -> `BLOCK_MERGE`;
- `release_blocked` -> `BLOCK_RELEASE`;
- `abort_required` -> `ABORT`.

If no action flags are present, the publisher must record pass/no-action and must not publish a blocking action.

If multiple blocking flags are present, project policy must define precedence. Without explicit precedence policy, the publisher must fail closed with review required.

## Negative Boundary

M202 does not implement:

- real CI integration;
- real release artifact blocking;
- external service publication;
- GitHub Actions status publication;
- artifact registry mutation;
- SWMM or D-Flow FM adapter calls;
- rollback execution;
- replay execution;
- mass-audit execution;
- scheduler lifecycle/process control;
- GoldenSuite, release, or merge readiness evidence.

## Required Future Evidence

Before implementation can claim publisher readiness, future evidence must include:

- dry-run pass/no-action evidence;
- dry-run review/block/abort evidence;
- fail-closed missing approval/policy/target/idempotency/payload evidence;
- conflicting action flag evidence;
- commit success evidence using a controlled test sink;
- commit failure evidence requiring operator review;
- explicit proof that CouplingLib core remains free of external publisher dependencies.

## Decision

M202 establishes the design boundary for a future external release-gate publisher. The next safe implementation task is a mock-only publisher result DTO/function in an external validation/orchestration layer, not in CouplingLib core, with no real CI or release artifact side effects.

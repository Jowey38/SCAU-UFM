# M203 Mock Release-Gate Publisher Boundary Design

## Scope

M203 designs a mock-only external release-gate publisher boundary after M202.

The boundary is outside CouplingLib core under validation/release-gate placement. It consumes release-gate action completion records and records mock publisher dry-run/commit outcomes using supplied sink results. It must not perform real CI publication, release artifact blocking, external service calls, adapter calls, rollback, replay, mass-audit, or scheduler lifecycle control.

## Placement

The mock publisher belongs under `libs/validation/release_gate/` and is linked against CouplingLib core DTOs as input data only.

CouplingLib core must not depend on the mock publisher.

## Inputs

The mock publisher consumes:

- release-gate action completion record;
- publisher boundary availability;
- operator approval;
- project policy reference;
- release target reference;
- idempotency key;
- action transaction identifier;
- payload reference;
- payload hash;
- dry-run or commit mode;
- supplied mock sink outcome.

## Fail-Closed Requirements

The mock publisher blocks when:

- completion is not complete;
- publisher boundary is absent;
- operator approval is missing;
- project policy reference is missing;
- release target reference is missing;
- idempotency key is missing;
- action transaction identifier is missing;
- payload reference is missing;
- payload hash is missing;
- publish mode is missing;
- dry-run and commit modes conflict.

It returns review-required when completion requires review or action flags are contradictory.

## Result Mapping

The mock publisher records:

- dry-run as `dry_run_recorded` without publish attempt;
- commit success as `published` using the supplied mock sink outcome;
- commit failure as `failed` using the supplied mock sink outcome;
- pass/no-action as published no blocking action;
- review/block/release/abort flags as data-only action kinds.

## Negative Boundary

M203 does not add:

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

## Decision

Implement a mock-only publisher result DTO/function under `libs/validation/release_gate/` with focused fail-closed, dry-run, success, failure, review, contradiction, and pass/no-action tests.

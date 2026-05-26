# M40 Coupling System Mass Runtime Gate Design

## Goal

Define the first runtime-gate semantics that can be built on top of the existing read-only system-mass diagnostic chain:

- `SystemMassDelta`
- `SystemMassConservationStatus`
- `SystemMassConservationDiagnostic`
- `CouplingState::diagnose_system_mass_against_snapshot(...)`

This document is design-only. It does not introduce code, tests, CI changes, runtime control-flow changes, or a new gate implementation.

## Existing Inputs

The current diagnostic chain provides strict, deterministic values:

- baseline total mass
- current total mass
- residual: `current.total_mass - baseline.total_mass`
- status: `conserved` only when strict `delta.conserved == true`; otherwise `drifted`

The existing path intentionally has no tolerance and no side effects.

## Proposed Gate Inputs

A future gate should take only these explicit inputs:

- `SystemMassConservationDiagnostic diagnostic`
- runtime context metadata supplied by the caller, not owned by the diagnostic layer:
  - coupling step index
  - substep index, if applicable
  - snapshot identifier or checkpoint reference
  - correctness-path vs performance-path mode
  - whether the result is intended for merge/release evidence

The gate must not recompute hidden state or mutate `CouplingState` while evaluating a diagnostic.

## Proposed Gate Output

A future gate should return a small value type rather than directly aborting:

```cpp
enum class SystemMassGateAction {
    continue_run,
    review_required,
    abort_run,
};

struct SystemMassGateDecision {
    SystemMassGateAction action;
    SystemMassConservationDiagnostic diagnostic;
};
```

The gate decision is a classification result. Separate runtime orchestration code would be responsible for turning `abort_run` into black-box writeout or process termination.

## Correctness-Path Semantics

For the correctness path, M31-M39 establish strict equality semantics. Therefore future gate behavior should be:

| Diagnostic status | Required gate action | Rationale |
|---|---|---|
| `conserved` | `continue_run` | strict residual is zero |
| `drifted` | `abort_run` candidate | strict conservation failed on the correctness path |

The first implementation should be conservative and avoid implicit recovery. If `drifted` occurs on the correctness path, the design candidate is `abort_run` because the existing correctness chain has already ruled out tolerance-based interpretation.

## Performance-Path Semantics

The main architecture spec defines `epsilon_mass = max(1e-10, 1e-12 × M_ref)` as a total mass conservation tolerance, but M31-M39 deliberately did not implement that tolerance.

A future performance-path gate may introduce a separate tolerance-aware helper, but that must be a distinct milestone because it changes semantics:

- explicit `epsilon_mass` input
- tests for residual inside, at, and outside tolerance
- evidence that correctness path remains strict
- no reuse of strict `classify_system_mass_conservation(...)` for tolerance decisions

Until that exists, performance-path gate behavior should not be implemented.

## Stability Protocol Mapping

Based on the stability/reliability protocol status vocabulary:

| Gate action | Protocol state candidate | Notes |
|---|---|---|
| `continue_run` | none | normal path |
| `review_required` | `REVIEW_REQUIRED` | human review, not release-grade evidence |
| `abort_run` | `ABORT` | runtime must stop and black-box evidence should be written by orchestration |

The diagnostic layer itself must not emit `BLOCK_MERGE` or `BLOCK_RELEASE`. CI/release states are consequences of evidence evaluation or runtime failure propagation, not pure diagnostic generation.

## Evidence Requirements Before Implementation

Before implementing a runtime gate, provide evidence for:

1. strict conserved diagnostic maps to `continue_run`
2. strict drifted diagnostic maps to `abort_run` on correctness path
3. diagnostic fields are preserved in the decision value
4. no `CouplingState` mutation occurs during gate evaluation
5. orchestration tests, if any, prove that `abort_run` handling is outside the pure gate helper
6. no `epsilon_mass` path is active unless a separate tolerance milestone is implemented

## Non-Goals

M40 does not define or implement:

- tolerance-aware mass conservation
- `epsilon_mass` helper behavior
- automatic baseline lifecycle management
- automatic snapshot creation
- rollback or replay triggering
- black-box file writing
- `ABORT` execution mechanics
- `REVIEW_REQUIRED` operator workflow
- CI `BLOCK_MERGE` or `BLOCK_RELEASE` enforcement
- SWMM / D-Flow FM adapter integration
- scheduler integration
- multi-donor arbitration

## Recommended M41 Slice

The next smallest implementation slice should be a pure gate-decision helper:

- add `SystemMassGateAction`
- add `SystemMassGateDecision`
- add `decide_system_mass_gate_action(const SystemMassConservationDiagnostic&)`
- strict behavior only:
  - `conserved -> continue_run`
  - `drifted -> abort_run`
- no runtime orchestration side effects

That keeps the first gate implementation testable and avoids introducing process-control behavior before the decision semantics are locked.

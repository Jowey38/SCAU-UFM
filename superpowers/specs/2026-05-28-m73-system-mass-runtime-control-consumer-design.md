# M73 System Mass Runtime Control Consumer Design

## Scope

M73 closes the gap left explicit by M72: the system-mass helper chain can produce a `SystemMassRuntimeControlDecision`, but no outer runtime/operator boundary currently consumes that decision. M73 defines a minimal consumer boundary that maps a runtime control decision into an explicit runtime control result.

The boundary is intentionally narrow:

- input: an existing `SystemMassRuntimeControlDecision`;
- output: a small runtime control result that preserves the decision and exposes whether the caller should continue or abort;
- behavior: `should_abort == false` maps to continue, `should_abort == true` maps to abort.

## Architecture

The consumer belongs in `CouplingLib`, not in SWMM, D-Flow FM, or either adapter. It should remain in the coupling core layer because it consumes only coupling DTOs and helper outputs.

The design should add the smallest production API needed to make consumption explicit. A suitable shape is a pure helper near the existing system-mass runtime-control helpers:

```text
consume_system_mass_runtime_control_decision(SystemMassRuntimeControlDecision) -> SystemMassRuntimeControlResult
```

The result type should carry:

- the original `SystemMassRuntimeControlDecision` for diagnostics and audit continuity;
- a control state with continue/abort semantics.

This is a consumer boundary, not a process-control boundary. It must not terminate the process, throw for drift, call rollback/replay, or write evidence files.

## Data Flow

1. Existing code computes system mass audit/diagnostic/gate/runtime-control helpers.
2. M73 receives the resulting `SystemMassRuntimeControlDecision`.
3. The consumer reads `decision.should_abort`.
4. If false, it returns a continue result.
5. If true, it returns an abort result.

This keeps the chain explicit:

```text
compute_system_mass -> audit -> diagnostic -> gate_decision -> gate_outcome -> runtime_control_decision -> runtime_control_consumer_result
```

M73 extends M72 by proving the final decision is consumed by a production boundary, while still avoiding a real scheduler loop or process abort.

## Error Handling And Boundaries

The consumer should be total for valid `SystemMassRuntimeControlDecision` values. It should not introduce new validation, tolerances, or exceptions. The existing helper chain remains responsible for producing internally consistent decisions.

M73 does not add:

- process termination;
- rollback or replay orchestration;
- snapshot creation;
- SWMM or D-Flow FM integration;
- runtime evidence file writing;
- `REVIEW_REQUIRED`, `BLOCK_MERGE`, or `BLOCK_RELEASE` handling.

## Testing

Add focused GoogleTest coverage in `tests/unit/coupling/test_coupling_core_state.cpp` or a nearby coupling-core test file, following existing style.

Required cases:

- conserved/reference decision -> consumer result continues;
- drifted/reference decision -> consumer result aborts;
- conserved/snapshot decision -> consumer result continues;
- drifted/snapshot decision -> consumer result aborts.

Assertions should verify that the consumer result preserves the original decision fields needed by downstream diagnostics: gate outcome status, gate action, diagnostic status, residual, baseline total mass, current total mass, handling state, and `should_abort`.

## Evidence

M73 evidence should record:

- the new consumer API boundary;
- the four focused tests listed above;
- local focused build/test output for `test_coupling_core_state` or the selected coupling-core test target;
- full serial Debug CTest output if the focused test passes;
- CI run and job results after the test/evidence commits are pushed.

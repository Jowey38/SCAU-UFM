# M74 System Mass Runtime Operator Boundary Design

## Scope

M74 extends M73 by defining the smallest outer runtime/operator boundary that consumes `SystemMassRuntimeControlResult` and exposes an explicit operator action. M73 proved that coupling core can consume a `SystemMassRuntimeControlDecision` into a continue/abort result; M74 defines how an outer runtime-facing helper reads that result and turns it into an operator-visible action.

The M74 boundary is intentionally narrow:

- input: an existing `SystemMassRuntimeControlResult`;
- output: a small operator action DTO that preserves the consumed result and exposes whether the outer runtime should continue or request abort;
- behavior: `SystemMassRuntimeControlState::continue_run` maps to continue, `SystemMassRuntimeControlState::abort` maps to abort requested.

## Architecture

The operator boundary remains in `CouplingLib` and should not depend on SWMM, D-Flow FM, or any 1D engine ABI. It belongs near the coupling-core runtime-control helpers because it consumes only coupling DTOs and produces a coupling-owned action result.

A suitable minimal shape is:

```text
make_system_mass_runtime_operator_action(SystemMassRuntimeControlResult) -> SystemMassRuntimeOperatorAction
```

The result type should carry:

- the original `SystemMassRuntimeControlResult` for diagnostics and audit continuity;
- an operator action state with `continue_run` and `abort_requested` semantics.

This is not a process-control implementation. M74 must not terminate the process, throw on drift, perform rollback/replay, write runtime evidence files, or invoke adapters.

## Data Flow

M74 extends the explicit chain by one outer boundary:

```text
compute_system_mass -> audit -> diagnostic -> gate_decision -> gate_outcome -> runtime_control_decision -> runtime_control_result -> runtime_operator_action
```

The data flow is:

1. Existing helpers produce a `SystemMassRuntimeControlDecision`.
2. M73 consumes that decision into `SystemMassRuntimeControlResult`.
3. M74 receives the result.
4. If `result.state == SystemMassRuntimeControlState::continue_run`, it returns an operator action with continue semantics.
5. If `result.state == SystemMassRuntimeControlState::abort`, it returns an operator action with abort-requested semantics.

This proves the runtime-control result is consumed by an outer runtime-facing boundary while still avoiding process termination and scheduler orchestration.

## Error Handling And Boundaries

The operator helper should be total for valid `SystemMassRuntimeControlResult` values. It should not introduce new tolerances, conservation thresholds, validation branches, exceptions, or state mutation. Existing audit, diagnostic, gate, runtime-control, and consumer helpers remain responsible for producing internally consistent inputs.

M74 does not add:

- process termination;
- rollback or replay orchestration;
- snapshot creation requirements;
- SWMM or D-Flow FM integration;
- runtime evidence file writing;
- tolerance or `epsilon_mass` behavior;
- `REVIEW_REQUIRED`, `BLOCK_MERGE`, or `BLOCK_RELEASE` handling.

## Testing

Add focused GoogleTest coverage in the coupling-core test area, following the current `test_coupling_core_state.cpp` style.

Required cases:

- conserved/reference path -> runtime-control result -> operator action continues;
- drifted/reference path -> runtime-control result -> operator action requests abort;
- conserved/snapshot path -> runtime-control result -> operator action continues;
- drifted/snapshot path -> runtime-control result -> operator action requests abort.

Assertions should verify that the operator action preserves the consumed runtime-control result and its diagnostic chain: operator action state, runtime-control result state, gate outcome status, gate action, diagnostic status, residual, baseline total mass, current total mass, handling state, and `should_abort`.

## Evidence

M74 evidence should record:

- the new operator action API boundary;
- the four focused tests listed above;
- local focused build/test output for the selected coupling-core test target;
- full serial Debug CTest output if focused tests pass;
- CI run and job results after implementation and evidence commits are pushed.

# M75 System Mass Audit-To-Runtime-Operator-Action End-To-End Consistency Evidence

## Scope

M75 adds end-to-end consistency tests that chain `audit_system_mass_against_*` through `make_system_mass_conservation_diagnostic`, `decide_system_mass_gate_action`, `make_system_mass_runtime_gate_outcome`, `make_system_mass_runtime_control_decision`, `consume_system_mass_runtime_control_decision`, and `make_system_mass_runtime_operator_action`, then assert the result equals the matching state helper path followed by the M73/M74 consumers for both reference and snapshot baselines under conserved and drifted system-mass states.

Helpers exercised:

- `CouplingState::audit_system_mass_against_reference(...)`
- `CouplingState::audit_system_mass_against_snapshot(...)`
- `CouplingState::decide_system_mass_runtime_control_against_reference(...)`
- `CouplingState::decide_system_mass_runtime_control_against_snapshot(...)`
- `make_system_mass_conservation_diagnostic(const SystemMassDelta&)`
- `decide_system_mass_gate_action(const SystemMassConservationDiagnostic&)`
- `make_system_mass_runtime_gate_outcome(const SystemMassGateDecision&)`
- `make_system_mass_runtime_control_decision(const SystemMassRuntimeGateOutcome&)`
- `consume_system_mass_runtime_control_decision(const SystemMassRuntimeControlDecision&)`
- `make_system_mass_runtime_operator_action(const SystemMassRuntimeControlResult&)`

Asserted invariants:

- `make_system_mass_runtime_operator_action(consume_system_mass_runtime_control_decision(make_system_mass_runtime_control_decision(make_system_mass_runtime_gate_outcome(decide_system_mass_gate_action(make_system_mass_conservation_diagnostic(audit_against_*))))))` agrees with `make_system_mass_runtime_operator_action(consume_system_mass_runtime_control_decision(decide_system_mass_runtime_control_against_*))`;
- operator action state agrees;
- runtime-control result state agrees;
- runtime gate outcome fields agree (`status`, `action`, diagnostic `status`, `residual`, `baseline_total_mass`, `current_total_mass`);
- runtime control fields agree (`handling_state`, `should_abort`);
- conserved -> operator action `continue_run`;
- drifted -> operator action `abort_requested`.

The tests extend the helper-consistency audit chain by two more hops (`runtime_control_decision -> runtime_control_result -> runtime_operator_action`), complementing M72/M73/M74. This evidence does not prove that an outer scheduler terminates the process, writes runtime evidence, or invokes adapters; those remain outside the M75 boundary.

## Boundary

M75 is a pure consistency-test boundary only.

It does not add:

- new production API or DTOs;
- process termination;
- black-box or evidence file writing from runtime;
- rollback or replay orchestration;
- snapshot creation requirements;
- SWMM or D-Flow FM integration;
- tolerance or `epsilon_mass` behavior;
- `REVIEW_REQUIRED`, `BLOCK_MERGE`, or `BLOCK_RELEASE` handling.

## Local Evidence

Focused build and test:

```text
cmake --build --preset windows-msvc --target test_coupling_core_state
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_core_state$"
```

Result:

```text
1/1 Test #23: test_coupling_core_state .........   Passed    0.20 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) =   0.29 sec
```

Full serial Debug CTest:

```text
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
32/32 Test #32: test_coupling_system_mass_audit ..... Passed
100% tests passed, 0 tests failed out of 32
Total Test time (real) =   5.82 sec
```

## CI Evidence

Pending. Record the push commit and GitHub Actions run after this implementation/evidence change is pushed.

## Review Evidence

Manual boundary review found the M75 change is limited to:

- new GoogleTest cases under `tests/unit/coupling/test_coupling_core_state.cpp` that assert the full audit-to-operator-action helper chain agrees with the matching `CouplingState::decide_system_mass_runtime_control_against_*` path followed by M73/M74 consumers for reference and snapshot baselines under conserved and drifted system-mass states;
- this evidence record under `superpowers/specs/`.

The change does not modify production headers or source, does not add or change DTOs, and does not introduce adapters, process termination, runtime side effects, runtime evidence writing, rollback, replay, tolerance, snapshot creation requirements, or release/merge gate handling.

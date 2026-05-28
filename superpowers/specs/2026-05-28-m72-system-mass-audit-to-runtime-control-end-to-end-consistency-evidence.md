# M72 System Mass Audit-To-Runtime-Control End-To-End Consistency Evidence

## Scope

M72 added end-to-end consistency tests that chain `audit_system_mass_against_*` through `make_system_mass_conservation_diagnostic`, `decide_system_mass_gate_action`, `make_system_mass_runtime_gate_outcome`, and `make_system_mass_runtime_control_decision`, then assert the result equals the matching `decide_system_mass_runtime_control_against_*` helper, for both reference and snapshot baselines, under conserved and drifted system-mass states.

Helpers exercised:

- `CouplingState::audit_system_mass_against_reference(...)`
- `CouplingState::audit_system_mass_against_snapshot(...)`
- `CouplingState::decide_system_mass_runtime_control_against_reference(...)`
- `CouplingState::decide_system_mass_runtime_control_against_snapshot(...)`
- `make_system_mass_conservation_diagnostic(const SystemMassDelta&)`
- `decide_system_mass_gate_action(const SystemMassConservationDiagnostic&)`
- `make_system_mass_runtime_gate_outcome(const SystemMassGateDecision&)`
- `make_system_mass_runtime_control_decision(const SystemMassRuntimeGateOutcome&)`

Asserted invariants:

- `make_system_mass_runtime_control_decision(make_system_mass_runtime_gate_outcome(decide_system_mass_gate_action(make_system_mass_conservation_diagnostic(audit_against_*))))` agrees with `decide_system_mass_runtime_control_against_*`;
- runtime gate outcome fields agree (`status`, `action`, diagnostic `status`, `residual`, `baseline_total_mass`, `current_total_mass`);
- runtime control fields agree (`handling_state`, `should_abort`);
- conserved -> `status == running`, `handling_state == continue_run`, and `should_abort == false`;
- drifted -> `status == abort`, `handling_state == abort`, and `should_abort == true`.

The tests extend the helper-consistency audit chain by one more hop (`runtime_gate_outcome -> runtime_control_decision`), complementing M71 (`audit -> runtime_gate`). Together with M61/M63/M64/M65/M66/M67/M68/M69/M70/M71, the `compute_system_mass -> audit -> diagnostic -> gate_decision -> gate_outcome -> handling_state -> should_abort` helper chain is covered through both stepwise and bridged invariants on reference and snapshot baselines. This evidence does not prove that an outer runtime scheduler or operator loop consumes `SystemMassRuntimeControlDecision`; that remains outside the M72 boundary.

## Boundary

M72 is a pure consistency-test boundary only.

It does not add:

- new public API or DTOs;
- process termination;
- black-box or evidence file writing from runtime;
- rollback or replay orchestration;
- snapshot creation requirements;
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
1/1 Test #23: test_coupling_core_state .........   Passed    0.22 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) =   0.46 sec
```

Full serial Debug CTest:

```text
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
32/32 Test #32: test_coupling_system_mass_audit ..... Passed
100% tests passed, 0 tests failed out of 32
Total Test time (real) =   8.40 sec
```

## CI Evidence

M72 commit:

```text
510cbe6 test(coupling): assert runtime control against baseline matches audit chain
```

GitHub Actions CI run:

```text
26572566628 push master success
```

Jobs:

```text
spike-isolation-check success
linux-gcc success
windows-msvc success
```

## Review Evidence

Manual boundary review found the M72 change is limited to:

- new GoogleTest cases under `tests/unit/coupling/test_coupling_core_state.cpp` that assert `make_system_mass_runtime_control_decision` chained from `audit_system_mass_against_*` agrees with `decide_system_mass_runtime_control_against_*` for reference and snapshot baselines under conserved and drifted system-mass states;
- a new evidence record under `superpowers/specs/` for the local focused build and test result.

The change does not modify production headers or source, does not add or change DTOs, and does not introduce process termination, runtime side effects, runtime evidence writing, rollback, replay, tolerance, snapshot creation requirements, or release/merge gate handling.

# M74 System Mass Runtime Operator Boundary Evidence

## Scope

M74 adds a minimal production operator boundary for `SystemMassRuntimeControlResult`. The operator helper maps `SystemMassRuntimeControlState::continue_run` to `SystemMassRuntimeOperatorActionState::continue_run` and `SystemMassRuntimeControlState::abort` to `SystemMassRuntimeOperatorActionState::abort_requested`, while preserving the original runtime-control result for diagnostics and audit continuity.

Helpers exercised:

- `CouplingState::decide_system_mass_runtime_control_against_reference(...)`
- `CouplingState::decide_system_mass_runtime_control_against_snapshot(...)`
- `consume_system_mass_runtime_control_decision(const SystemMassRuntimeControlDecision&)`
- `make_system_mass_runtime_operator_action(const SystemMassRuntimeControlResult&)`

Asserted invariants:

- conserved reference path -> runtime-control result -> operator action continues;
- drifted reference path -> runtime-control result -> operator action requests abort;
- conserved snapshot path -> runtime-control result -> operator action continues;
- drifted snapshot path -> runtime-control result -> operator action requests abort;
- operator action preserves runtime-control result state, gate outcome status, gate action, diagnostic status, residual, baseline total mass, current total mass, handling state, and `should_abort`.

## Boundary

M74 is an operator-boundary helper only.

It does not add:

- process termination;
- rollback or replay orchestration;
- snapshot creation requirements;
- SWMM or D-Flow FM integration;
- runtime evidence file writing;
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
1/1 Test #23: test_coupling_core_state .........   Passed    0.42 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) =   0.82 sec
```

Full serial Debug CTest:

```text
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
32/32 Test #32: test_coupling_system_mass_audit ..... Passed
100% tests passed, 0 tests failed out of 32
Total Test time (real) =  13.66 sec
```

## CI Evidence

Pending. Record the push commit and GitHub Actions run after this implementation/evidence change is pushed.

## Review Evidence

Manual boundary review found the M74 change is limited to:

- one coupling-core operator action enum and DTO;
- one pure helper that consumes `SystemMassRuntimeControlResult`;
- GoogleTest cases proving reference and snapshot runtime-control results are consumed into continue/abort-requested operator actions under conserved and drifted system-mass states;
- this evidence record.

The change does not modify adapters, does not terminate the process, does not write runtime evidence files, and does not introduce rollback, replay, tolerance, or release/merge gate handling.

# M73 System Mass Runtime Control Consumer Evidence

## Scope

M73 adds a minimal production consumer boundary for `SystemMassRuntimeControlDecision`. The consumer maps `should_abort == false` to `SystemMassRuntimeControlState::continue_run` and `should_abort == true` to `SystemMassRuntimeControlState::abort`, while preserving the original decision for diagnostics and audit continuity.

Helpers exercised:

- `CouplingState::decide_system_mass_runtime_control_against_reference(...)`
- `CouplingState::decide_system_mass_runtime_control_against_snapshot(...)`
- `consume_system_mass_runtime_control_decision(const SystemMassRuntimeControlDecision&)`

Asserted invariants:

- conserved reference decision -> consumer result continues;
- drifted reference decision -> consumer result aborts;
- conserved snapshot decision -> consumer result continues;
- drifted snapshot decision -> consumer result aborts;
- consumer result preserves gate outcome status, gate action, diagnostic status, residual, baseline total mass, current total mass, handling state, and `should_abort`.

## Boundary

M73 is a consumer-boundary helper only.

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
1/1 Test #23: test_coupling_core_state .........   Passed    0.18 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) =   0.21 sec
```

Full Debug build and serial CTest:

```text
cmake --build --preset windows-msvc
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
32/32 Test #32: test_coupling_system_mass_audit ..... Passed
100% tests passed, 0 tests failed out of 32
Total Test time (real) =  10.91 sec
```

## CI Evidence

Not collected in this local M73 pass because the evidence commit has not yet been pushed.

## Review Evidence

Manual boundary review found the M73 change is limited to:

- one coupling-core result enum and DTO;
- one pure helper that consumes `SystemMassRuntimeControlDecision`;
- GoogleTest cases proving reference and snapshot decisions are consumed into continue/abort results under conserved and drifted system-mass states.

The change does not modify adapters, does not terminate the process, does not write runtime evidence files, and does not introduce rollback, replay, tolerance, or release/merge gate handling.

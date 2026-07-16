# M94 Engine Interface Core Runtime Member Guard Evidence

## Scope

M94 strengthens the mock adapter boundary tests by replacing the previous weak abstract-class constructibility smoke check with direct compile-time detection of forbidden core-runtime member names on `ISwmmEngine` and `IDFlowFMEngine`.

Forbidden member names checked on both engine interfaces:

- `rollback`
- `replay_pending`
- `mass_deficit_account`
- `q_limit`
- `v_limit`
- `accept_exchange_decision`

These names represent core/runtime semantics that must not be carried by the 1D engine lifecycle/state interfaces.

## Boundary

M94 is a test-strengthening change only.

It does not add:

- SWMM headers, C API calls, native libraries, or `extern/swmm5/` dependencies;
- D-Flow FM / BMI headers, native libraries, or `extern/dflowfm*` dependencies;
- real `SWMMAdapter` or `DFlowFMAdapter` implementation;
- new `Q_limit` or `V_limit` definitions outside `CouplingLib core`;
- new `mass_deficit_account` behavior outside `CouplingLib core`;
- shared arbitration, rollback, replay, runtime scheduler, or runtime evidence writing;
- GoldenSuite, release-gate, `REVIEW_REQUIRED`, `BLOCK_MERGE`, or `BLOCK_RELEASE` execution.

## Tests

Focused unit tests remain under `tests/unit/coupling/test_coupling_adapter_boundaries.cpp`.

Updated asserted invariant:

- `EngineInterfacesDoNotCarryCoreRuntimeSemantics` now uses SFINAE member-detection traits rather than abstract-class constructibility checks.

The test now directly verifies that neither `ISwmmEngine` nor `IDFlowFMEngine` exposes the forbidden core-runtime member names listed above.

Existing M77-M93 invariants remain covered.

## Local Evidence

Focused build and test:

```text
cmake --build --preset windows-msvc --target test_coupling_adapter_boundaries
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_adapter_boundaries$"
```

Result:

```text
1/1 Test #24: test_coupling_adapter_boundaries ...   Passed    0.18 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) =   0.27 sec
```

Full serial Debug CTest:

```text
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
33/33 Test #33: test_coupling_system_mass_audit ..... Passed
100% tests passed, 0 tests failed out of 33
Total Test time (real) =   5.79 sec
```

## CI Evidence

Pending. Record the push commit and GitHub Actions run after this implementation/evidence change is pushed.

## Review Evidence

Manual boundary review found the M94 change is limited to:

- adding compile-time member-detection traits in the adapter-boundary test file;
- replacing weak abstract-class constructibility assertions with direct forbidden-member assertions;
- this evidence record.

The change does not include or link third-party engine code, does not expose native SWMM/D-Flow FM/BMI types to `CouplingLib core`, and does not implement shared arbitration, flow limits, deficit ledger behavior, rollback/replay, scheduler process control, or release/merge gate handling outside their normative owners.

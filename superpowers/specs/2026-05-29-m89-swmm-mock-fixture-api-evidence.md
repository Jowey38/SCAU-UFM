# M89 SWMM Mock Fixture API Evidence

## Scope

M89 adds deterministic fixture setters to the mock-only SWMM adapter boundary so adapter-boundary tests can configure node head and link flow without depending on a real SWMM engine.

Added mock-only APIs:

- `MockSwmmEngine::set_node_head_fixture(int node_id, double head)`
- `MockSwmmEngine::set_link_flow_fixture(int link_id, double q)`

These APIs are intentionally mock-only. They do not model SWMM hydraulics and do not imply real SWMM native API support.

## Behavior

`set_node_head_fixture(...)`:

- requires initialized mock engine;
- requires non-negative `node_id`;
- requires finite `head`;
- stores deterministic value returned by `get_node_head(node_id)`.

`set_link_flow_fixture(...)`:

- requires initialized mock engine;
- requires non-negative `link_id`;
- requires finite `q`;
- stores deterministic value returned by `get_link_flow(link_id)`.

`initialize(...)` and `finalize()` continue to clear fixture state.

## Boundary

M89 is a mock fixture API addition only.

It does not add:

- SWMM headers, C API calls, native libraries, or `extern/swmm5/` dependencies;
- real `SWMMAdapter` implementation;
- D-Flow FM / BMI headers, native libraries, or `extern/dflowfm*` dependencies;
- real `DFlowFMAdapter` implementation;
- new `Q_limit` or `V_limit` definitions outside `CouplingLib core`;
- new `mass_deficit_account` behavior outside `CouplingLib core`;
- shared arbitration, rollback, replay, runtime scheduler, or runtime evidence writing;
- GoldenSuite, release-gate, `REVIEW_REQUIRED`, `BLOCK_MERGE`, or `BLOCK_RELEASE` execution.

## Tests

Focused unit tests remain under `tests/unit/coupling/test_coupling_adapter_boundaries.cpp`.

New asserted invariants:

- SWMM mock fixture head is returned deterministically by `get_node_head(...)`;
- SWMM mock fixture link flow is returned deterministically by `get_link_flow(...)`;
- SWMM mock fixture setters reject negative IDs;
- SWMM mock fixture setters reject non-finite values;
- SWMM mock fixture setters require initialized engine.

Existing M77-M88 invariants remain covered.

## Local Evidence

Focused build and test:

```text
cmake --build --preset windows-msvc --target test_coupling_adapter_boundaries
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_adapter_boundaries$"
```

Result:

```text
1/1 Test #24: test_coupling_adapter_boundaries ...   Passed    0.28 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) =   0.38 sec
```

Full serial Debug CTest:

```text
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
33/33 Test #33: test_coupling_system_mass_audit ..... Passed
100% tests passed, 0 tests failed out of 33
Total Test time (real) =   6.34 sec
```

## CI Evidence

Pending. Record the push commit and GitHub Actions run after this implementation/evidence change is pushed.

## Review Evidence

Manual boundary review found the M89 change is limited to:

- adding two SWMM mock fixture setter APIs;
- validating initialized state, IDs, and finite fixture values;
- adding unit tests for deterministic fixture state and invalid fixture input rejection;
- this evidence record.

The change does not include or link third-party engine code, does not expose native SWMM/D-Flow FM/BMI types to `CouplingLib core`, and does not implement shared arbitration, flow limits, deficit ledger behavior, rollback/replay, scheduler process control, or release/merge gate handling outside their normative owners.

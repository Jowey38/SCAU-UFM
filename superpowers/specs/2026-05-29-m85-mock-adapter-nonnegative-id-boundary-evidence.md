# M85 Mock Adapter Nonnegative ID Boundary Evidence

## Scope

M85 tightens the mock-only drainage and river adapter boundaries by rejecting negative 1D identifiers at the mock seams.

Updated SWMM mock behavior:

- `MockSwmmEngine::get_node_head(node_id)` requires `node_id >= 0`;
- `MockSwmmEngine::get_node_lateral_inflow(node_id)` requires `node_id >= 0`;
- `MockSwmmEngine::set_node_lateral_inflow(node_id, ...)` requires `node_id >= 0`;
- `MockSwmmEngine::is_surcharged(node_id)` requires `node_id >= 0`;
- `MockSwmmEngine::get_link_flow(link_id)` requires `link_id >= 0`;
- `accept_swmm_exchange_decision(..., node_id, ...)` requires `node_id >= 0`.

Updated D-Flow FM mock behavior:

- `MockDFlowFMEngine::get_value(..., location_id)` requires `location_id >= 0`;
- `MockDFlowFMEngine::set_value(..., location_id, ...)` requires `location_id >= 0`;
- `accept_dflowfm_exchange_decision(..., location_id, ...)` requires `location_id >= 0`.

This prevents invalid 1D indices from silently entering mock adapter state or accepted-exchange writes.

## Boundary

M85 is a mock boundary validation tightening only.

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

New asserted invariants:

- SWMM mock rejects negative node IDs for node getters/setters and surcharge state;
- SWMM mock rejects negative link IDs for link flow;
- SWMM accept helper rejects negative node IDs;
- D-Flow FM mock rejects negative location IDs for get/set;
- D-Flow FM accept helper rejects negative location IDs.

Existing M77/M78/M79/M80/M81/M82/M83/M84 invariants remain covered.

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
Total Test time (real) =   6.33 sec
```

## CI Evidence

Pending. Record the push commit and GitHub Actions run after this implementation/evidence change is pushed.

## Review Evidence

Manual boundary review found the M85 change is limited to:

- adding non-negative ID checks to mock SWMM node/link access and accept helper;
- adding non-negative location ID checks to mock D-Flow FM get/set and accept helper;
- adding unit tests for rejected negative IDs;
- this evidence record.

The change does not include or link third-party engine code, does not expose native SWMM/D-Flow FM/BMI types to `CouplingLib core`, and does not implement shared arbitration, flow limits, deficit ledger behavior, rollback/replay, scheduler process control, or release/merge gate handling outside their normative owners.

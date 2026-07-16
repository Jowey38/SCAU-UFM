# M79 Mock Adapter Finite Input Validation Evidence

## Scope

M79 tightens the M77 mock-only drainage and river adapter boundaries by rejecting non-finite time-step, flow-intent, and accepted-flow values at the mock seams.

Updated validation:

- `MockSwmmEngine::step(dt_swmm)` requires finite positive `dt_swmm`;
- `MockSwmmEngine::set_node_lateral_inflow(q)` requires finite `q`;
- `make_swmm_exchange_request(q_request, dt_sub)` requires finite non-negative `q_request` and finite positive `dt_sub`;
- `accept_swmm_exchange_decision(...)` requires finite positive `dt_sub` and finite `q_granted` / `q_repay`;
- `MockDFlowFMEngine::update(dt_dfm)` requires finite positive `dt_dfm`;
- `MockDFlowFMEngine::set_value(value)` requires finite `value`;
- `make_dflowfm_exchange_request(q_request, dt_sub)` requires finite non-negative `q_request` and finite positive `dt_sub`;
- `accept_dflowfm_exchange_decision(...)` requires finite positive `dt_sub` and finite `q_granted` / `q_repay`.

This prevents `NaN` / `Inf` values from entering mock state through adapter-boundary helper calls before real SWMM or D-Flow FM adapter implementation is allowed by spike evidence.

## Boundary

M79 is a mock boundary validation tightening only.

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

- SWMM mock rejects non-finite step time;
- SWMM mock rejects non-finite lateral inflow;
- SWMM exchange request helper rejects non-finite `q_request` and `dt_sub`;
- D-Flow FM mock rejects non-finite update time;
- D-Flow FM mock rejects non-finite state values;
- D-Flow FM exchange request helper rejects non-finite `q_request` and `dt_sub`;
- both accept helpers reject non-finite decision flow fields.

Existing M77/M78 invariants remain covered.

## Local Evidence

Focused build and test:

```text
cmake --build --preset windows-msvc --target test_coupling_adapter_boundaries
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_adapter_boundaries$"
```

Result:

```text
1/1 Test #24: test_coupling_adapter_boundaries ...   Passed    0.22 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) =   0.31 sec
```

Full serial Debug CTest:

```text
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
33/33 Test #33: test_coupling_system_mass_audit ..... Passed
100% tests passed, 0 tests failed out of 33
Total Test time (real) =   6.12 sec
```

## CI Evidence

Pending. Record the push commit and GitHub Actions run after this implementation/evidence change is pushed.

## Review Evidence

Manual boundary review found the M79 change is limited to:

- adding finite checks to mock drainage and river adapter-boundary helper implementations;
- adding unit tests for non-finite input rejection;
- this evidence record.

The change does not include or link third-party engine code, does not expose native SWMM/D-Flow FM/BMI types to `CouplingLib core`, and does not implement shared arbitration, flow limits, deficit ledger behavior, rollback/replay, scheduler process control, or release/merge gate handling outside their normative owners.

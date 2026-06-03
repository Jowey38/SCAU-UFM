# M80 Mock Adapter Nonnegative Accepted Flow Evidence

## Scope

M80 tightens the M77/M79 mock-only drainage and river adapter accept helpers by rejecting negative accepted-flow fields from `core::ExchangeDecision`.

Updated validation:

- `accept_swmm_exchange_decision(...)` requires finite non-negative `q_granted` and `q_repay`;
- `accept_dflowfm_exchange_decision(...)` requires finite non-negative `q_granted` and `q_repay`.

This keeps mock adapter seams aligned with the coupling-core exchange-decision invariant that granted and repayment flow terms are non-negative before they are written into a 1D engine boundary.

## Boundary

M80 is a mock boundary validation tightening only.

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

- SWMM accept helper rejects negative `q_granted`;
- SWMM accept helper rejects negative `q_repay`;
- D-Flow FM accept helper rejects negative `q_granted`;
- D-Flow FM accept helper rejects negative `q_repay`.

Existing M77/M78/M79 invariants remain covered.

## Local Evidence

Focused build and test:

```text
cmake --build --preset windows-msvc --target test_coupling_adapter_boundaries
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_adapter_boundaries$"
```

Result:

```text
1/1 Test #24: test_coupling_adapter_boundaries ...   Passed    0.20 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) =   0.29 sec
```

Full serial Debug CTest:

```text
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
33/33 Test #33: test_coupling_system_mass_audit ..... Passed
100% tests passed, 0 tests failed out of 33
Total Test time (real) =   5.66 sec
```

## CI Evidence

Pending. Record the push commit and GitHub Actions run after this implementation/evidence change is pushed.

## Review Evidence

Manual boundary review found the M80 change is limited to:

- adding non-negative checks to mock drainage and river accept helper flow fields;
- adding unit tests for negative accepted-flow rejection;
- this evidence record.

The change does not include or link third-party engine code, does not expose native SWMM/D-Flow FM/BMI types to `CouplingLib core`, and does not implement shared arbitration, flow limits, deficit ledger behavior, rollback/replay, scheduler process control, or release/merge gate handling outside their normative owners.

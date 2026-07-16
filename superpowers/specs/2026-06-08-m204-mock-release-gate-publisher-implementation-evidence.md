# M204 Mock Release-Gate Publisher Implementation Evidence

## Scope

M204 records implementation evidence for the mock-only external release-gate publisher boundary designed by M203.

This milestone adds a validation-layer mock publisher only. It does not perform real CI publication, release artifact blocking, external service calls, adapter calls, rollback, replay, mass-audit, or scheduler lifecycle control.

## Implemented Boundary

M204 adds `publish_mock_release_gate_action(...)` under `libs/validation/release_gate/`.

The boundary consumes `FaultControllerSchedulerReleaseGateActionCompletionRecord` plus publisher metadata and supplied mock sink outcome. It returns `MockReleaseGatePublisherResult`.

## Implemented DTOs

M204 adds:

- `MockReleaseGatePublisherStatus`;
- `MockReleaseGatePublisherReason`;
- `MockReleaseGatePublishedActionKind`;
- `MockReleaseGatePublisherRequest`;
- `MockReleaseGatePublisherResult`.

The result preserves action kind, requested control kind, target phase, mutation generation before/after, evidence record identifier, policy reference identifier, action reference identifier, action transaction identifier, release target reference, idempotency key, payload reference/hash, review/block/abort flags, publish flags, external-publish-performed flag, and adapter-call-attempted flag.

## Placement Evidence

The implementation is outside CouplingLib core:

- `libs/validation/release_gate/include/validation/release_gate/mock_publisher.hpp`;
- `libs/validation/release_gate/src/mock_publisher.cpp`;
- `tests/unit/validation/release_gate/test_validation_release_gate_mock_publisher.cpp`.

CouplingLib core does not link against the validation publisher. The validation publisher links to CouplingLib core only to consume completion record DTOs.

## Focused Coverage

The focused tests cover:

- incomplete completion blocks;
- missing publisher boundary blocks;
- missing operator approval blocks;
- missing project policy, release target, idempotency key, action transaction, payload reference, and payload hash block;
- missing publish mode and conflicting dry-run/commit modes block;
- review-required completion maps to review required;
- contradictory action flags map to review required;
- dry-run records without publish attempt;
- commit success records supplied publish success without real external publish;
- commit failure records supplied failure without real external publish;
- pass/no-action records no blocking action.

## Focused Local Evidence

Command:

```text
cmake --build build/windows-msvc --target test_validation_release_gate_mock_publisher && ctest --test-dir build/windows-msvc -C Debug -R "test_validation_release_gate_mock_publisher|test_coupling_real_executor_release_gate_completion" --output-on-failure
```

Result:

```text
scau_coupling_core.vcxproj -> H:\githubcode\SCAU-UFM\build\windows-msvc\libs\coupling\core\Debug\scau_coupling_core.lib
scau_validation_release_gate.vcxproj -> H:\githubcode\SCAU-UFM\build\windows-msvc\libs\validation\release_gate\Debug\scau_validation_release_gate.lib
test_validation_release_gate_mock_publisher.vcxproj -> H:\githubcode\SCAU-UFM\build\windows-msvc\tests\unit\validation\release_gate\Debug\test_validation_release_gate_mock_publisher.exe
Test project H:/githubcode/SCAU-UFM/build/windows-msvc
    Start  98: test_coupling_real_executor_release_gate_completion
1/2 Test  #98: test_coupling_real_executor_release_gate_completion ...   Passed    0.24 sec
    Start 108: test_validation_release_gate_mock_publisher
2/2 Test #108: test_validation_release_gate_mock_publisher ...........   Passed    0.51 sec

100% tests passed, 0 tests failed out of 2

Total Test time (real) =   1.02 sec
```

## Confirmed Negative Boundary

M204 does not add:

- real CI integration;
- real release artifact blocking;
- external service publication;
- GitHub Actions status publication;
- artifact registry mutation;
- SWMM process, handle, C API, or adapter calls;
- D-Flow FM / BMI process, handle, or adapter calls;
- rollback execution;
- replay execution;
- mass-audit execution;
- scheduler lifecycle/process control;
- GoldenSuite, release, or merge readiness evidence.

## CI Evidence

Pending. Record the push commit and GitHub Actions run after the M204 implementation/evidence changes are pushed.

## Decision

M204 establishes a mock-only external release-gate publisher result boundary outside CouplingLib core. It records dry-run/commit outcomes from supplied mock sink state while keeping real CI/release publication outside this repository boundary.

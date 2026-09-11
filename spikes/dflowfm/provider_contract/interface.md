# D-Flow FM provider contract (C3/B4')

Status: incomplete proposal; NOT validated contract evidence. The interrupted authoring task produced this draft only. Existing native water-balance observations do not establish the complete C3 hydraulic boundary mapping contract.

## Contract boundary

The provider is a driver-owned observation boundary over a concrete, governed D-Flow FM runtime. It must expose one snapshot per initialized runtime epoch:

- `provider_id`: stable provider identity;
- `provider_version`: provider/runtime version string;
- `capability_schema_version`: integer capability schema version;
- `capabilities`: the explicitly supported observation classes;
- `observations`: cumulative values since the most recent `initialize` (or restart reload);
- `trace`: source and version evidence sufficient to reproduce the observation.

The provider does not own `Q_limit`, `V_limit`, deficit, rollback, replay, arbitration, or coupling scheduling. Those remain CouplingLib/driver semantics.

## Required capability

A C3 provider must declare the following capability before a mapping can be consumed:

`dflowfm.external_net.v1`

That capability covers storage plus cumulative `boundary_in_m3`, `boundary_out_m3`, and API lateral classes. API lateral volume is removed once from the external-net audit because CouplingLib already accounts for the exchange. The snapshot must also prove that unsupported forcing classes (`qext`, rain/evaporation, groundwater, and file-forced lateral input) are zero.

The provider must identify the concrete runtime and version. A missing identity, version, capability, or trace is invalid even when numeric observations are present.

## Fail-closed rules

Validation rejects the case before runtime linking when:

1. no provider record is supplied;
2. the required capability is absent or its version is incompatible;
3. provider identity/version/trace is missing;
4. an observation is missing, non-finite, or has an invalid sign;
5. an unsupported forcing class is nonzero;
6. observations cross an initialize boundary without a new baseline.

A rejected provider cannot be downgraded to `provider_required` silently and cannot produce a runnable river link. Geometry-only N2 authoring remains valid independently of this contract.

## Evidence status

The executable native-boundary tests now use `tests/unit/coupling/fake_dflowfm_bmi.cpp` and `test_coupling_dflowfm_engine.cpp`. They validate the existing C ABI, not the proposed JSON schema above. The fixture selectors `valid_provider`, `wrong_abi`, `wrong_size`, `missing_capability`, `read_failed`, `stale_time`, and `negative_storage` drive actual dynamic-library calls. `validation_report.md` records results and the remaining C3 mapping gap. No C3 completion claim is justified.

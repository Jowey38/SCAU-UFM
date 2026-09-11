# Provider native-boundary validation

Scope: existing `dflowfm_get_water_balance_v1` C ABI; this is partial C3/B4' evidence. The proposed JSON metadata in this directory is not an executable provider schema and does not activate river mappings.

## Reproduction

```sh
cmake --build H:/scau-close-build --config Debug --parallel 1 --target test_coupling_dflowfm_engine
ctest --test-dir H:/scau-close-build -C Debug -R '^(test_coupling_dflowfm_engine|test_dflowfm_external_net_provider)$' --output-on-failure
```

MSVC Debug: 2/2 CTest registrations passed on 2026-09-10 session.

| Contract case | Actual behavior |
|---|---|
| Missing runtime library | Initialize fails and engine remains uninitialized |
| Missing required BMI symbol | Initialize fails |
| ABI version 2 instead of 1 | Native observation throws |
| Incorrect struct_size | Native observation throws |
| valid_components 0xFE instead of required 0xFF | Native observation throws |
| Bridge return code 7 | Native observation throws |
| Observation timestamp differs from engine clock | scope_complete=false |
| Negative physical storage | scope_complete=false |
| Valid ABI/version/capabilities | storage=60 m3; boundary_in=75 m3; boundary_out=25 m3 |
| Advance 30 seconds | Observation clock advances from 100 to 130 seconds |
| Finalize/reinitialize | Observation clock returns to initialization baseline 100 seconds |
| Unsupported forcing class | Existing external-net provider unit cases reject nonzero source/qext/rain/evaporation/groundwater |
| API lateral accounting | Existing derivation cases verify exactly-once removal from audit external net |

Fixtures are compiled into the fake BMI shared library; config strings select malformed snapshots. They are not runnable hydraulic models. No real provider identity or production runtime version is fabricated.

## Remaining C3 work

- Bind provider/runtime identity, build manifest and input hashes to run evidence.
- Freeze and validate hydraulic boundary ID, direction, units, datum, lateral mapping and initialization contract for N2-authored networks.
- Connect validated provider output to coupling_maps and case_export; current code intentionally emits provider_required.
- Execute a governed synthetic river end-to-end and rejection tests through the production mapping/export path.
- Keep unavailable capability or provider fail-closed. Native observation success alone does not open the PreProc river export gate.

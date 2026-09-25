# D-Flow FM provider contract (C3/B4')

Status: **implemented and locally verified** (2026-09-26). The machine-facing
contract lives in the main repository's coupling driver header
`dflowfm_provider_contract.hpp` (outside `spikes/`, per spike-spec isolation);
the run loop freezes it into `run_summary.json`, and `python/scau_results/linked.py`
re-validates that artefact independently. This page describes the frozen
semantics and the evidence; it is not itself the source of truth.

## Scope (deliberately narrow)

The only exchange kind the runtime drives through the engine is the
CouplingLib-mediated API lateral (`laterals/<id>/water_discharge`). The contract
therefore freezes **per-boundary identity for API laterals only**. Open river
boundaries are observed **in aggregate** through the native water balance
(`boundary_in_m3` / `boundary_out_m3`) and carry no per-object identity. Any
other exchange kind is `dflowfm_contract_exchange_kind_unsupported`.

## Frozen identity

```text
provider_id                = "dflowfm"
capability_schema_version  = "dflowfm.external_net.v1"
boundary:
  boundary_id        case-owned native lateral id  (config surface_river_link.lateral_id)
  provider_object_id engine-side location id       (config surface_river_link.location)
  surface_cell       coupled 2D cell
  exchange_kind      "api_lateral"
case identity:
  source_stcf_hash   FNV-1a64 over the STCF bytes
  dflowfm_mdu_hash   FNV-1a64 over the MDU bytes (REQUIRED when a native observation is bound)
time base:
  t[i] = start_time + (i + 1) * dt_couple   (seconds, model logical time)
units:
  volume m3, time s
flux convention (native block):
  boundary_in_m3 / boundary_out_m3 : into / out of the river domain, cumulative since initialize
  api_lateral_in_m3 / api_lateral_out_m3 : CouplingLib-driven laterals, cumulative since initialize
```

`boundary_id` and `provider_object_id` are each unique across the run; two
links with the same name but different objects, or the same object under two
names, are rejected **from the config alone, before any case load or engine call**.

## Fail-closed rules (reason codes)

| Condition | Reason code | Where |
|---|---|---|
| provider id not `dflowfm` | `dflowfm_contract_provider_mismatch` | C++ / Python |
| capability not `dflowfm.external_net.v1` | `dflowfm_contract_capability_unsupported` | C++ / Python |
| STCF hash missing; MDU hash missing with native scope | `dflowfm_contract_case_identity_missing` | C++ / Python |
| `dt_couple <= 0` or non-finite time base | `dflowfm_contract_time_base_invalid` | C++ |
| empty `boundary_id`, negative object id | `dflowfm_contract_boundary_identity_missing/invalid` | C++ / Python |
| duplicate `boundary_id` / `provider_object_id` | `dflowfm_contract_boundary_id_duplicate` / `dflowfm_contract_provider_object_duplicate` | C++ / Python |
| exchange kind other than `api_lateral` | `dflowfm_contract_exchange_kind_unsupported` | C++ / Python |
| native epoch off the time base, duplicate, reversed | `dflowfm_contract_native_time_invalid` | C++ (commit gate) / Python (ledger load) |
| cumulative gross class decreased | `dflowfm_contract_native_monotonicity_violated` | C++ / Python |
| non-finite or negative gross class | `dflowfm_contract_native_observation_invalid` | C++ / Python |
| native record missing at a committed epoch | `dflowfm_contract_native_observation_missing` | Python |
| river ledger link not a frozen boundary / identity triple differs | `LINKAGE_REJECTED` | Python |
| MDU bytes differ from the recorded hash | `LINKAGE_REJECTED` | Python |

In the run loop a native-series violation lands in `review_required` with
`refused_engine_rollback`; the offending epoch is not committed. Unsupported
forcing classes (`source`, `qext`, rain/evaporation, groundwater) remain
governed by the M276 external-net provider (`dflowfm_external_unproven_class_nonzero`).

## Accounting (C3-08)

`native api_lateral (in - out)` and `ledger river (granted + repay - returned)`
are the same intended quantity integrated by two parties. The linker states
both definitions and reports `NO_GAP` (≤ 1e-9 relative) or
`LATERAL_INTEGRATION_GAP`; it never corrects either side. Recorded real value
(G19 and the `scau_sim` run below): `1.57317293025994 m3` on both sides, gap 0.

## Evidence

- Unit: `tests/unit/coupling/test_dflowfm_provider_contract_unit.cpp` (8 cases).
- Run loop (mock engines): `tests/integration/sim_driver/test_sim_driver_run_loop.cpp`
  — identity emission + ledger tracking, series violation → review_required,
  native scope without MDU bytes rejected, duplicate identity rejected before running.
- Golden **G39** `tests/golden/dflowfm_provider_contract/` (CI gate): driver artefact →
  independent Python linker → `provenance_validated`, `NO_GAP`, tampered MDU refused.
- Python: `python/tests/test_results_linked.py` (15 rejection sub-cases, accounting, statuses).
- Real engines: G19 gateway asserts native api-lateral == ledger total (8/8 gateway
  pass, 2026-09-25); a real `scau_sim` surface + SWMM + D-Flow FM run (mixed-minimal
  STCF, `swmm_river_datum.inp`, `single_reach.mdu`, 3 × 60 s, whole-system audit
  `conserved`) produced a summary that `scau_results link --result-manifest
  --swmm-report --dflowfm-mdu` bound three ways; QGIS 4.0.0 offscreen smoke
  (`--c3-summary/--c3-mdu`) shows `C3Contract` / `DFlowNative
  provenance_validated` / `DFlowLateralAccounting:NO_GAP` and refuses a
  tampered MDU.

Older evidence (`validation_report.md`, `fake_dflowfm_bmi.cpp`) covers the
native C ABI layer and remains valid for that layer.

## Not covered by this contract

- Per-object identity for open river boundaries (aggregate only).
- PreProc/GIS authoring of `surface_river_link` from river geometry (N2 chain);
  the contract validates what the config declares, it does not derive it.
- Native `*_his.nc` / `*_map.nc` time-series parsing; the native view is the
  engine water balance recorded at each committed epoch.

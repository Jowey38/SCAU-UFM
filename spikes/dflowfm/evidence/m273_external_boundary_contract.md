# M273 D-Flow FM External-Boundary Contract Spike

Conclusion level: **FAILED / BLOCKED**

No production D-Flow FM external-net provider is justified by the available BMI 1.0 evidence.

## Runtime and case

| Item | Value |
|---|---|
| Date | 2026-08-08 |
| Runtime | `Delft3D-main/install_fm-suite_release/bin/dflowfm.dll` |
| Host | standalone `spikes/dflowfm/host/dflowfm_spike_host.cpp`, Release, VS 17 2022 x64, build `H:/scau-vb273` |
| Geometry | project-authored `single_reach_1d/single_reach_net.nc` |
| Open case | upstream `dischargebnd=0.125 m3/s`, downstream `waterlevelbnd=1.0 m` |
| Control | upstream `dischargebnd=0.0 m3/s`, same downstream stage |
| Stepping | 10 x `update(60.0)`; both traces report `max_dt_abs_error=0` |

D-Flow FM initialized both cases and explicitly reported one open boundary cell for `dischargebnd upstream` and one for `waterlevelbnd downstream`.

## Raw evidence

Committed traces:

- `spikes/dflowfm/cases/single_reach_open_boundary/m273_external_boundary_control.trace.txt`
- `spikes/dflowfm/cases/single_reach_open_boundary/m273_external_boundary_indexed.trace.txt`
- `spikes/dflowfm/cases/single_reach_open_boundary/m273_boundary_map.trace.txt`
- `spikes/dflowfm/cases/single_reach_open_boundary/m273_boundary_dt30.trace.txt`
- `spikes/dflowfm/cases/single_reach_open_boundary/m273_boundary_dt120.trace.txt`

### Storage response

The control remains at `sum(vol1)=6500 m3` for all 10 steps. In the forced case:

| step | time (s) | sum(vol1) (m3) | delta from initial (m3) |
|---|---:|---:|---:|
| 0 | 0 | 6500.000000000000 | 0.000000000000 |
| 1 | 60 | 6511.030547108130 | 11.030547108130 |
| 5 | 300 | 6538.983737618280 | 38.983737618280 |
| 10 | 600 | 6521.060340460530 | 21.060340460530 |

The prescribed upstream input is `0.125 * 600 = 75 m3`. The final storage gain is only `21.06034046053 m3`; the remaining net volume crosses the downstream stage boundary. This is physically plausible and failure-revealing: storage change alone cannot identify cumulative external net flux.

### BMI candidate variables

`qext`, `qextreal`, and `vextcum` remain `unavailable` at initialization and after every update in both the control and open-boundary cases. Their BMI inventory presence does not provide runtime data for `[boundary]` forcings.

`q1` is readable as a rank-1 array of length 12. The forced trace shows:

- `q1[11] = +0.125` at every sampled step;
- `q1[10]` evolves from approximately `-2.24e-5` to `-0.236817`;
- indices 0 through 9 are internal-link flows.

A follow-up indexed topology trace also reads `kcu` and `ln`:

- `kcu[0..9] = 1` for internal 1D links;
- `kcu[10] = kcu[11] = -1` for 1D boundary links;
- `ln[10] = (12, 11)` and `ln[11] = (13, 1)` in this runtime's 1-based native node administration.

This is useful case-local evidence, but not yet a governed production contract:

1. The case-local `kcu/ln` mapping identifies native boundary links, but no production adapter contract maps those native pairs back to every authored boundary ID and boundary quantity across arbitrary networks.
2. `q1` positive direction is topology-relative, not an external-inflow-positive audit convention; the adapter still needs a version-governed orientation rule for each boundary quantity.
3. The runtime does not honor every caller timestep. With `--dt 30`, the engine advances by its internal 60 s and the host reports `time_trace_valid=false`; with `--dt 120`, it advances by 120 s. A provider cannot integrate using requested dt without first auditing the actual engine dt.
4. End-of-step `q1` samples do not integrate the first-step storage change. At step 1, the boundary-like endpoint values are `+0.125` and approximately `-2.24e-5 m3/s`, while `delta sum(vol1)=11.030547 m3`, so a rectangular 60 s integration of sampled endpoint values does not close storage.
5. No restart/reload proof exists for cumulative boundary volume or integration state.
6. BMI 1.0 exposes no units query; SI units here come from the authored forcing file and case evidence, not a general runtime contract.

## Decision

M273 stops at the spike boundary. It does **not** add:

- `DFlowFMExternalNetProvider` or equivalent production code;
- `IDFlowFMEngine` external-flux methods;
- a SimDriver `dflowfm_external_net_volume` binding;
- G19 promotion or a new gating GoldenTest.

The real whole-system audit must remain `REVIEW_REQUIRED` when complete D-Flow FM external-net scope is absent.

## Evidence required to unblock production

At least one of the following must be demonstrated and version-governed:

1. an engine-supported cumulative boundary-volume API covering all external boundary types with external-inflow-positive sign and restart semantics; or
2. a complete boundary ID to native flow-link mapping plus orientation, time-integration convention, all boundary/source classes, and replay-safe cumulative state, validated against `delta sum(vol1)` across inflow, outflow, mixed-boundary, and restart cases.

Until then, deriving external net as `delta sum(vol1)` or summing `q1` is prohibited because it would either be circular or include internal fluxes.

# M288 failure-revealing design review

Status: implementation entry review, 2026-09-10. This records an inline review; no independent subagent review completed (API 402). It does not certify M288-A/B/C as implemented.

## Input readiness

B1/B2' selected surface+SWMM execution. PR #107 adds this path and a real 180-second / three-epoch synthetic run without a D-Flow DLL. Its output currently contains run_summary.json only: committed epochs and audit metadata do not provide per-cell hydrodynamic time series. C3 hydraulic boundary/provider evidence remains incomplete. Therefore dual-mode output authoring can proceed on its own defined scope, while three-model result integration remains gated by C3.

## Reviewed design

Place a default-disabled surface timeseries writer at the committed epoch boundary in SimDriver, after successful exchange write-back, checkpoint validation, and enabled mass audit. Never write rejected or partially advanced epochs as committed observations. The writer owns output only and receives const state/geometry; it must not mutate numerical state or call engines.

Use CF/UGRID topology with time and cell dimensions, h/eta/hu/hv/wet_mask, explicit units, CRS, stable cell indices, bed elevations, selected frequency, schema version, and engine participation. The run manifest records input STCF/config hashes and enabled-engine identities. A finished manifest references output content hashes only after successful close. Partial files remain identifiable and unusable as completed runs.

Frequency is an integer multiple of coupling epochs and explicit in RuntimeConfig. Disabled mode creates no files, hashes no state for output, and leaves the previous runtime path unchanged. A failure while writing after engine advancement must stop with a visible output failure; it cannot claim an engine rollback occurred.

M288-B reads validated immutable output, computes max depth, threshold extent, arrival/duration, and selected point/profile series, then exports NetCDF/GeoTIFF with provenance. Arrival and duration require explicit threshold/interpolation conventions; no-data and never-wet are distinct from time zero. P9 delegates these checks to pure result helpers and uses read-only rendering.

## Required adversarial checks

| Trigger | Expected evidence |
|---|---|
| Writer disabled | No output; final state hash, epoch results and audit counters bitwise equal to prior path |
| Surface output missing | Refuse results loading with missing input path |
| Truncated NetCDF or missing h/eta/hu/hv/wet_mask | Reject schema before deriving a map |
| Non-finite depth/momentum, negative depth, inconsistent cell count | Reject input with variable and timestep context |
| Repeated/decreasing time or engine time mismatch | Refuse temporal combination |
| Unknown units/CRS or incompatible datum | Refuse overlay/comparison; never infer a conversion |
| Stale schema | Explicit migration-required result; preserve original |
| Manifest hash mismatch | Refuse result loading even if NetCDF parses |
| SWMM node ID absent | Linkage rejection naming the missing node |
| River requested without provider/capability | provider_required or explicit capability failure; no invented boundary |
| Failure before epoch commit | No frame for the failed epoch |
| Write failure after commit/engine advance | Visible incomplete-output outcome; no silent success or fictitious rollback |
| Repeated postprocessing | Same input/parameters produce identical derived arrays and deterministic artifacts |
| Never-wet or threshold tie | Defined nodata/arrival/duration values verified against analytic series |
| Illegal mapped field | Reject before canonical pipeline entry |

## Implementation sequence and acceptance

1. Freeze runtime output config and machine-readable schema with units, time semantics and participation.
2. Implement writer with staged finalization, input provenance and readback validation; exercise analytic one-cell and mixed-mesh cases.
3. Register default-off parity and writer roundtrip checks in CI/GoldenSuite under an available G ID.
4. Implement deterministic result CLI and analytic arrival/duration tests.
5. Integrate P9 after CLI validation exists; run QGIS version matrix.
6. Add native SWMM/D-Flow result linkage only with confirmed ID, time, unit, and provider contracts.

No result writer or result CLI is delivered by this review. M288 stays incomplete until the executable tests above pass.

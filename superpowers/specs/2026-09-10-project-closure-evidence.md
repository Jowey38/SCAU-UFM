# Project closure evidence index

Updated 2026-09-26.

## Closed

- A6 documentation: PR #104 merged as `75dcd9d`.
- E6/E7, N1/N2, P10/P11 packaging and workbenches: PR #106 merged after eight checks; current master then advanced through later closure PRs.
- B7 cross-platform G30: PR #105 merged as `78f8d12`; run `34462940790` passed all nine checks. Ubuntu artifact `b7-evidence-linux-x86_64` (`10146309017`) reports same-host bitwise reruns and reference/candidate SHA `a89ba98da95bcd5732c17e1bf9977e8f75bdc1d81b96a6844333fc796e911bc4`.
- B1/B2' surface+SWMM mode: PR #107 merged; nine CI checks passed. The local driver slice passed 5/5; real SWMM executed 180 seconds / three epochs with a deliberately missing D-Flow DLL. Disabled river has no lifecycle, state, audit-provider or checkpoint calls.
- Native D-Flow provider rejection coverage: PR #108 merged as `ce9944e`; nine CI checks passed. Dynamic fixture tests cover missing runtime/symbol, ABI version/size/capability mismatch, bridge read failure, stale time, negative storage, valid observation and reinitialization.

## Partial / open

- M288-A/B/C results line: PR #109 (`c7d47fe`), all nine CI checks green on the final head including real-dflowfm-golden. Delivered in order: (1) provenance manifest — FNV-1a64 source-STCF hash, final surface state hash and output byte hash cross-checked three ways, with `run_summary.json` as an independent fourth; (2) spatial sampling by even-odd point-in-cell over preserved UGRID connectivity, fail-closed on geographic/undeclared frames; (3) dependency-free baseline GeoTIFF export with CRS taken only from a `case_sha256`-verified pipeline manifest or `--crs`, read back with GDAL 3.12.2; (4) P9 thin-shell page in QGIS 4.0.0 offscreen smoke incl. byte-tamper rejection; (5) 1D/2D linked view from per-link gross ledger records joined to SWMM `.rpt` node summaries by name, bound to the surface result by state hash. Python 109/109; driver CTest 5/5. Still open and explicitly documented in `python/scau_results/README.md`: runtime config hash in manifest, CRS identifier inside the STCF itself, compressed/tiled GeoTIFF, QGIS 3.40 track, release-gate registration (D-Flow native result linkage now delivered through C3, see below).
- C3/B4' provider contract: **PR #110: CI run `36164654444` passed all nine checks on `6830ce7` (real-dflowfm-golden reproduced `[g19-c3] native_api_lateral_in=1.57317293025994 == ledger`; gpu-cuda-golden green).** Frozen contract in `libs/coupling/driver/.../dflowfm_provider_contract.hpp` (identity, uniqueness, `api_lateral`-only, MDU byte hash with native scope, fixed-dt time base, cumulative monotonicity; `dflowfm_contract_*` reason codes). Run loop validates identity from the config before any case load and the native series inside the commit gate; `run_summary.json` carries the `dflowfm` provenance block and per-epoch `dflowfm_native`. `scau_results link --dflowfm-mdu` re-validates independently and reports C3-08 accounting (`NO_GAP` / `LATERAL_INTEGRATION_GAP`, never corrected). Evidence: G39 golden (CI gate, producer→consumer on one artefact incl. tampered-MDU refusal); unit 8, run-loop 4, Python 15 rejection sub-cases; local CTest 167/167 + real gateway 8/8 (G19 asserts native api-lateral 1.57317293025994 m³ == ledger); real `scau_sim` surface+SWMM+D-Flow run linked three ways to `provenance_validated`; QGIS 4.0.0 offscreen smoke `c3_linked_view ok`. Scope narrowed explicitly: open river boundaries are aggregate-only; PreProc river-geometry authoring (N2) and native `*_his.nc` parsing are outside the contract. `dflowfm_native: not consumed` no longer exists in code.
- M287-D: blocked by M281. Committed sample and decision record state that no authorized upstream city sample or format contract exists; synthetic fixtures do not unblock it.
- QGIS 3.40 LTR: environment absent. QGIS 4.0.0 headless smoke passed; no 3.40 pass claim is made.

## Release gate

Release is not complete while PR #109 is draft and C3, M287-D, QGIS 3.40, or required M288 acceptance evidence remains open. The task list in `开发收口任务清单.md` is the operational checklist; this index is the evidence boundary.

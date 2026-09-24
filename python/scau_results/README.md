# Surface results prototype

Status: partial M288-A/B implementation, not release-ready.

Enable writer with `surface_timeseries_path = <new-file.nc>` and optionally `surface_output_every_epochs = 3` in RuntimeConfig v2. Empty/absent path disables the writer. It preserves the input STCF topology, adds h/eta/hu/hv/wet_mask over model logical time, and writes the initial frame plus selected committed epochs and the final epoch. Time is logical seconds, not a civil date. Publication is an artifact-set protocol: every destination (`.nc`, `.manifest.json` and both `.partial` staging names) is preflighted **before any engine advances**; at completion the manifest is written to a staging file, flushed, closed and re-read for equality, then the NetCDF is published by same-directory hard link (atomic, never overwrites) and the manifest by same-directory rename; if the manifest publish fails the NetCDF is withdrawn, so the set is never half-published.

```sh
PYTHONPATH=python python -m scau_results run.nc --threshold 0.01 --output derived.json
```

Provenance (schema 2): the writer records `source_stcf_hash` (FNV-1a64 over the input STCF bytes, repo hash convention) and, at completion, `final_surface_state_hash` (the same `hash_surface_state` the run summary reports) and `committed_epochs` as global attributes, then publishes a sidecar `<output>.manifest.json` carrying those values plus `output_hash` over the published bytes. The CLI refuses to read a result unless: the manifest exists, its `output_hash` matches the bytes actually read, the manifest and NetCDF attributes agree, and the source STCF still exists with unchanged bytes. `run_summary.json` independently reports the same `final_surface_state_hash`, giving a third cross-check.

Reads one immutable byte snapshot, checks completion/schema/units/time order and finite fields, then writes a new JSON file with source/source-STCF hashes, per-cell maximum depth, first sampled threshold crossing, and duration using left-sample piecewise-constant intervals. Never-wet arrival is null. Output is deterministic for identical input and threshold.

`--geotiff-dir DIR --pixel-size N [--crs EPSG:xxxx]` rasterizes `max_depth_m`, `arrival_time_s` and `duration_s` into baseline little-endian Float32 GeoTIFFs (single strip, uncompressed, PixelIsArea, `GDAL_NODATA=nan`) written by a dependency-free writer in `scau_results/geotiff.py`. Pixel centres are located to cells with the same even-odd/boundary/lowest-index rule as `--points`; uncovered pixels and never-wet arrival are both NaN (disambiguate via `duration_s == 0`). Export is all-or-nothing: every request parameter (points, cells, CRS, pixel size, field length, float32 range) is validated **before** any file is written; rasters are staged in a sibling temp directory and published by a single directory rename; an existing destination directory is a conflict, never a merge target; a rejected request leaves no `.tif` behind. Identical inputs produce byte-identical rasters.

CRS resolution is fail-closed and never invented: an EPSG code is written only if `--crs` is given or a PreProc `pipeline_manifest.json` is found beside the source STCF **and** its `case_sha256` matches the source bytes; explicit and discovered values must agree (compatibility error otherwise). The CRS must be a **2-D projected CRS with metre axes** (`pyproj` `is_projected`, exactly two axes, `unit_conversion_factor == 1`); geographic, geocentric (e.g. EPSG:4978), vertical (EPSG:5703), foot-based (EPSG:2263) and non-EPSG CRSs are rejected, since the mesh and pixel size are metres. Absent both, the raster is unreferenced and the report says `crs_source: undeclared`. Verified read-back with GDAL 3.12.2 (QGIS 4.0.0): EPSG:3395 golden → `epsg=3395`, correct geotransform, NaN nodata; synthetic export package → `epsg=null` as expected since that package has no CRS governance.

`--points x1 y1 x2 y2 ...` locates each point to a cell by even-odd point-in-polygon over the preserved UGRID face connectivity (boundary points count as inside; a point on a shared edge resolves deterministically to the lowest cell index) and emits `sampled` plus the `series` for the located cells. Coordinates are interpreted in the mesh's own projected metres: the result file carries `projection_x/y_coordinate` in `m` but **no CRS identifier**, so no transformation is attempted and the output labels the coordinate system accordingly. Geographic (degree) meshes, meshes without connectivity, non-finite input, and points outside every face are rejected as linkage failures. `--points` and `--cells` are mutually exclusive.

`--cells 379 50` adds `series`: full h/eta/hu/hv rows for the requested cells in the requested order (point series; a transect-ordered list yields a profile). Empty, duplicate, boolean or out-of-range indices are rejected as linkage failures. No spatial lookup is performed; cell indices must come from the case mesh.

The 180 s synthetic surface+SWMM example run has the whole-system audit disabled; its extracted series demonstrate the pipeline only and are not physical validation evidence.

Local evidence: writer parity/roundtrip integration passes; actual surface+real SWMM run produced four frames across 180 seconds; Python tests cover analytic arrival/duration and malformed result rejection.

Remaining requirements: runtime config text is not yet hashed into the manifest (only the STCF and the resulting state are); strict CF time-reference decision and independent format interoperability validation; configured variable-set contract; full disabled-mode GoldenSuite comparison; CRS identifier propagation from PreProc into the STCF/result file itself (today CRS is recovered only via a hash-verified sidecar pipeline manifest or `--crs`); compressed/tiled GeoTIFF variants; P9; native 1D result linkage; and release CI. These gaps must remain visible; do not claim full M288 completion from this prototype.

## 1D/2D linked view (`link` subcommand)

```sh
PYTHONPATH=python python -m scau_results link run_summary.json --swmm-report run.rpt --result-manifest run.nc.manifest.json --output linked.json
```

The driver records, per committed epoch, one `link_exchanges` entry per 2D<->1D link: engine, engine-side node id, config-level `node_name`, surface `cell`, and the CouplingLib ledger volumes `v_granted` / `v_repay` / `v_returned`. These are **gross** ledger movements; the epoch's `drained_volume`/`returned_volume` remain **net** per-cell write-back deltas. The run summary also carries the run's identity: `source_stcf_hash`, `swmm_inp_hash`, `swmm_report_path`/`swmm_report_hash` (hashed after `finalize()` closes the report), `start_time`, `dt_couple`.

**Every input must prove it belongs to this run before it is joined:**

- The summary itself is validated before aggregation: required identity fields present (never `None == None`), `outcome == completed`, `len(epochs) == committed_epochs`, `logical_time == start + (i+1)·dt_couple` and strictly increasing, no duplicate link per epoch, all volumes finite and ≥ 0, node/cell identity stable across epochs, and per-epoch **gross − returned == net write-back** plus the same invariant over run totals.
- `--result-manifest <run.nc>.manifest.json` runs the **full** `summarize` validation on the result file beside it (manifest schema, output bytes, source STCF bytes, NetCDF attributes), then requires `final_surface_state_hash`, `source_stcf_hash` and `committed_epochs` to match the summary. A manifest without its result, or from another run, is a linkage failure.
- `--swmm-report` must have byte hash equal to the summary's `swmm_report_hash`. A mock run (no report) cannot bind any report; a same-named node in a different run's report is refused.
- Report tables must **declare** SI units in their headers (`Meters`, `CMS`, `10^6 ltr`); US-unit or unit-less headers are rejected, never assumed. `Continuity Error (%)` is read from inside the `Flow Routing Continuity` section only (the `Runoff Quantity Continuity` value is reported separately). SWMM writes volumes with `%12.3g` — **three significant figures**, not a fixed 1 m³.

Per-link `gap` is a **diagnostic, never a correction**: `signed_gap_m3 = rpt lateral − ledger in`, `relative_gap`, and `rpt_rounding_half_ulp_m3` (half a unit in the report's 3rd significant figure). `diagnostic_code` is `NO_GAP` when the gap is within that rounding, otherwise `ROUTING_GAP_ATTRIBUTION_INSUFFICIENT`. The only stronger code, `KNOWN_ROUTING_CONTINUITY_GAP`, is **not yet emitted**: it requires a known case, known SWMM version, verified initial dry-conduit state, matching temporal pattern, valid provenance **and** a reconciled accounting definition — SWMM's node lateral total is a trapezoidal integral of `oldLatFlow/newLatFlow` (`stats.c`) while the ledger is per-epoch `granted + repay`; these have not been reconciled. The global routing continuity error is reported with an explicit `scope_note` that it is **not** evidence about any single node's gap. Original volumes are always preserved; nothing is reconciled, corrected, hidden or turned into a validation pass.

On the 180 s synthetic surface+SWMM run (bound report, SWMM 5.2.4): J1 +1.76 m³ (+0.46 %), J2 +13.46 m³ (+4.81 %), both `ROUTING_GAP_ATTRIBUTION_INSUFFICIENT`; routing continuity error −6.627 %. An earlier revision of this file attributed the J2 gap to the engine's initially-dry-conduit behaviour (bug-209); that claim is withdrawn as unsupported by the evidence available.

D-Flow FM native results are deliberately **not** consumed (`dflowfm_native: not consumed`) until the C3 provider contract closes; river links still appear from the ledger with `engine_native: null`.

### P9 canvas picking

Canvas coordinates are in the QGIS **project** CRS; the sampler requires the **model** CRS. The page resolves the model CRS the same way GeoTIFF export does (a `case_sha256`-verified pipeline manifest beside the source STCF) and transforms picked points into it; when no governed model CRS exists, canvas picking is **refused** and the operator must type coordinates in the mesh's own CRS. Points are stored at full precision (`repr`), not truncated.

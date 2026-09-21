# Surface results prototype

Status: partial M288-A/B implementation, not release-ready.

Enable writer with `surface_timeseries_path = <new-file.nc>` and optionally `surface_output_every_epochs = 3` in RuntimeConfig v2. Empty/absent path disables the writer. It preserves the input STCF topology, adds h/eta/hu/hv/wet_mask over model logical time, and writes the initial frame plus selected committed epochs and the final epoch. Time is logical seconds, not a civil date. Output is staged as `.partial`; completion publishes without replacing an existing file, using a same-directory hard link (unsupported filesystems fail closed).

```sh
PYTHONPATH=python python -m scau_results run.nc --threshold 0.01 --output derived.json
```

Provenance (schema 2): the writer records `source_stcf_hash` (FNV-1a64 over the input STCF bytes, repo hash convention) and, at completion, `final_surface_state_hash` (the same `hash_surface_state` the run summary reports) and `committed_epochs` as global attributes, then publishes a sidecar `<output>.manifest.json` carrying those values plus `output_hash` over the published bytes. The CLI refuses to read a result unless: the manifest exists, its `output_hash` matches the bytes actually read, the manifest and NetCDF attributes agree, and the source STCF still exists with unchanged bytes. `run_summary.json` independently reports the same `final_surface_state_hash`, giving a third cross-check.

Reads one immutable byte snapshot, checks completion/schema/units/time order and finite fields, then writes a new JSON file with source/source-STCF hashes, per-cell maximum depth, first sampled threshold crossing, and duration using left-sample piecewise-constant intervals. Never-wet arrival is null. Output is deterministic for identical input and threshold.

`--geotiff-dir DIR --pixel-size N [--crs EPSG:xxxx]` rasterizes `max_depth_m`, `arrival_time_s` and `duration_s` into baseline little-endian Float32 GeoTIFFs (single strip, uncompressed, PixelIsArea, `GDAL_NODATA=nan`) written by a dependency-free writer in `scau_results/geotiff.py`. Pixel centres are located to cells with the same even-odd/boundary/lowest-index rule as `--points`; uncovered pixels and never-wet arrival are both NaN (disambiguate via `duration_s == 0`). Files are never overwritten; identical inputs produce byte-identical rasters.

CRS resolution is fail-closed and never invented: an EPSG code is written only if `--crs` is given or a PreProc `pipeline_manifest.json` is found beside the source STCF **and** its `case_sha256` matches the source bytes; explicit and discovered values must agree (compatibility error otherwise); geographic or non-EPSG CRSs are rejected. Absent both, the raster is unreferenced and the report says `crs_source: undeclared`. Verified read-back with GDAL 3.12.2 (QGIS 4.0.0): EPSG:3395 golden → `epsg=3395`, correct geotransform, NaN nodata; synthetic export package → `epsg=null` as expected since that package has no CRS governance.

`--points x1 y1 x2 y2 ...` locates each point to a cell by even-odd point-in-polygon over the preserved UGRID face connectivity (boundary points count as inside; a point on a shared edge resolves deterministically to the lowest cell index) and emits `sampled` plus the `series` for the located cells. Coordinates are interpreted in the mesh's own projected metres: the result file carries `projection_x/y_coordinate` in `m` but **no CRS identifier**, so no transformation is attempted and the output labels the coordinate system accordingly. Geographic (degree) meshes, meshes without connectivity, non-finite input, and points outside every face are rejected as linkage failures. `--points` and `--cells` are mutually exclusive.

`--cells 379 50` adds `series`: full h/eta/hu/hv rows for the requested cells in the requested order (point series; a transect-ordered list yields a profile). Empty, duplicate, boolean or out-of-range indices are rejected as linkage failures. No spatial lookup is performed; cell indices must come from the case mesh.

The 180 s synthetic surface+SWMM example run has the whole-system audit disabled; its extracted series demonstrate the pipeline only and are not physical validation evidence.

Local evidence: writer parity/roundtrip integration passes; actual surface+real SWMM run produced four frames across 180 seconds; Python tests cover analytic arrival/duration and malformed result rejection.

Remaining requirements: runtime config text is not yet hashed into the manifest (only the STCF and the resulting state are); strict CF time-reference decision and independent format interoperability validation; configured variable-set contract; full disabled-mode GoldenSuite comparison; CRS identifier propagation from PreProc into the STCF/result file itself (today CRS is recovered only via a hash-verified sidecar pipeline manifest or `--crs`); compressed/tiled GeoTIFF variants; P9; native 1D result linkage; and release CI. These gaps must remain visible; do not claim full M288 completion from this prototype.

## 1D/2D linked view (`link` subcommand)

```sh
PYTHONPATH=python python -m scau_results link run_summary.json --swmm-report run.rpt --result-manifest run.nc.manifest.json --output linked.json
```

The driver now records, per committed epoch, one `link_exchanges` entry per 2D<->1D link: engine, engine-side node id, config-level `node_name`, surface `cell`, and the CouplingLib ledger volumes `v_granted` / `v_repay` / `v_returned`. These are **gross** ledger movements; the epoch's `drained_volume`/`returned_volume` remain **net** per-cell write-back deltas (the C++ integration test pins gross−net agreement). `link` aggregates them per link, optionally joins SWMM's own `.rpt` Node Depth / Node Inflow summaries by `node_name` (10⁶ L → m³; the rpt rounds to 1 m³), and reports `lateral_gap_m3 = rpt lateral − ledger in` as a **diagnostic only** — the engine's number never corrects the ledger. `--result-manifest` binds the view to a surface result by `final_surface_state_hash`; a manifest from another run is a linkage failure. Only `completed` runs are linked; summaries without `link_exchanges` (older driver) are rejected.

On the 180 s synthetic surface+SWMM run: J1 gap +1.8 m³, J2 gap +13.5 m³ against a −6.6 % SWMM routing continuity error — the engine's documented initially-dry-network continuity behaviour, surfaced rather than absorbed.

D-Flow FM native results are deliberately **not** consumed (`dflowfm_native: not consumed`) until the C3 provider contract closes; river links still appear from the ledger with `engine_native: null`.

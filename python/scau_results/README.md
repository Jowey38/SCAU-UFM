# Surface results prototype

Status: partial M288-A/B implementation, not release-ready.

Enable writer with `surface_timeseries_path = <new-file.nc>` and optionally `surface_output_every_epochs = 3` in RuntimeConfig v2. Empty/absent path disables the writer. It preserves the input STCF topology, adds h/eta/hu/hv/wet_mask over model logical time, and writes the initial frame plus selected committed epochs and the final epoch. Time is logical seconds, not a civil date. Output is staged as `.partial`; completion publishes without replacing an existing file, using a same-directory hard link (unsupported filesystems fail closed).

```sh
PYTHONPATH=python python -m scau_results run.nc --threshold 0.01 --output derived.json
```

Provenance (schema 2): the writer records `source_stcf_hash` (FNV-1a64 over the input STCF bytes, repo hash convention) and, at completion, `final_surface_state_hash` (the same `hash_surface_state` the run summary reports) and `committed_epochs` as global attributes, then publishes a sidecar `<output>.manifest.json` carrying those values plus `output_hash` over the published bytes. The CLI refuses to read a result unless: the manifest exists, its `output_hash` matches the bytes actually read, the manifest and NetCDF attributes agree, and the source STCF still exists with unchanged bytes. `run_summary.json` independently reports the same `final_surface_state_hash`, giving a third cross-check.

Reads one immutable byte snapshot, checks completion/schema/units/time order and finite fields, then writes a new JSON file with source/source-STCF hashes, per-cell maximum depth, first sampled threshold crossing, and duration using left-sample piecewise-constant intervals. Never-wet arrival is null. Output is deterministic for identical input and threshold.

`--points x1 y1 x2 y2 ...` locates each point to a cell by even-odd point-in-polygon over the preserved UGRID face connectivity (boundary points count as inside; a point on a shared edge resolves deterministically to the lowest cell index) and emits `sampled` plus the `series` for the located cells. Coordinates are interpreted in the mesh's own projected metres: the result file carries `projection_x/y_coordinate` in `m` but **no CRS identifier**, so no transformation is attempted and the output labels the coordinate system accordingly. Geographic (degree) meshes, meshes without connectivity, non-finite input, and points outside every face are rejected as linkage failures. `--points` and `--cells` are mutually exclusive.

`--cells 379 50` adds `series`: full h/eta/hu/hv rows for the requested cells in the requested order (point series; a transect-ordered list yields a profile). Empty, duplicate, boolean or out-of-range indices are rejected as linkage failures. No spatial lookup is performed; cell indices must come from the case mesh.

The 180 s synthetic surface+SWMM example run has the whole-system audit disabled; its extracted series demonstrate the pipeline only and are not physical validation evidence.

Local evidence: writer parity/roundtrip integration passes; actual surface+real SWMM run produced four frames across 180 seconds; Python tests cover analytic arrival/duration and malformed result rejection.

Remaining requirements: runtime config text is not yet hashed into the manifest (only the STCF and the resulting state are); strict CF time-reference decision and independent format interoperability validation; configured variable-set contract; full disabled-mode GoldenSuite comparison; CRS identifier propagation from PreProc into the result file (sampling currently works only in the mesh's undeclared projected frame); GeoTIFF; P9; native 1D result linkage; and release CI. These gaps must remain visible; do not claim full M288 completion from this prototype.

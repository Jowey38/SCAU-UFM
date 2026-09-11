# Surface results prototype

Status: partial M288-A/B implementation, not release-ready.

Enable writer with `surface_timeseries_path = <new-file.nc>` and optionally `surface_output_every_epochs = 3` in RuntimeConfig v2. Empty/absent path disables the writer. It preserves the input STCF topology, adds h/eta/hu/hv/wet_mask over model logical time, and writes the initial frame plus selected committed epochs and the final epoch. Time is logical seconds, not a civil date. Output is staged as `.partial`; completion publishes without replacing an existing file, using a same-directory hard link (unsupported filesystems fail closed).

```sh
PYTHONPATH=python python -m scau_results run.nc --threshold 0.01 --output derived.json
```

Reads one immutable byte snapshot, checks completion/schema/units/time order and finite fields, then writes a new JSON file with source SHA256, per-cell maximum depth, first sampled threshold crossing, and duration using left-sample piecewise-constant intervals. Never-wet arrival is null. Output is deterministic for identical input and threshold.

Local evidence: writer parity/roundtrip integration passes; actual surface+real SWMM run produced four frames across 180 seconds; Python tests cover analytic arrival/duration and malformed result rejection.

Remaining requirements: complete run/config/input hash manifest, strict CF time-reference decision and independent format interoperability validation, configured variable-set contract, full disabled-mode GoldenSuite comparison, GeoTIFF/profile extraction, P9, native 1D result linkage, and release CI. The source_stcf path is not a content-provenance guarantee. These gaps must remain visible; do not claim full M288 completion from this prototype.

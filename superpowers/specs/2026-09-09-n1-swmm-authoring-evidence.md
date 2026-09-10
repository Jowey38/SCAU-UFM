# N1-A/N1-B SWMM authoring evidence

Date: 2026-09-09

Implemented whitelist SWMM INP parsing/semantic comparison and deterministic drainage_network.geojson authoring. The authoring path permits only TITLE, OPTIONS, JUNCTIONS, OUTFALLS, CONDUITS, XSECTIONS, COORDINATES, and REPORT. Subcatchment/raingage/infiltration sections are rejected. Authored output preserves an existing model.inp as model.source.inp; external mode copies the supplied original without rewriting it.

Verification:

- `PYTHONPATH=python py -3 -m unittest python/tests/test_inp_io.py`
- Result: 3 tests passed.

The real-engine `scau_preproc swmm-parse --inp` command is implemented in the preprocessor CLI and is the authoritative parse gate when the embedded SWMM target is available.

# N2-A/N2-B River Authoring Evidence

Date: 2026-09-09

## Scope

This slice implements `river_sketch.geojson` schema v1 validation and governed D-Flow FM 1D authoring. It writes a deterministic UGRID NetCDF network, reads and validates its topology, and emits an MDU template whose hydraulic values remain explicit `TODO_PROVIDER` placeholders. The authoring manifest records hashes, generator version, provider requirements, and a skipped DLL smoke status.

Hydraulic inference, P11 UI, and real DLL loading are intentionally out of scope. A non-empty `provider_required` list is an export blocker (`RiverHydraulicsProviderRequired`); geometry-only authoring remains usable for sandbox and review workflows.

## Verification

`PYTHONPATH=python py -3 -m unittest python/tests/test_dflowfm_author.py`

Result: 2 tests passed.

The tests cover isolated-node rejection, sorted deterministic NetCDF bytes, UGRID read/validation, MDU/manifest generation, and fail-closed readiness without requiring a D-Flow FM DLL.

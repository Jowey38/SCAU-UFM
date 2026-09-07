# pyproj / PROJ license note

pyproj is distributed under the **MIT** license; it bundles the PROJ library
(**MIT / X11-style**, https://proj.org/about.html#license) and the PROJ
coordinate-operation database (mixed permissive terms recorded by PROJ).
Both are license-compatible with the project.

SCAU-UFM usage decision (M287-B2):

1. pyproj is used only in the Python preprocessing layer
   (`python/scau_preproc/crs_governance.py`) to reproject vector inputs and
   SWMM `[COORDINATES]` into the project target CRS. Nothing in `libs/`,
   `apps/`, or `tests/` links PROJ; the C++ side consumes only the governed
   files the stage writes.
2. Determinism contract: the PROJ pipeline string (`Transformer.definition`),
   PROJ and pyproj versions are recorded in `crs_audit.json` and the pipeline
   manifest. A PROJ upgrade that changes a pipeline or its numerics surfaces
   as a fixture SHA change in G37, never silently.
3. Grid-based transformations (datum shift grids) are not used by the
   synthetic template; a policy that needs them must pin the grid files
   (PROJ_DATA) here before use so the transform stays reproducible offline.

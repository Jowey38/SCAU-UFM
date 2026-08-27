# Data and preprocessing quality requirements

The values below are a template. A real provider must confirm project-specific thresholds where marked `TODO_PROVIDER`.

## Fatal checks

- Dataset authorization is absent or false.
- Source or target CRS is missing.
- Horizontal/vertical units or vertical datum are missing.
- Required file is missing or cannot be opened.
- Computational boundary is not closed or is empty.
- Geometry is invalid and no approved repair rule exists.
- DEM has NoData inside a required calculation area without an approved fill rule.
- SWMM mapping references a node ID absent from the supplied model.
- A D-Flow FM mapping references an unknown boundary ID.
- Mesh has invalid indices, duplicate edges, non-manifold edges, zero-area cells, or inconsistent face/edge connectivity.
- Generated canonical fields have inconsistent lengths or invalid ranges.

## Review checks

- Geometry was repaired according to an approved rule.
- Building polygons overlap.
- Building/roof drain relation was inferred spatially rather than explicitly supplied.
- Surface/inlet/SWMM relation was inferred by proximity.
- Elevation difference between a surface cell and an exchange node exceeds `TODO_PROVIDER`.
- A land-cover class uses a default Manning value.
- A soil zone uses a default parameter.
- A mesh cell violates the current review thresholds (minimum angle 10 degrees or maximum aspect ratio 20) without being fatal.
- D-Flow FM boundary direction or type was inferred.

## Required reports

The real importer must emit:

- source inventory and provenance;
- CRS and unit conversion report;
- geometry validation and repair report;
- mesh topology and quality report;
- field coverage and range report;
- surface-to-SWMM mapping report;
- roof-to-SWMM mapping report;
- optional surface-to-D-Flow-FM mapping report;
- reproducibility manifest with source hashes and parameter version.

No production case should be exported while fatal findings remain or required review findings remain unconfirmed.

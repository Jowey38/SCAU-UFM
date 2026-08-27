# D-5 sample acceptance checklist

Use this checklist after replacing the synthetic fixtures with an authorized source sample. Mark each item `PASS`, `REVIEW`, or `FAIL` and attach evidence.

## Authorization and provenance

- [ ] Provider authorization is attached and covers project use.
- [ ] Every source file has an owner, URI, version, and checksum.
- [ ] Synthetic/template markers are removed or set to false for real data.

## CRS and units

- [ ] Source CRS is declared for every spatial dataset.
- [ ] Target CRS is agreed and executable.
- [ ] Horizontal units are declared.
- [ ] Vertical units and vertical datum are declared.
- [ ] Time zone and time reference are declared for forcing/model files.
- [ ] All conversions are recorded and reproducible.

## Geometry and terrain

- [ ] Computational boundary is closed and valid.
- [ ] Building geometries are valid and overlap policy is approved.
- [ ] DEM interpretation is confirmed: bare-earth DEM, DSM, or mixed surface.
- [ ] DEM NoData behavior is approved.
- [ ] DEM covers every required mesh cell.
- [ ] Land-cover and soil zones cover the required area or have an approved fallback.

## Mesh and STCF

- [ ] Mesh generation constraints are approved.
- [ ] Mesh contains only supported triangle/quad cells.
- [ ] Topology passes node/face/edge consistency checks.
- [ ] No duplicate or non-manifold edges remain.
- [ ] Mesh quality report has no unaccepted fatal findings.
- [ ] Generated STCF file has schema version 5.
- [ ] UGRID metadata and zero-based connectivity are valid.
- [ ] Field lengths equal cell/edge dimensions.
- [ ] `phi_t`, tensor, roughness, soil, `omega_edge`, `phi_e_n`, and `phi_et` pass range checks.
- [ ] `phi_t`/tensor/edge physical rules are documented and approved.

## SWMM coupling

- [ ] SWMM version and all referenced files are supplied.
- [ ] SWMM node/link IDs are stable and complete.
- [ ] Surface/inlet-to-node mapping is explicit or manually approved.
- [ ] Roof/building/drain-to-node mapping is explicit or manually approved.
- [ ] Exchange elevations and units are confirmed.
- [ ] One-to-many and many-to-one relations are explicitly allowed or rejected.
- [ ] No runtime `Q_limit`, deficit, rollback, replay, or arbitration semantics are embedded in preprocessing output.

## D-Flow FM coupling (optional)

- [ ] Authorized D-Flow FM case and runtime contract are supplied.
- [ ] Native boundary IDs are verified against the supplied case.
- [ ] Boundary type and direction are confirmed.
- [ ] 2D boundary edge mapping is reviewed.
- [ ] Restart and timestep behavior is documented.

## Reproducibility and release

- [ ] Source hashes, parameter versions, and tool versions are recorded.
- [ ] The case can be regenerated from the manifest and source files.
- [ ] All `FATAL` findings are resolved.
- [ ] All `REVIEW` findings have an owner decision.
- [ ] Dedicated importer and GoldenTest evidence exists.
- [ ] Project authority accepts the final D-5 decision.

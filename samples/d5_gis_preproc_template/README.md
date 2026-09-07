# D-5 GIS Pre-Processing Sample Template

This directory is a **synthetic input template** for the SCAU-UFM D-5 real-city-data importer. It is intended for a data owner or project operator to copy, replace with an authorized city sample, and complete with the source-specific metadata and mapping decisions.

The committed geometries, coordinates, IDs, and SWMM case are authored fixtures. They are not real city data, are not an external-data contract, and do not unblock D-5 by themselves.

## What a real provider must replace

Replace the synthetic files under `boundary/`, `terrain/`, `buildings/`, `landcover/`, `soil/`, `swmm/`, and optionally `dflowfm/` with an authorized sample. Keep the metadata and mapping files, but fill every `TODO_PROVIDER` field and remove any placeholder decision.

At minimum, provide a small but complete sample containing:

- a closed computational boundary;
- a DEM with CRS, vertical datum, units, resolution, and NoData definition;
- valid building footprints with stable IDs;
- land-cover or surface-class data;
- a runnable SWMM 5.2.x input and all referenced files;
- an explicit surface/inlet/roof-to-SWMM mapping;
- an expected-output or acceptance reference.

Add the D-Flow FM files and river mapping only when river coupling is in scope.

## Directory map

```text
metadata/
  dataset_inventory.csv       Every source file, owner, format, CRS, units, authorization.
  crs_and_units.yaml          Project target CRS and vertical/time conventions.
  field_dictionary.csv        Source field meaning and target canonical field.
  mapping_rules.yaml          Physical conversion and spatial relation rules.
  geometry_clean_policy.json  Versioned explicit tolerances and repair rules; no implicit defaults.
  crs_policy.json             B2 CRS governance: target projected metre CRS, allowed source CRSs,
                              DEM policy (rasters are never resampled here); geographic CRS is fatal.
  dpm_rule_table.json         B5 DPM/soil rule table (approval-gated; synthetic_unapproved here).
  quality_requirements.md     Fatal/review/pass checks and acceptance thresholds.
boundary/
  computational_boundary.geojson
terrain/
  dem.asc                     Synthetic ASCII-grid DEM fixture.
  dem_metadata.json           DEM metadata; production may use GeoTIFF/NetCDF.
buildings/
  buildings.geojson
landcover/
  landcover.geojson
coupling/
  confirmed/*.json            Operator decisions on coupling candidates (M287-C4 contract v1:
                              accept / reject / retarget / create with candidate hash).
mesh_controls/
  mesh_controls.geojson       Optional operator-drawn breaklines / refinement regions
                              (mesh_controls_schema_version 1; referenced from
                              job_config "mesh_controls", never auto-discovered).
soil/
  soil_zones.geojson
  soil_parameters.csv
swmm/
  model.inp
  node_surface_mapping.csv
  roof_drain_mapping.csv
dflowfm/
  README.md                   Optional D-Flow FM input contract and placeholders.
  boundary_mapping.csv
reference/
  expected_mesh_summary.json  Human-reviewed expectations for the sample.
  expected_relationships.csv  Expected coupling relationships.
validation/
  acceptance_checklist.md
  provider_submission_checklist.md
manifest.json                 Machine-readable package inventory and status.
```

## Canonical targets

The current external import contract recognizes these target names:

```text
node_x, node_y, face_nodes, edge_nodes,
phi_t, phi_xx, phi_xy, phi_yy,
manning_n, z_b, soil_type,
omega_edge, phi_e_n, phi_et
```

`phi_t` and `phi_xx/phi_xy/phi_yy` are not inferred from building polygons without a project-approved physical mapping rule. `omega_edge` and `phi_e_n` remain distinct fields.

The expected 2D generated artifact is a validated STCF v5/CF-UGRID NetCDF case. This template intentionally does not commit a fake generated NetCDF file. The current repository's authored STCF/UGRID fixtures remain the reference for topology and serialization conventions.

## Required provider decisions

Before a real sample can enter an importer spike, the provider must confirm:

1. authorization and permitted use;
2. source format and version for every file;
3. source CRS, target CRS, vertical datum, and units;
4. NoData and invalid-geometry behavior;
5. stable source IDs and relationship keys;
6. DEM interpretation (bare-earth DEM, DSM, or mixed surface);
7. building blocking/opening and roof-drainage semantics;
8. land-cover to Manning/soil mapping;
9. DPM parameter source and conversion rule;
10. surface cell/inlet/roof to SWMM node mapping;
11. river boundary IDs, directions, and boundary types if D-Flow FM is included;
12. expected mesh, mapping, and field-quality acceptance values.

Until those decisions are supplied by the data owner and accepted by the project, D-5 remains blocked as recorded in M281.

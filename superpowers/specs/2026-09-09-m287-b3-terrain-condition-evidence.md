# M287-B3 Terrain Conditioning Evidence

Date: 2026-09-09
Scope: UFM-Mapper master plan v3 §8.3 slice **B3** (stage B). Any change to the
input DEM is a *repair*: it needs an explicit, versioned authorization policy and
a per-operation audit; without it the DEM is sampled verbatim. New gate
**G38 `preproc_terrain_condition_case`** (`ci_gate:true`). M281 D-5 stays BLOCKED.

## Deliverables

- `metadata/terrain_condition_policy.json` contract
  (`terrain_condition_policy_schema_version 1`): `enabled`, mandatory
  `authorization {authorized_by, date}` when enabled, `operations`
  `reproject` (nearest | bilinear, optional target cell size) /
  `stream_enforcement` (streams GeoJSON, burn depth, sample spacing) /
  `fill_depressions` (`priority_flood` **v1** only, `epsilon_m`, 4/8
  connectivity, NoData-as-outlet, review depth threshold), `diagnostics`.
  Unknown operation, unknown algorithm / version, enabled-without-authorization
  or enabled-with-nothing-authorized are all fatal at policy load.
- `python/scau_preproc/terrain_condition.py`: fixed order
  reproject → stream enforcement → Priority-Flood (Barnes, Lehman & Mulla 2014,
  deterministic heap tie-break by (row, col)); ASCII grid I/O; writes
  `<output>/conditioned_terrain/{dem.asc, depression_depth.asc,
  terrain_change.asc, terrain_condition_report.json}` (policy hash,
  authorization, per-operation statistics, before/after SHA-256, change summary,
  findings). `TerrainFillDepthReview` (review) when the max fill exceeds the
  policy threshold; missing streams / missing target CRS are fatal.
- Raster reprojection takeover: `crs_governance.govern_package(...,
  dem_reprojection_authorized=True)` records the DEM as
  `reprojection_deferred_to_terrain_condition` instead of failing
  `must_match_target`; `terrain/streams.geojson` joins the governed vector set.
  Target cell centres are inverse-transformed with the pinned pyproj
  Transformer (pipeline string recorded); cells outside the source become NoData.
- `pipeline.py`: `job_config.terrain_condition {policy}` (default: package
  `metadata/terrain_condition_policy.json` when present); the policy is loaded
  **before** CRS governance (to pass the reproject authorization), the stage runs
  after governance on the staging package, and the generator receives the
  conditioned DEM path **only when `enabled: true`** — the disabled path leaves
  `generator.config.json` untouched. `validation.terrain_condition`,
  `pipeline_manifest.terrain_condition` and terrain findings are recorded;
  `TerrainConditionFailed` is a fatal finding naming the dataset.
- `meshgen.py`: optional `dem` config key (defaults to the package DEM).
- Sample `metadata/terrain_condition_policy.json` (**disabled**, `TODO_PROVIDER`
  authorization) registered in the sample manifest.
- Plugin 0.10.0: DEM policy field → `job_config.terrain_condition`; Terrain lamp
  shows `conditioned (…): N cells changed, max |dz|` (review) after an enabled
  run; Outputs group lists the three conditioned rasters (double-click loads an
  `.asc` as `QgsRasterLayer`); 地形/网格 report page gets terrain rows;
  reproducibility page shows the manifest block.
- `tools/qgis/offscreen_smoke.py` (reusable headless dialog smoke; prints one
  `SMOKE <step> ok|FAIL` per step; `--terrain-policy`, `--run`).
- `python/tests/test_terrain_condition.py` (12) + terrain rows test →
  **73 headless tests**.
- G38 fixture: `cases/regenerate.py --run` authors `dem_depression.asc` (20 × 12
  @ 10 m, west→east slope with a 4 × 4 bowl −0.30 / −0.60 m, spill column
  10.30 m), `streams.geojson`, enabled / disabled policies, and captures the
  conditioned twin (`7d3e43ea…`), the raw twin, the conditioned rasters, the
  report and both manifests.

## Evidence

1. **Disabled policy = bitwise no-op**: the sample package with the disabled
   policy meshes to the **G30 case bytes** (asserted by `regenerate.py` and by
   G38 via the recorded manifest SHA); the unit test shows no file is written.
2. **Authorized run**: 19 stream cells burned by 0.20 m, then Priority-Flood
   seeded from 60 rim cells fills 16 cells (max 0.52 m, 440 m³); 35 cells
   changed in total, net +60 m³. Every bowl cell of the meshed case has
   `z_b == 10.30` exactly (fill to spill, ε = 0) whereas the raw twin dips to
   9.78; stream cells are lower by exactly 0.20 m; **every other cell, all
   `manning_n` and all `phi_t` are bit-identical** between the twins (the stage
   touches only what the policy authorizes).
3. **Algorithm unit evidence**: pit fills to the rim spill elevation with 4- and
   8-connectivity; ε imposes a gradient; NoData acts as an outlet only when
   authorized; a slope without pits is unchanged; repeat runs are identical.
4. **Reprojection takeover**: `must_match_target` still fails without B3;
   with an authorized `reproject` the DEM is deferred, then resampled
   EPSG:3857 → EPSG:3395 onto a 20 × 12 @ 10 m target grid (PROJ pipeline
   recorded), identity source/target is skipped, geographic CRS is fatal.
5. **Negatives**: 12 malformed policies rejected at load; missing streams →
   fatal naming `streams`; reproject without CRS governance → fatal.
6. **GoldenSuite**: `ctest -R "preproc|case_export|manifest"` 10/10 (G30, G32,
   G35, G36, G37 unchanged; G38 new); manifest checker OK with G38.
7. **QGIS 4.0.0 offscreen** (`tools/qgis/offscreen_smoke.py --run`): sample
   package (disabled policy) → `PipelineOk`; overlay package + enabled fixture
   policy via the new field → `PipelineOk`, `validation.terrain_condition`
   active operations `[stream_enforcement, fill_depressions]`, 35 cells changed;
   tree / report refresh ok before and after the run.

## Boundaries kept

- The input package is never modified; conditioned rasters live in the run
  output and are hashed into the manifest.
- No operation runs without `enabled: true` **and** an authorization block;
  the only fill algorithm is `priority_flood` v1 — a different algorithm or
  version is a policy change, never a silent upgrade.
- Fill depth above the policy threshold is a **review** item (operator checks
  `depression_depth.asc`); structural problems are fatal.
- C4 confirmations do not depend on `z_b` sampling, but a reprojected DEM
  changes `exchange_elevation_m` placeholders in spatial candidates — expected
  drift, as recorded for B2.

## Follow-ups

- Multi-DEM mosaicking (priority merge, v3 "B3b") is not in this slice.
- Local terrain modification (channel cut / levee raise polygons) is not in
  this slice; the policy schema leaves room for a fourth operation.
- GeoTIFF / COG input still needs an authorized real sample (M281); B3 reads
  the ESRI ASCII grid the synthetic package ships.

# M287-B2 CRS Governance Reprojection Evidence

Date: 2026-09-07
Scope: UFM-Mapper master plan v3 §8.3 slice **B2** (stage A). Coordinate reference
systems become a declared, audited, fail-closed contract; sources may be
reprojected into the project target CRS under a versioned policy. New gate
**G37 `preproc_crs_governance_case`** (`ci_gate:true`). M281 D-5 stays BLOCKED.

## Deliverables

- `metadata/crs_policy.json` contract (`crs_policy_schema_version 1`):
  `target_crs` (projected, metre; geographic ⇒ fatal; `TODO_PROVIDER` ⇒ fatal),
  `allow_reprojection`, `allowed_source_crs` whitelist (geographic entries ⇒
  fatal), `source_crs_overrides` for files without a `crs` member, `dem_policy`
  (`must_match_target` default — **rasters are never resampled in B2**; or
  `declare_only`), `audit_sample_count`.
- `python/scau_preproc/crs_governance.py`:
  - resolves each dataset's CRS (GeoJSON legacy `crs` member incl. URN form, or
    override), rejects undeclared / geographic / non-allowed / disallowed
    reprojection, each error naming the dataset;
  - materialises a **governed staging package** `<output>/governed_package/`:
    vectors (boundary, buildings, landcover, soil zones) and mesh controls
    (inherit the boundary CRS) reprojected with pyproj, SWMM `[COORDINATES]`
    rewritten **only in that section** (all other lines verbatim), everything
    else copied byte-for-byte; sources already in the target CRS are copied,
    never pushed through PROJ;
  - coordinates rounded to 1e-6 m for stable text; inverse round-trip on audit
    samples must be < 1e-6 m (fatal otherwise);
  - `crs_audit.json`: policy hash, target WKT, PROJ/pyproj versions, per-dataset
    action + PROJ pipeline string + before/after samples with round-trip error.
- `pipeline.py`: `job_config.crs {policy}` (default: package
  `metadata/crs_policy.json` when present; no policy ⇒ v1 unchecked mode,
  recorded as `crs_governance: null`); governance runs before contract checks
  on the staging package and every later stage consumes it; fatal
  `CrsGovernanceFailed` with dataset object; `pipeline_manifest.crs_governance`
  records pipelines and SHA-256 of every reprojected file.
- Sample `metadata/crs_policy.json` (identity EPSG:3857 target — exercises the
  checks without transforming); G37 uses the reprojecting variant
  `cases/crs_policy_3395.json` (EPSG:3857 → EPSG:3395, `dem_policy declare_only`).
- Plugin 0.9.0: CRS policy field; data-tree lamps read the pipeline audit when
  present (`CRS src -> dst (action)`); 接入 (A) report page rows.
- Third-party governance: `third_party/licenses/pyproj-proj-LICENSE-NOTE.md` +
  `third_party/manifest/pyproj.version` (MIT / PROJ MIT-X11; python-only use;
  pipeline strings pinned; grid files must be pinned before any grid transform).
- `python/tests/test_crs_governance.py` (5) → 60 headless tests total.
- G37 fixture (case SHA `8a4d72b6…`, audit, manifest, policy, rewritten `.inp`)
  + C++ golden; manifest/checker/CMake 3-way sync.

## Evidence

1. **Identity policy leaves bytes untouched**: sample package with the identity
   policy + mesh controls → every dataset `copied_identity`, `reprojected_files
   = []`, meshed case SHA `d48253d0…` **== G32 fixture bytes**.
2. **Reprojecting run** (EPSG:3857 → EPSG:3395): 5 vector/control files
   reprojected, SWMM 3 nodes rewritten (`J1 55.000000 9.933056`), DEM
   `declared_mismatch_allowed`; PROJ 9.8.1 pipeline
   `inv webmerc → merc` recorded; round-trip errors 0 / 2.8e-14 m; mesh 1116
   nodes / 1072 cells; `scau_preproc validate` ok.
3. **Determinism**: two runs into different output dirs → identical SHA-256 for
   every governed file and the meshed case.
4. **Negatives (8)**: geographic target, `TODO_PROVIDER` target, source not in
   whitelist, geographic layer (rejected at the whitelist), undeclared layer
   (fatal) → rescued by `source_crs_overrides` (ok), DEM mismatch under
   `must_match_target`, reprojection with `allow_reprojection: false` — all
   fatal `CrsGovernanceFailed` naming the dataset, exit 2.
5. **G37 golden** (authoritative loader): node extent x ∈ [0, 200], **y max
   119.196674** (Mercator compression, proves a real transform), no quality
   findings; both rewritten SWMM inlets are contained by road cells (Manning
   0.015) of the reprojected mesh — vector and SWMM reprojections mutually
   consistent; audit pins the pipeline string and actions; lake at rest 1e-12.
   `ctest -R "preproc|coupling…|case_export|manifest"` 12/12.
6. **QGIS 4.0.0 offscreen**: policy field → `job_config.crs`; run ok; tree lamps
   `buildings Pass (reprojected)`, `swmm Pass (coordinates_rewritten)`, `dem
   Review (declared_mismatch_allowed)`; 接入 page shows target/PROJ/files;
   geographic policy → `CrsGovernanceFailed` rendered, gate closed.

## Boundaries kept

- The input package is never modified; governance writes a staging copy.
- No raster resampling (B3 owns terrain conditioning); DEM mismatch is fatal by
  default and only *declared* under explicit policy.
- Transform is pinned by pipeline string + versions; upgrades surface as G37
  fixture changes, never silently.

## Follow-ups

- **B3** DEM conditioning may take over raster reprojection under its own
  authorization policy; until then `dem_policy: must_match_target`.
- Confirmation files hash the candidate relations, which include node
  coordinates: a CRS change legitimately drifts existing confirmations
  (observed: 4 × `ConfirmationDrift` on the reprojected run) — by design.
- Datum-shift grids are out of scope until a policy needs them (pin PROJ_DATA).

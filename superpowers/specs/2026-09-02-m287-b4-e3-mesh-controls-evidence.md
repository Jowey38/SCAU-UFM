# M287-B4 + E3 Mesh Controls (Breaklines / Refinement Regions / Heat Map) Evidence

Date: 2026-09-02
Scope: first slice of the UFM-Mapper plan (`docs/superpowers/plans/2026-09-02-ufm-mapper-gis-platform-plan.md`
§11 step 1): backend mesh-control contract (B4) and the QGIS mesh workbench
tab (E3). Synthetic-package workflows only; M281 D-5 stays BLOCKED; no gate
semantics changed. New gate: **G32 `preproc_mesh_controls_case`** (`ci_gate:true`,
same fixture-only pattern as G30).

## Deliverables

- `python/scau_preproc/mesh_controls.py` — the `mesh_controls` GeoJSON contract
  (schema v1): loading, fail-closed validation, post-mesh breakline-preservation
  assertion, per-cell diagnostics writer. Pure geometry; no gmsh import.
- `python/scau_preproc/meshgen.py` — refinement regions become gmsh plane
  sub-surfaces with a `Constant` size field; breaklines are embedded curves
  (`gmsh.model.mesh.embed`) with an optional linear `Threshold(Distance)` field;
  fields are combined by `Min` and made the single size source
  (`MeshSizeFromPoints/ExtendFromBoundary/FromCurvature = 0`) **only when at
  least one size field exists**. Building holes and breaklines are attached to
  the surface that owns them (ownership from the validator). New outputs:
  `mesh_quality_cells.geojson` (always) and a `mesh_controls` block in
  `mesh_quality.json` (breakline edge counts, region size statistics, controls
  SHA-256). Generator now runs as `py -3 -m scau_preproc.meshgen`.
- `python/scau_preproc/pipeline.py` — `job_config.json` (schema v1, extended,
  not versioned up: the key is optional) gains
  `"mesh_controls": {"geojson": path, "default_size_m"?, "default_dist_max_m"?}`;
  missing file / non-positive defaults are contract errors; the controls
  SHA-256 is recorded in `pipeline_manifest.json`; generator rejections carry
  the `MeshControlsRejected` finding code plus an `objects[]` list
  (`feature_id`, `kind`, `violation`) so the UI can locate offenders.
- `samples/d5_gis_preproc_template/mesh_controls/mesh_controls.geojson` —
  synthetic controls: 3 breaklines (one sized, one inside a region), 2
  refinement regions (one owning building hole `BLDG_SYNTH_001`).
- `tests/golden/preproc_mesh_controls_case/` — G32 fixture
  (`synthetic_city_block_controls.stcf.nc`, SHA-256
  `d48253d0b3fc063eb2080baa0c0df9186de4233a69d95db9d38a84ffd9dd27b9`), committed
  pipeline manifest, the controls file, and the C++ golden. Registered in
  `goldensuite.json`, `check_manifest.py` REQUIRED, and the per-test
  CMakeLists (`LABELS golden`).
- `python/tests/` — headless unit tests (16) for `mesh_controls.py` and the
  plugin's `jobio.py` (`py -3 -m unittest discover -s python/tests -t python`).
- `qgis_plugin/scau_preproc_workbench/` (plugin 0.2.0):
  - `jobio.py` — `build_job_config(... mesh_controls_geojson, defaults)`,
    `mesh_controls_precheck` (schema/attribute presence only — geometry rules
    stay in the generator), `mesh_controls_template`, `heatmap_classes`
    (5 fixed good→bad classes per metric), `mesh_quality_summary`,
    `layer_paths` now lists `mesh_controls` and `mesh_quality_cells`, and
    `subprocess_env()` (bug fix below).
  - `workbench_dialog.py` — tabbed dialog: E1 job page + **网格工作台** tab:
    enable/point to `mesh_controls.geojson`, default size/dist, create
    breakline + refinement-region memory scratch layers
    (`control_id/control_kind/size_m/dist_max_m`), save scratch layers to the
    contract GeoJSON, pre-check, heat-map metric selector (skewness,
    non-orthogonality, min angle, edge ratio, area) rendered with
    `QgsGraduatedSymbolRenderer`, quality summary rows after every ok run.

## Contract (single source: `mesh_controls.py` docstring)

| control_kind | geometry | properties | effect |
|---|---|---|---|
| `breakline` | LineString | `control_id`, optional `size_m` + `dist_max_m` | embedded; every segment preserved as a mesh-edge chain; optional linear refinement `size_m → lc` over `dist_max_m` |
| `refinement_region` | Polygon | `control_id`, required `size_m` in (0, lc] | plane sub-surface meshed at `size_m`; ring preserved as mesh edges |

Fail-closed rules (all reported with `control_id` + `violation` in
`generator.diagnostic.geojson`, exit 2, no partial case): unsupported schema
version, duplicate id, unknown kind, geometry-type mismatch, unclosed /
self-intersecting ring, degenerate polyline, vertex not strictly inside the
boundary, vertex on/in a building hole, crossing the boundary or a hole,
`size_m` out of (0, lc], sized breakline without positive `dist_max_m`,
overlapping regions, building straddling a region boundary, breakline
crossing a region boundary, and (post-mesh) breakline not preserved as mesh
edges. v1 limitations (documented, not silently relaxed): controls may not
touch the boundary or a building ring; breaklines may not cross region rings.

## Evidence

1. **Zero-controls path byte-identical** — pipeline run on the D-5 template
   without `mesh_controls` reproduces the committed G30 fixture SHA-256
   `a89ba98da95bcd5732c17e1bf9977e8f75bdc1d81b96a6844333fc796e911bc4`
   (determinism rerun `bitwise_identical_reruns: true`).
2. **Controls run** (`H:/scau-b4-out/m287b4_job.json`): status ok; gmsh 4.15.2;
   1113 nodes / 1070 cells (44 tri, 1026 quad) / 2184 edges; breaklines
   preserved with 59 / 8 / 15 mesh edges; `RR_POND` (size 2.5 m) 288 cells,
   mean edge 2.2746 m, max 3.825 m; `RR_BUILDING_1` (size 4.0 m) 104 cells,
   mean edge 3.5487 m, max 4.778 m; unrefined remainder mean ≈ 5.3 m;
   determinism rerun bitwise identical (also identical across two output
   directories); `scau_preproc validate` exit 0; min angle 18.96°, max edge
   ratio 3.17 (inside review thresholds).
3. **Negative runs** (9): building-straddling region, breakline touching the
   boundary, breakline crossing a hole, `size_m > lc`, overlapping regions,
   bad schema version, unknown kind, sized breakline without `dist_max_m`,
   missing controls file — every one exit 2, `validation.json` fatal with
   `MeshControlsRejected` + `objects[]`, diagnostic GeoJSON written, no
   `case.stcf.nc` / `.tmp` left behind.
4. **G32 golden** (independent C++ re-check through the authoritative loader):
   shape/field domains, breakline + region-ring edge chains, region cells
   finer than the remainder (recorded means pinned to 1e-3), no fatal or
   review quality findings, 20-step lake at rest ≤ 1e-12. `ctest -R
   "preproc|coupling_maps|manifest"`: 7/7 (G22, G30, G31, G32, manifest).
5. **Headless UI layer**: 16/16 unit tests.
6. **QGIS 4.0.0 offscreen smoke** (`python-qgis.bat`, `QT_QPA_PLATFORM=offscreen`,
   deployed profile copy): create scratch layers → draw 2 breaklines + 1
   region via the API → save to GeoJSON (3 features, contract-exact
   properties) → pre-check pass → `_run()` real subprocess → `PipelineOk`,
   943 cells, both breaklines preserved, export gate 允许 → heat map layer
   valid with `QgsGraduatedSymbolRenderer(equiangle_skewness)` 5 classes →
   `加载图层` lists `mesh_controls` → bad controls file blocked pre-run with
   `MeshControlSize`.

## Bug fixed en route (logged in `.wolf/buglog.json`)

- The QGIS host exports `PYTHONHOME`/`PYTHONPATH` for its bundled Python 3.12
  (`qgis-bin.env`); inherited by the `py -3` (3.14) pipeline subprocess they
  cause `AssertionError: SRE module mismatch` before `validation.json` exists,
  which E1 rendered only as `NoValidationReport`. `jobio.subprocess_env()` now
  scrubs those variables; the dialog also surfaces subprocess stderr whenever
  no validation report was produced.
- `region_size_report` initially averaged a *set of edge lengths* (deduping
  equal lengths); the C++ golden's independent computation exposed the bias
  (3.483 vs 3.549 m). Fixed to dedupe by edge index; manifest regenerated
  (case SHA unchanged).

## Not done / follow-ups

- Controls touching the boundary / buildings and breaklines crossing region
  rings are rejected rather than split (v1 limitation; a splitting step would
  need its own determinism evidence).
- Heat-map metrics are display diagnostics; `libs/mesh` quality thresholds
  remain the only certification path (no Python duplication).
- Cross-platform determinism remains the open B7 experiment (same-platform
  claim only).
- Plan §11 next: M287-C4 + E4 (confirmation contract + coupling editor).

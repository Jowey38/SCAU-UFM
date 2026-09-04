# M287-C5 Spatial-Candidate Mapping Mode Evidence

Date: 2026-09-04
Scope: UFM-Mapper master plan v3 §8.2 slice **C5** (project decision 2026-09-03,
PR #94): coupling candidates without explicit ID tables. New gate **G34
`coupling_spatial_candidates_case`** (`ci_gate:true`, committed-fixture pattern).
M281 D-5 stays BLOCKED; nothing spatial becomes authoritative.

## Deliverables

- `python/scau_preproc/coupling_maps.py` — `generate(..., mode, roof_node_max_distance_m)`:
  - `explicit_ids` (default) — v1 behaviour, tables required; **outputs byte-identical
    to the committed G31 fixture** (relations, dflowfm payload, fragment); report only
    gains additive keys (`mode`, `mode_parameters`, `spatial_candidates`, `findings`,
    per-chain `by_confidence`).
  - `spatial_candidates` — tables are not read. Every SWMM **junction**
    (`[JUNCTIONS]`; outfalls/storage/dividers are skipped) whose coordinate lies in a
    mesh cell → `SPATIAL_SURF_<node>` candidate, `method =
    spatial_point_in_cell_candidate`, `confidence: review`, `needs_confirmation`,
    `exchange_elevation_m = cell z_b` tagged `exchange_elevation_source:
    placeholder:cell_z_b`. Node inside a building footprint → **fatal** (checked
    against the footprint itself, before containment). Node with no containing cell
    → review finding `SwmmNodeNotInAnyCell` / `SwmmNodeOutsideSurfaceDomain`, no
    relation. Every building → `SPATIAL_ROOF_<building>` candidate to the nearest
    junction (tie → lexicographic node id) within `roof_node_max_distance_m`
    (default 50 m, versioned in the report/manifest), `catchment_area_m2 =`
    footprint area tagged `placeholder:footprint_area`; too far → review finding
    `RoofNoJunctionWithinDistance`; no junctions → `RoofNoJunctionAvailable`.
  - `mixed` — table rows are authoritative (`high`) where present (a table may be
    absent entirely); nodes/buildings not covered get spatial candidates (`review`).
  - CLI: optional 4th argument `mode`. `MappingError` now carries `.message`.
- `python/scau_preproc/pipeline.py` — `job_config.coupling_maps` accepts `true`
  (explicit) or `{"mode", "roof_node_max_distance_m"}` (validated; recorded in
  `pipeline_manifest.json.coupling_maps`); generator findings propagate to
  `validation.findings`; generator error message propagates into
  `CouplingMapGenerationFailed.detail`.
- `qgis_plugin/scau_preproc_workbench/` (0.4.0) — mode combo + roof-distance spin on
  the job page; `jobio.build_job_config(coupling_mode, roof_node_max_distance_m)`;
  `coupling_mode_rows` (mode + per-chain high/review split) in the findings table.
- `tests/golden/coupling_spatial_candidates_case/` — G34 fixture (spatial run on the
  D-5 package, G30 mesh) + C++ golden; manifest/checker/CMake 3-way sync.
- `python/tests/test_coupling_modes.py` (5, hand-built 2×2 mesh, no gmsh) + 1 jobio
  test → 31 headless tests total.

## Evidence

1. **Explicit mode unchanged**: `cmp` of `surface_swmm_mapping.json`,
   `roof_drain_mapping.json`, `surface_dflowfm_mapping.json`, `simdriver_links.conf`
   against the committed G31 fixture → identical; G31 still passes.
2. **Spatial mode on the D-5 package** (tables ignored): J1 → cell 379, J2 → cell 234
   (same cells the explicit tables resolve to — same containment geometry, different
   authority), crests 10.4 / 10.7 = cell `z_b` (explicit table had 10.0 / 9.5); roofs
   BLDG_1 → J1, BLDG_2 → J2 at 45.0 m; all four `review`; outfall O1 emits nothing;
   pipeline status `review`, `CouplingCandidatesUnconfirmed 4`.
3. **Mixed mode, partial tables** (surface table keeps only J1, roof table deleted):
   surface `{high: 1, review: 1}` (J1 explicit, J2 spatial), roof 2 spatial; report
   `spatial_candidates = {surface_nodes: [J2], roof_buildings: [both]}`.
4. **Negatives**: node moved into BLDG_1 footprint → fatal exit 2 with the footprint
   id in `detail`; node at (250, 10) → `SwmmNodeOutsideSurfaceDomain` review, other
   node still a candidate; `roof_node_max_distance_m = 30` → both roofs
   `RoofNoJunctionWithinDistance`, zero roof relations; junctions rewritten as
   outfalls → zero surface candidates + `RoofNoJunctionAvailable`; `mode` unknown or
   distance ≤ 0 → fatal.
5. **G34 golden** (SimDriver parser + surface2d loader): spatial fragment cells equal
   the explicit G31 cells while crests differ; each crest equals
   `bed_elevations[cell]` of the containing cell and the node lies inside it; JSON
   probes: 2 + 2 review-only candidates, placeholder sources, no `high`, no O1,
   mode/threshold declared. `ctest -R "preproc|coupling_maps|coupling_confirmations|
   coupling_spatial|manifest"` 9/9.
6. **QGIS 4.0.0 offscreen smoke**: mode combo → `job_config.coupling_maps =
   {mode: spatial_candidates, roof_node_max_distance_m: 50.0}`; run → `CouplingMode`,
   per-chain `high=0 review=2` rows, `CouplingCandidatesUnconfirmed`, export gate 禁止.

## Boundaries kept

- Spatial results are **candidates only**: `review` + `needs_confirmation`; they become
  effective exclusively through C4 confirmations (`effective/simdriver_links.conf`).
- Placeholders are tagged with their source (`cell_z_b`, `footprint_area`) — nothing is
  presented as a measured crest or catchment.
- No runtime semantics; placeholders `exchange_width_m` / `priority_weight` pass through.

## Follow-ups

- **E4** coupling editor: render candidates (review in red), write C4 decisions incl.
  `create`; N1-C reuses this mode for authored networks.
- Mixed mode currently keys explicit coverage by node id / building id; a table row
  pointing at a node outside the mesh is still fatal (explicit path), by design.

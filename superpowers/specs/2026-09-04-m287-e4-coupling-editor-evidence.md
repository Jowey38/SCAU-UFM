# M287-E4 Coupling Relationship Editor Evidence

Date: 2026-09-04
Scope: UFM-Mapper master plan v3 §8.2 slice **E4** (page P6) — the QGIS editor that
renders coupling candidates on the map and writes the C4 confirmation contract.
Closes phase I ("candidate → confirmation → effective" loop). No new gate: the
contract it writes is already locked by G33; the editor's pure layer is
headless-tested against `scau_preproc.confirmations` (single contract).

## Deliverables

- `qgis_plugin/scau_preproc_workbench/jobio.py` (pure, qgis-free):
  - `build_coupling_link_layers(job)` → three GeoJSON collections **links**
    (LineString source → target-cell centroid), **nodes** (Point: SWMM node
    coordinate or building centroid) and **cells** (Polygon from
    `mesh_quality_cells.geojson`), each tagged `chain / mapping_id / swmm_node_id /
    building_id / candidate_cell_index / cell_index / exchange_elevation_m /
    method / confidence / status / decision_file`; `status ∈ unconfirmed |
    accepted | retargeted | rejected | drifted` derived from the confirmation
    files by re-hashing the CURRENT candidate (`candidate_sha256` identical to
    `scau_preproc.confirmations` — asserted by a test).
  - `write_decision` (accept / reject / retarget, hash bound to the candidate the
    operator saw), `write_create_decision` (operator-authored link, refuses an
    existing candidate id, roof requires `building_id`), `remove_decision` (undo),
    atomic `.tmp` → rename writes, `confirmations_dir_for(job)` = job
    `confirmations_dir` or package `coupling/confirmed/` (pipeline default),
    `coupling_editor_rows` summary.
- `workbench_dialog.py` (plugin 0.5.0) — **耦合编辑器** tab: operator id
  (QgsSettings-persisted), refresh → categorized-by-status layers
  (`scau_coupling_cells/links/nodes`; red unconfirmed, green accepted, blue
  retargeted, grey rejected, orange drifted), candidate table, row selection
  highlights the feature on all three layers, target cell (spin or "取地图选中单元"
  from a selected heat-map cell) + optional crest + note, accept / reject /
  retarget / undo buttons, create-link row (chain, mapping_id, node, building).
  Every action re-reads files and refreshes; nothing is cached in the UI.
- `python/tests/test_jobio_coupling_editor.py` (4) → 35 headless tests total.

## Evidence

1. **Headless**: layers/statuses before any decision (3 unconfirmed, roof link from
   building centroid to cell centroid); accept + retarget + reject + create written
   by `jobio` are loaded and merged by `scau_preproc.confirmations.merge` to a
   `complete` status with effective ids `[MAP_1, MAP_NEW, SPATIAL_SURF_J2]`;
   retarget shows `cell_index` = new cell and `candidate_cell_index` = original;
   candidate regenerated underneath a decision → `drifted`; undo removes the file;
   error paths (unknown candidate, retarget without target, empty operator, create
   reusing a candidate id, roof create without building).
2. **QGIS 4.0.0 offscreen end-to-end** (spatial mode, empty confirmations dir):
   run 1 → gate 禁止; editor loads 3 layers, 4 links, `QgsCategorizedSymbolRenderer`
   on `status`, table all `unconfirmed`; widget-driven accept (J1), retarget (J2 →
   cell 50, crest 9.6), reject (roof 1), accept (roof 2) → `CouplingEditor pass
   … unconfirmed=0`; selecting a row selects 1 feature on the links layer; undo →
   `unconfirmed`, re-reject; create `MAP_CREATED_O1` (O1 → cell 40, crest 10.2);
   create with an existing id → `CreateRejected`; run 2 → **gate 允许**,
   `CouplingConfirmations complete`, effective fragment =
   `cell=40,O1,10.2 / cell=379,J1,10.4 / cell=50,J2,9.6`; five decision files on
   disk with `confirmed_by = smoke_operator`.

## Boundaries kept

- The editor never edits generator output; it writes only `coupling/confirmed/*.json`.
- Spatial candidates remain review until an explicit decision; "create" is an
  explicit operator decision carrying author/time, not an inferred link.
- Cell geometry used for display comes from the diagnostic layer; authority stays
  with the pipeline (C++ containment re-check in G31/G34, merge in stage E').

## Follow-ups

- Phase I complete (C4 ✔ C5 ✔ E4 ✔). Next per v3 §8.8: **B6 case exporter** (must
  consume `effective/simdriver_links.conf` and refuse `status: incomplete`) + **E5**
  gate/export centre; **E2** data-tree page can run in parallel.
- Map-click retarget uses the heat-map layer selection; a dedicated map tool is a
  polish item for E6.

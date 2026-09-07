# M287-E2 Data Tree + E5 Gate/Report Browser Evidence

Date: 2026-09-06
Scope: UFM-Mapper master plan v3 §5 pages **P2 数据目录树** (E2, including the
P1/P3 status content: CRS/units/completeness lamps) and **P7/P8 门禁与报告**
(E5: report browser + gate summary + finding → map-object locator; the export
action landed with B6). UI-only slice: no new contract, no gate; every lamp and
row is a rendering of files the pipeline already writes.

## Deliverables

- `jobio.py` (pure, qgis-free):
  - `data_tree(job)` → RAS-Mapper-style groups **Terrain / Land Cover-Soil /
    Geometries / 1D Networks / Policies / Outputs** from the package
    `manifest.json` datasets; per item lamp `pass | review | fatal | missing`:
    required-missing ⇒ fatal, optional-missing ⇒ missing, geographic CRS
    (EPSG:4326 / degrees) ⇒ fatal, undeclared CRS (`TODO_PROVIDER`) or non-metre
    units ⇒ review, mapping tables with provider placeholders ⇒ review, dflowfm
    ⇒ review (provider_required); a validation finding whose `objects[]` reference
    the layer's kind escalates the lamp (fatal/review). Outputs group: validation
    status lamp, quality/diagnostic/effective-links artefacts, exported package.
    `data_tree_summary` counts lamps.
  - `report_pages(job)` → six key/value pages (接入 A / 地形-网格 B-C / 字段 D /
    耦合 E-E' / 导出 F / 可复现) read from validation, pipeline manifest, mesh
    quality, mapping report, effective links and the exported package manifest.
  - `findings_table(validation)` → one row per (finding, object);
    `locate_object(job, kind, feature_id)` → `(layer_name, path, expression)` for
    `mesh_control:*`, `coupling:*`, `building`, `boundary` / generator diagnostic,
    `swmm:*` (editor nodes layer), with expression quoting; `None` when no map
    layer exists for the kind.
- `workbench_dialog.py` (plugin 0.7.0): **数据目录** tab (QTreeWidget, coloured
  lamps, group lamp = worst child, double-click loads GeoJSON items as layers)
  and **门禁与报告** tab (six report sub-tabs, findings table, 定位 button that
  selects the feature and zooms the canvas when `iface` is present, gate summary
  that also drives the export button). `plugin.py` passes `iface` to the dialog.
- `python/tests/test_jobio_data_tree.py` (4) → 51 headless tests total.

## Evidence

1. **Headless**: sample package tree groups/lamps (DEM review for `TODO_PROVIDER`
   CRS, SWMM pass, dflowfm review, buildings loadable); synthetic manifest with a
   missing required DEM ⇒ fatal, optional soil ⇒ missing, a fatal building finding
   escalates the buildings lamp; EPSG:4326 ⇒ fatal; report pages keyed as
   documented; findings flattened per object; locator expressions incl. quote
   escaping; `None` for kinds without a layer.
2. **QGIS 4.0.0 offscreen** on the C5 "node outside" review run: tree renders six
   groups (`Pass 7 / Review 8 / Fatal 0`), double-click on `buildings` loads
   `scau_buildings`; report browser: gate 禁止导出 (status=review, 3 findings),
   page row counts 8/16/4/10/5/8; 定位 on the `RoofNoJunctionWithinDistance`
   finding selects `"building_id" = 'BLDG_SYNTH_002'` → 1 feature; 定位 on the
   `swmm:junction` finding reports `NoLocator` with guidance (editor layers not
   built yet). Switching to the B6 ok run: gate 允许导出, export button enabled,
   export page shows target/hash/files/gate/run_conf.

## Boundaries kept

- No validation is re-implemented: lamps quote manifest declarations and pipeline
  findings; the CRS rule (reject geographic, review undeclared) mirrors the B2
  contract-to-be without transforming anything.
- Locators only select/zoom existing layers; nothing is edited.

## Follow-ups

- B2 CRS governance will replace the manifest-declaration lamp with the pipeline's
  reprojection audit (same lamp semantics).
- E6/E7 polish: icons, translations, project index; `swmm:*` locator could build a
  nodes layer directly from the `.inp` when the editor layers are absent.

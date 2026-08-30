# M287-E1 QGIS Plugin First Slice Evidence

Date: 2026-08-30
Scope: M287 plan phase E, first slice — the stateless QGIS driver UI over the
`scau_preproc` pipeline. Synthetic-package workflows only; M281 D-5 stays
BLOCKED. Phase E remains IN PROGRESS (remaining pages listed below).

## Deliverables

- `qgis_plugin/scau_preproc_workbench/`:
  - `jobio.py` — pure-python UI<->CLI contract layer (NO qgis imports):
    builds/writes `job_config.json` (schema v1), runs
    `py -3 -m scau_preproc.pipeline` as a subprocess with a UI timeout,
    re-reads `validation.json`, flattens findings for display, computes the
    export gate (`status: ok` and no fatal findings only), and lists
    map-visualizable artifacts. Fail-closed results are DATA the UI renders,
    never swallowed exceptions.
  - `workbench_dialog.py` — the E1 dialog: package/output/validator paths,
    characteristic length, recombination, determinism check, coupling-map
    stage toggle; run button; findings table; export-gate label; layer
    loading (boundary/buildings/landcover + generator diagnostic GeoJSON).
  - `plugin.py` + `__init__.py` + `metadata.txt` (`supportsQt6=yes`,
    experimental, qgisMinimumVersion 3.40; tested against QGIS 4.0.0
    install at `D:\Program Files\QGIS 4.0.0`).
- `tools/qgis/deploy_plugin.sh` — copy deployment into the QGIS4 default
  profile plugins directory.

## Architecture compliance (M287 plan section 3)

- Stateless pipe: UI renders choices -> `job_config.json` -> subprocess CLI
  -> reads `validation.json`; no computation state in the UI; reports are
  re-read from disk each run.
- The pipeline executes in the SYSTEM python (gmsh + netCDF4), not the QGIS
  runtime; the subprocess boundary doubles as the GPL boundary on both sides
  (QGIS GPL shell above, gmsh GPL runtime below; the repo core touches
  neither).
- Export gate mirrors plan page 8: disabled unless a clean `ok` validation.

## Headless verification (jobio, no GUI; local 2026-08-30)

| Scenario | Result |
|---|---|
| nominal synthetic package (coupling maps on) | status ok; findings row `PipelineOk`; exportable True; layers boundary/buildings/landcover found |
| degenerate package (bowtie building) | status fatal; findings row `MeshGenerationFailed` naming `BLDG_DEGENERATE_BOWTIE`; exportable False; `generator_diagnostic` layer present for map inspection |
| deployment | plugin copied into `%APPDATA%/QGIS/QGIS4/profiles/default/python/plugins/scau_preproc_workbench` |

GUI-side visual confirmation (enable plugin, run dialog, inspect layers) is
operator-executed in QGIS; the plugin shell contains no logic beyond
rendering `jobio` outputs, which the headless runs above certify.

## Remaining E-slices (phase E stays IN PROGRESS)

- data-catalog detail page, field-mapping editor (canonical-target-only
  dropdowns), semantic/physical parameter tables, coupling relation editor
  (map + confirm/reject over the M287-C mapping JSONs), full report browser;
- packaging polish (icons, translations) and a Plugin Reloader workflow note.

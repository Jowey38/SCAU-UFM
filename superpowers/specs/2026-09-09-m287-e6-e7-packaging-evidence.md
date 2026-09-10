# M287-E6/E7 Plugin Packaging Evidence

Date: 2026-09-09
Scope: UFM-Mapper master plan v3 §8.4 "E6/E7 打包收口": icon, translation hook,
Plugin Reloader / deployment notes, dual-version smoke checklist, `project.ufm.json`
project index. Plugin 0.12.0. UI-only slice, no pipeline change, no gate.

## Deliverables

- `icon.svg` (toolbar + `metadata.txt icon=`); metadata gains `tags`, `changelog`,
  page-oriented `about` (P1–P8).
- **i18n hook**: `plugin.py` installs `i18n/scau_preproc_workbench_<locale>.qm`
  when present (locale from `QgsSettings locale/userLocale`); removed on unload.
  `i18n/scau_preproc_workbench_en.ts` English scaffold + `i18n/README.md`
  (`pylupdate6` / `lrelease` workflow). No `.qm` is committed: every locale shows
  the authored Chinese UI until strings are wrapped in `tr()` — recorded as such.
- **`project.ufm.json` (E7)**: regenerable, `authoritative: false` index written
  beside the output root (default `<output_dir>/../project.ufm.json`, overridable
  on the job page). `update_project_index` merges jobs keyed by `output_dir`
  (status, `case_sha256`, finding count, artefact paths incl. export target) and
  snapshots the dialog fields (`ui_state`); every pipeline run refreshes it;
  "打开工程" restores the fields (`_apply_ui_state`). Nothing downstream reads it.
- `tools/qgis/SMOKE_CHECKLIST.md`: dual-track (4.0 / 3.40 LTR) checklist, wrapper
  recipe, Plugin Reloader + deployment notes; `deploy_plugin.sh` takes an optional
  plugins dir so the same script serves the QGIS 3 profile.
- `offscreen_smoke.py` gains `project_index_roundtrip`.
- `python/tests/test_jobio_project_index.py` (2) → 75 headless tests on this branch.

## Evidence

1. Headless: index accumulates two output dirs, replaces a re-run dir, records
   status/hash/artefacts, snapshots `terrain_condition` / `coupling_maps` blocks;
   invalid or missing index is `None` / `NoProjectIndex`.
2. **QGIS 4.0.0 offscreen** (`--run`): construct, data_tree, report_browser,
   `project_index_roundtrip` (save with `lc=6.5`, change to 8.0, open → 6.5
   restored, package path restored), run_pipeline `PipelineOk`, tree/report after
   run — all `ok`. Toolbar icon / translator load are interactive-only items.
3. **QGIS 3.40 LTR**: not installed on the development host; the checklist row is
   open and says so. `qgisMinimumVersion=3.40` remains a declaration, not evidence.

## Boundaries kept

- `project.ufm.json` is convenience state: deleting it loses nothing that
  `job_config.json` / `validation.json` / `pipeline_manifest.json` carry.
- Deployment stays a copy; the repo root is a validated dialog field.

## Follow-ups

- Wrap dialog strings in `self.tr()` and commit a `.qm` when an English UI is
  requested; install QGIS 3.40 LTR on a host and tick the second smoke row.

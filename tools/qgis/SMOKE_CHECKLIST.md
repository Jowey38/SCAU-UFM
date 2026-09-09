# QGIS dual-version smoke checklist (E6/E7)

Run after every plugin change on **both** tracks; paste the `SMOKE …` lines of
`tools/qgis/offscreen_smoke.py` as evidence.

| Track | Interpreter | Status (2026-09-09) |
|---|---|---|
| QGIS 4.0.0 (Qt6, python-qgis.bat) | `"<QGIS 4>/bin/python-qgis.bat"` | **executed**: construct, data_tree, report_browser, parameter_tables, field_mapping, project_index_roundtrip, run_pipeline, *_after_run all `ok`; `--exercise-edits` writes exactly the three `v001` files |
| QGIS 3.40 LTR (Qt5, python-qgis-ltr.bat) | `"<QGIS 3.40>/bin/python-qgis-ltr.bat"` | **not installed on the development host** — item open; the plugin declares `qgisMinimumVersion=3.40` and uses only `qgis.PyQt` enum forms valid on Qt5/Qt6 |

## Recipe (Windows, from Git Bash via a .bat wrapper)

```bat
@echo off
set QT_QPA_PLATFORM=offscreen
call "<QGIS>\bin\python-qgis.bat" "<checkout>\tools\qgis\offscreen_smoke.py" --repo-root "<checkout>" --run
```

`cmd //c <wrapper.bat>`; create any temp fixture with `os.environ["TEMP"]` so bash and cmd see the same path.
Add `--exercise-edits --package <COPY of the sample>` to drive the P5/P3 save slots;
add `--terrain-policy <policy.json>` for the B3 field.

## Per-track items

- [ ] `construct_dialog` — dialog builds with the checkout repo root; all 7 tabs present
- [ ] `data_tree` / `report_browser` — sample package renders; Terrain/1D lamps as documented
- [ ] `parameter_tables` / `field_mapping` — P5/P3 tables load
- [ ] `project_index_roundtrip` — `project.ufm.json` save → edit field → open restores it
- [ ] `run_pipeline` (`--run`) — `PipelineOk`; export gate label updates; `project.ufm.json` refreshed beside the output dir
- [ ] `--exercise-edits` on a package copy — `dpm_rule_table.v001.json` (approval reset), `soil_parameters.v001.csv`, `field_mapping.v001.json`
- [ ] Interactive only: toolbar icon shows `icon.svg`; menu entry under `SCAU-UFM`; Plugin Reloader picks up a redeploy

## Deployment / reload

- Deploy is a **copy** (`tools/qgis/deploy_plugin.sh [repo_root] [profile_plugins_dir]`): never infer the repo root from `__file__`; the dialog field is authoritative and persisted (`QgsSettings`).
- Install **Plugin Reloader** from the QGIS repository, pick `scau_preproc_workbench`, press reload after each `deploy_plugin.sh`; a `QTranslator` is (re)installed per load.
- Profiles: QGIS 4 `%APPDATA%\QGIS\QGIS4\profiles\default\python\plugins`, QGIS 3 `%APPDATA%\QGIS\QGIS3\profiles\default\python\plugins`.
- `PYTHONHOME`/`PYTHONPATH` exported by the QGIS host are scrubbed before the `py -3` pipeline subprocess (jobio.subprocess_env).

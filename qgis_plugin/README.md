# SCAU PreProc Workbench (QGIS plugin)

M287-E first slice: a STATELESS driver UI over the `scau_preproc` pipeline.
The plugin renders user choices into `job_config.json`, runs
`py -3 -m scau_preproc.pipeline` as a subprocess (system python owns
gmsh/netCDF4; the subprocess boundary is the state/fault/GPL boundary), then
re-reads `validation.json` and displays findings, the export gate, and the
package/diagnostic GeoJSON layers. No computation state lives in the UI.

## Deploy

```bash
tools/qgis/deploy_plugin.sh          # copies into the QGIS4 default profile
```

Then in QGIS: Plugins -> Manage and Install Plugins -> Installed -> enable
"SCAU PreProc Workbench" (experimental). A toolbar action opens the dialog.

## E1 slice scope

- job page: package dir / output dir / optional validator CLI, characteristic
  length, recombination, determinism check, coupling-map stage toggle;
- run: subprocess pipeline with UI timeout; findings table from
  `validation.json` (fatal findings and timeout render as rows, never hidden);
- export gate label: enabled only on a clean `status: ok` (plan page 8 rule);
- layer loading: boundary/buildings/landcover + generator diagnostic GeoJSON.

Remaining pages (data catalog detail, field mapping editor, semantic tables,
coupling relation editor, full report browser) are later E-slices; their
backend contracts (mapping JSONs, reports) already exist from M287-B/C.

## Requirements

- QGIS 3.40+/4.x (Qt6 supported; tested against QGIS 4.0.0);
- system `py -3` with `gmsh` + `netCDF4` (the pipeline runtime);
- a build of `scau_preproc.exe` for the optional authoritative validate step.

# N1-D P10 / N2-D P11 workbench evidence

Date: 2026-09-10

## Delivered

- Added pure `jobio` helpers for draft GeoJSON validation, authored write actions, and job configuration checks.
- P10 adds authored/external SWMM mode controls. The modes are mutually exclusive; external input is copied through the existing `swmm_author` path and authored output is written through the same governed author.
- P11 adds river sketch preflight and UGRID/MDU authoring through `dflowfm_author`. The page displays the count of `provider_required` hydraulic groups and does not invent hydraulic fields.
- Added P10/P11 tabs and buttons to the existing QGIS dialog style. Runtime coupling was not changed.

## Verification

- `PYTHONPATH=python py -3 -m unittest python/tests/test_jobio_network_workbench.py`
- Result: 2 tests passed.
- Existing author tests remain separately covered by the N1/N2 evidence documents.

The QGIS shell remains a thin renderer; all validation and authoring decisions are delegated to pure jobio helpers and the existing governed author modules.

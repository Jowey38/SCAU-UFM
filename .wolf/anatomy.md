# anatomy.md

## C3 provider contract (branch feat/c3-provider-contract, 2026-09-26)

- `libs/coupling/driver/include/coupling/driver/dflowfm_provider_contract.hpp` + `src/…cpp` — frozen C3 contract: `DFlowFMProviderContractSnapshot`, `DFlowFMBoundaryIdentity`, `DFlowFMNativeEpochObservation`; `validate_dflowfm_provider_contract`, `validate_dflowfm_native_series` (reason codes `dflowfm_contract_*`).
- `apps/sim_driver/run_loop.{hpp,cpp}` — `RunLoopHooks::dflowfm_native_observation`; `validate_dflowfm_contract_for_config` (public); identity check before case load; native series validated in the commit gate.
- `apps/sim_driver/run_summary.{hpp,cpp}` — `DFlowFMBoundaryRecord`, `DFlowFMNativeEpochRecord`; summary `dflowfm` block + per-epoch `dflowfm_native` JSON.
- `apps/sim_driver/main.cpp` — real mode binds `observe_dflowfm_external_net` as the native provider.
- `tests/unit/coupling/test_dflowfm_provider_contract_unit.cpp` — 8 validator cases.
- `tests/integration/sim_driver/test_sim_driver_run_loop.cpp` — 4 C3 run-loop cases (mock engines, fixture MDU in temp dir).
- `tests/golden/dflowfm_provider_contract/` — **G39** (CI gate): driver artefact → `scau_results link` → provenance_validated / NO_GAP / tampered-MDU refusal.
- `tests/golden/surface2d_tri_coupling_real/…` — G19 binds native provider, asserts native api-lateral == ledger.
- `python/scau_results/linked.py` — `_bind_dflowfm_contract`, `_validate_native_series`, `dflowfm_accounting`; `linked_view(..., dflowfm_mdu)`; CLI `--dflowfm-mdu`.
- `qgis_plugin/scau_preproc_workbench/jobio.py` — `run_linked_view(dflowfm_mdu=)`, `dflowfm_linked_rows`; `workbench_dialog.py` — `_results_mdu` field.
- `tools/qgis/offscreen_smoke.py` — `--c3-summary/--c3-mdu` block.
- `spikes/dflowfm/provider_contract/interface.md` — status now implemented + verified; scope narrowing documented.

> Auto-maintained by OpenWolf. Last scanned: 2026-09-25T03:07:44.147Z
> Files: 75 tracked | Anatomy hits: 0 | Misses: 0

## ../../scau-b7-close-20260910/

- `dual-pr-body.md` (~199 tok)
- `dual-real.conf` (~175 tok)
- `dual-results.conf` (~202 tok)
- `dual-swmm.inp` (~165 tok)
- `provider-pr-body.md` (~146 tok)
- `qgis-smoke-close.cmd` (~52 tok)
- `runner-git-proxy.ps1` (~159 tok)
- `runner-net-diag.ps1` (~292 tok)

## ../../scau-b7-close-20260910/apps/sim_driver/

- `main.cpp` — include <exception> (~1696 tok)
- `run_loop.cpp` — include "run_loop.hpp" (~10383 tok)
- `run_loop.hpp` — pragma once (~1032 tok)
- `run_summary.cpp` — include "run_summary.hpp" (~2159 tok)
- `run_summary.hpp` — pragma once (~1281 tok)
- `surface_timeseries.cpp` — include "surface_timeseries.hpp" (~2507 tok)

## ../../scau-b7-close-20260910/libs/coupling/drainage/include/coupling/drainage/

- `swmm_engine.hpp` — pragma once (~1209 tok)

## ../../scau-b7-close-20260910/libs/coupling/drainage/src/swmm_adapter/

- `swmm_engine.cpp` — include "coupling/drainage/swmm_engine.hpp" (~3465 tok)

## ../../scau-b7-close-20260910/python/scau_results/

- `__main__.py` — Inspect and summarize completed surface timeseries without modifying inputs. (~4468 tok)
- `geotiff.py` — Dependency-free baseline GeoTIFF export of cell-wise derived maps. (~3164 tok)
- `linked.py` — Read-only 1D/2D linked view: CouplingLib ledger per link vs engine-native summaries. (~4526 tok)
- `README.md` — Project documentation (~2332 tok)

## ../../scau-b7-close-20260910/python/tests/

- `test_jobio_network_workbench.py` — Headless tests for P10/P11 pure jobio helpers. (~3118 tok)
- `test_results_geotiff.py` — GeoTiffTests: read_tiff, test_rasterize_matches_analytic_cells_and_is_deterministic, test_geotiff_by (~2451 tok)
- `test_results_linked.py` — ReportParserTests: summary, link, test_reads_both_tables_units_and_section_scoped_continuity, test_u (~3065 tok)
- `test_results.py` — ResultsTests: fixture, write_manifest, add_mesh, test_spatial_sampling_locates_cells_and_fails_close (~3501 tok)

## ../../scau-b7-close-20260910/qgis_plugin/scau_preproc_workbench/

- `jobio.py` — Pure-python job/report I/O for the SCAU PreProc Workbench plugin. (~26773 tok)
- `workbench_dialog.py` — SCAU PreProc Workbench dialog (M287-E1 job page + M287-E3 mesh workbench). (~20864 tok)

## ../../scau-b7-close-20260910/tests/integration/sim_driver/

- `test_sim_driver_run_loop.cpp` — include <cstdlib> (~4574 tok)

## ../../scau-b7-close-20260910/tools/qgis/

- `offscreen_smoke.py` — Headless (offscreen) smoke test of the SCAU PreProc Workbench plugin. (~3594 tok)

## ./

- `开发收口任务清单.md` — 开发收口任务清单 (~594 tok)

## .wolf/


## apps/sim_driver/

- `CMakeLists.txt` (~209 tok)
- `main.cpp` — include <exception> (~1578 tok)
- `run_loop.cpp` — include "run_loop.hpp" (~10201 tok)
- `run_loop.hpp` — pragma once (~975 tok)
- `run_summary.cpp` — include "run_summary.hpp" (~1995 tok)
- `run_summary.hpp` — pragma once (~1142 tok)
- `runtime_config_io.cpp` — include "runtime_config_io.hpp" (~3205 tok)
- `sim_driver.cpp` — include "sim_driver.hpp" (~2383 tok)
- `sim_driver.hpp` — pragma once (~1299 tok)
- `surface_timeseries.cpp` — include "surface_timeseries.hpp" (~2089 tok)
- `surface_timeseries.hpp` — pragma once (~363 tok)

## docs/superpowers/plans/

- `2026-09-10-m288-failure-revealing-review.md` — M288 failure-revealing design review (~1115 tok)

## extern/swmm5/src/solver/include/


## libs/coupling/driver/

- `CMakeLists.txt` (~230 tok)

## libs/coupling/driver/include/coupling/driver/

- `dflowfm_provider_contract.hpp` — pragma once (~256 tok)
- `tri_coupling.hpp` — pragma once (~2399 tok)

## libs/coupling/driver/src/

- `dflowfm_provider_contract.cpp` — include "coupling/driver/dflowfm_provider_contract.hpp" (~1109 tok)
- `tri_coupling.cpp` — include "coupling/driver/tri_coupling.hpp" (~5633 tok)

## python/scau_preproc/

- `terrain_condition.py` — Terrain (DEM) conditioning under explicit policy authorization (M287-B3, stage B). (~7897 tok)

## python/scau_results/

- `__init__.py` — Validated, read-only surface result processing. (~16 tok)
- `__main__.py` — Inspect and summarize completed surface timeseries without modifying inputs. (~4068 tok)
- `geotiff.py` — Dependency-free baseline GeoTIFF export of cell-wise derived maps. (~2462 tok)
- `linked.py` — Read-only 1D/2D linked view: CouplingLib ledger per link vs engine-native summaries. (~2232 tok)
- `README.md` — Project documentation (~1776 tok)

## python/tests/

- `test_case_export.py` — Headless unit tests for scau_preproc.case_export (M287-B6). (~2369 tok)
- `test_results_geotiff.py` — GeoTiffTests: read_tiff, test_rasterize_matches_analytic_cells_and_is_deterministic, test_geotiff_by (~1929 tok)
- `test_results_linked.py` — LinkedViewTests: summary, link, test_rpt_parser_reads_both_tables_and_units, test_ledger_aggregates_ (~2244 tok)
- `test_results.py` — ResultsTests: fixture, write_manifest, add_mesh, test_spatial_sampling_locates_cells_and_fails_close (~2576 tok)
- `test_terrain_condition.py` — Headless unit tests for scau_preproc.terrain_condition (M287-B3). (~4043 tok)

## qgis_plugin/scau_preproc_workbench/

- `workbench_dialog.py` — SCAU PreProc Workbench dialog (M287-E1 job page + M287-E3 mesh workbench). (~20425 tok)

## spikes/dflowfm/


## spikes/dflowfm/provider_contract/

- `capability.json` (~192 tok)
- `interface.md` — D-Flow FM provider contract (C3/B4') (~698 tok)
- `validation_report.md` — Provider native-boundary validation (~613 tok)

## superpowers/specs/

- `2026-09-10-dual-model-run-evidence.md` — B1/B2' surface + SWMM execution evidence (~657 tok)
- `2026-09-10-project-closure-evidence.md` — Project closure evidence index (~752 tok)

## tests/golden/preproc_terrain_condition_case/cases/

- `terrain_condition_report.json` (~729 tok)

## tests/golden/swmm_external_net/


## tests/integration/sim_driver/

- `CMakeLists.txt` (~688 tok)
- `run_drainage_only.cmake` — Execute the exported synthetic package with real SWMM and no D-Flow runtime. (~456 tok)
- `test_sim_driver_run_loop.cpp` — include <cstdlib> (~4339 tok)
- `test_sim_driver_whole_system_mass_audit.cpp` — include <cstdlib> (~2957 tok)

## tests/unit/core/

- `test_runtime_config_io.cpp` — include <stdexcept> (~1758 tok)

## tests/unit/coupling/

- `CMakeLists.txt` (~5975 tok)
- `fake_dflowfm_bmi.cpp` — include <cstring> (~1143 tok)
- `test_coupling_dflowfm_engine.cpp` — include <limits> (~2172 tok)
- `test_dflowfm_provider_contract.cpp` — include <gtest/gtest.h> (~619 tok)

## third_party/patches/


## tools/qgis/

- `offscreen_smoke.py` — Headless (offscreen) smoke test of the SCAU PreProc Workbench plugin. (~3290 tok)
- `SMOKE_CHECKLIST.md` — QGIS dual-version smoke checklist (E6/E7) (~772 tok)

"""Headless (offscreen) smoke test of the SCAU PreProc Workbench plugin.

Runs inside a QGIS python (python-qgis.bat / python-qgis-ltr.bat), e.g.

  QT_QPA_PLATFORM=offscreen "<QGIS>/bin/python-qgis.bat" tools/qgis/offscreen_smoke.py \
      --repo-root <checkout> [--package samples/d5_gis_preproc_template] [--output <dir>] [--run]

It instantiates WorkbenchDialog from the CHECKOUT (not the deployed profile
copy), fills the job page with the sample package, and drives the private
slots that do not need a map canvas: data tree refresh, report refresh,
parameter-table / field-mapping pages, and optionally the pipeline run.
Exit code 0 = every step succeeded; every step prints one `SMOKE <step> ok|FAIL`
line so the run can be pasted into the dual-version checklist
(tools/qgis/SMOKE_CHECKLIST.md).
"""

from __future__ import annotations

import argparse
import os
import sys
import tempfile
import traceback
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", required=True)
    parser.add_argument("--package", default="samples/d5_gis_preproc_template")
    parser.add_argument("--output", default=None)
    parser.add_argument("--run", action="store_true", help="also run the pipeline subprocess")
    parser.add_argument("--terrain-policy", default=None, help="B3 policy path for the job page field")
    parser.add_argument("--exercise-edits", action="store_true",
                        help="P5/P3: edit a cell and save versioned rule/soil/mapping files (use on a COPY of the package)")
    parser.add_argument("--c3-summary", default=None,
                        help="run_summary.json of a REAL surface+SWMM+D-Flow run (C3 linked view)")
    parser.add_argument("--c3-mdu", default=None, help="the D-Flow FM .mdu that run hashed")
    parser.add_argument("--results-nc", default=None,
                        help="P9: a completed surface timeseries (with sidecar manifest) to validate, sample and rasterize")
    args = parser.parse_args()
    repo = Path(args.repo_root).resolve()
    package = (repo / args.package).resolve() if not Path(args.package).is_absolute() else Path(args.package)
    output = Path(args.output) if args.output else Path(tempfile.mkdtemp(prefix="scau_smoke_"))

    os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
    from qgis.core import Qgis, QgsApplication
    app = QgsApplication([], False)
    app.initQgis()
    print(f"SMOKE qgis_version {Qgis.QGIS_VERSION}")
    sys.path.insert(0, str(repo / "qgis_plugin"))
    failures = 0

    def step(name, fn):
        nonlocal failures
        try:
            fn()
            print(f"SMOKE {name} ok")
        except Exception:  # noqa: BLE001 - report every step
            failures += 1
            print(f"SMOKE {name} FAIL")
            traceback.print_exc()

    dialog = None

    def construct():
        nonlocal dialog
        from scau_preproc_workbench.workbench_dialog import WorkbenchDialog
        dialog = WorkbenchDialog(str(repo))
        dialog._repo_edit.setText(str(repo))
        dialog._package_edit.setText(str(package))
        dialog._output_edit.setText(str(output))
        dialog._validator_edit.setText("")
        if args.terrain_policy:
            dialog._terrain_policy_edit.setText(args.terrain_policy)
        job = dialog._current_job()
        assert job["package"] == str(package), job

    step("construct_dialog", construct)
    step("data_tree", lambda: dialog._refresh_data_tree())
    step("report_browser", lambda: dialog._refresh_reports())
    if hasattr(dialog, "_refresh_parameter_tables"):
        step("parameter_tables", lambda: dialog._refresh_parameter_tables())
    if hasattr(dialog, "_refresh_field_mapping"):
        step("field_mapping", lambda: dialog._refresh_field_mapping())
    if hasattr(dialog, "_save_project"):
        def project_roundtrip():
            dialog._project_edit.setText(str(output / "project.ufm.json"))
            dialog._lc_spin.setValue(6.5)
            dialog._save_project()
            dialog._lc_spin.setValue(8.0)
            dialog._open_project()
            assert abs(dialog._lc_spin.value() - 6.5) < 1e-9, dialog._lc_spin.value()
            assert dialog._package_edit.text() == str(package)
            dialog._lc_spin.setValue(8.0)
        step("project_index_roundtrip", project_roundtrip)
    if args.exercise_edits and hasattr(dialog, "_save_rule_table_version"):
        def edits():
            from qgis.PyQt.QtWidgets import QTableWidgetItem
            dialog._rule_table_edit.setText(str(package / "metadata" / "dpm_rule_table.json"))
            dialog._param_editor_edit.setText("smoke")
            dialog._mapping_editor_edit.setText("smoke")
            dialog._refresh_parameter_tables()
            assert dialog._class_table.rowCount() >= 1
            def findings():
                return [(dialog._findings.item(r, 1).text(), dialog._findings.item(r, 2).text()[:200])
                        for r in range(dialog._findings.rowCount())]
            dialog._class_table.setItem(0, 4, QTableWidgetItem("0.9"))   # grass phi_yy: legal edit
            dialog._save_rule_table_version()
            print("SMOKE after_rule_save", findings())
            dialog._save_soil_table_version()
            print("SMOKE after_soil_save", findings())
            dialog._refresh_field_mapping()
            assert dialog._mapping_table.rowCount() >= 1
            dialog._save_field_mapping()
            print("SMOKE after_mapping_save", findings())
            written = sorted(p.name for p in (package / "metadata").glob("*.v001.*")) +                       sorted(p.name for p in (package / "soil").glob("*.v001.*"))
            print("SMOKE versioned_files", written)
            assert written == ["dpm_rule_table.v001.json", "field_mapping.v001.json", "soil_parameters.v001.csv"], written
            codes = [dialog._findings.item(r, 1).text() for r in range(dialog._findings.rowCount())]
            assert "FieldMappingVersionWritten" in codes, codes
        step("parameter_and_mapping_edits", edits)
    if args.results_nc and hasattr(dialog, "_run_results"):
        def results():
            from qgis.core import QgsProject
            dialog._results_nc.setText(args.results_nc)
            dialog._results_points.setText("58.34 11.54")      # centroid of cell 379 in the synthetic case
            dialog._results_pixel.setValue(2.0)
            dialog._results_geotiff.setChecked(True)
            dialog._run_results()
            rows = [(dialog._findings.item(r, 1).text(), dialog._findings.item(r, 2).text()[:160])
                    for r in range(dialog._findings.rowCount())]
            print("SMOKE results_findings", rows)
            codes = [c for c, _ in rows]
            assert "ResultsValidated" in codes, codes
            assert "SampledCells" in codes and any("cells=[379]" in d for c, d in rows if c == "SampledCells"), rows
            assert codes.count("RasterLoaded") == 3 and "RasterInvalid" not in codes, codes
            assert dialog._results_series.rowCount() == 4, dialog._results_series.rowCount()
            layers = [l.name() for l in QgsProject.instance().mapLayers().values() if l.name().startswith("scau_")]
            print("SMOKE results_layers", sorted(layers))
            assert {"scau_max_depth_m", "scau_arrival_time_s", "scau_duration_s"} <= set(layers), layers
            # fail-closed path: a byte-tampered copy (same file name, so the
            # manifest's output name matches) must trip the output_hash check
            import shutil, tempfile as _tf
            tdir = Path(_tf.mkdtemp(prefix="scau_tamper_"))
            bad = tdir / Path(args.results_nc).name
            shutil.copyfile(args.results_nc, bad)
            shutil.copyfile(args.results_nc + ".manifest.json", str(bad) + ".manifest.json")
            with open(bad, "r+b") as fh:
                fh.seek(-8, 2); fh.write(b"\x00" * 8)
            dialog._results_nc.setText(str(bad)); dialog._results_points.setText("")
            dialog._run_results()
            codes = [dialog._findings.item(r, 1).text() for r in range(dialog._findings.rowCount())]
            detail = dialog._findings.item(0, 2).text()
            print("SMOKE results_tampered", codes, detail[:120])
            assert codes == ["ResultsRejected"], codes
            assert "output bytes do not match manifest" in detail, detail
            # 1D/2D linked view: run_summary + SWMM .rpt beside the results file
            nc_path = Path(args.results_nc)
            summary_path = nc_path.with_name(nc_path.name.replace(".nc", "-summary.json"))
            rpt_path = nc_path.with_name("dual-swmm.rpt")
            if summary_path.is_file() and rpt_path.is_file():
                dialog._results_nc.setText(args.results_nc)
                dialog._results_summary.setText(str(summary_path))
                dialog._results_rpt.setText(str(rpt_path))
                dialog._run_linked_view()
                codes = [dialog._findings.item(r, 1).text() for r in range(dialog._findings.rowCount())]
                print("SMOKE linked_findings", codes)
                assert codes[0] == "LinkedView" and dialog._findings.item(0, 0).text() == "pass", codes
                assert "SwmmReportBound" in codes, codes            # report bytes proven to be this run's
                assert codes.count("ROUTING_GAP_ATTRIBUTION_INSUFFICIENT") == 2, codes   # J1 and J2, no attribution
                assert "KNOWN_ROUTING_CONTINUITY_GAP" not in codes, codes
                # C3: a surface+SWMM run has no river engine; the D-Flow row must
                # say so rather than pretend a native result was consumed
                dfl = [dialog._findings.item(r, 2).text() for r in range(dialog._findings.rowCount())
                       if dialog._findings.item(r, 1).text() == "DFlowNative"]
                print("SMOKE linked_dflowfm", dfl)
                assert dfl == ["river engine not part of this run"], dfl
                assert dialog._results_linked.rowCount() == 2, dialog._results_linked.rowCount()
                nodes = [dialog._results_linked.item(r, 1).text() for r in range(2)]
                print("SMOKE linked_nodes", nodes, [(dialog._results_linked.item(r, 6).text(),
                                                     dialog._results_linked.item(r, 8).text()) for r in range(2)])
                assert nodes == ["J1", "J2"], nodes
                # canvas picking must refuse when the model CRS is undeclared (synthetic package)
                dialog._pick_result_point()
                pick = [dialog._findings.item(r, 1).text() for r in range(dialog._findings.rowCount())]
                print("SMOKE canvas_pick_offscreen", pick)
                assert pick == ["NoCanvas"], pick                   # offscreen: no iface; never passes project coords
                # a surface result from a DIFFERENT run must be refused by state hash
                import json as _json
                other = tdir / "other.nc"
                other.write_bytes(b"")
                Path(str(other) + ".manifest.json").write_text(_json.dumps(
                    {"final_surface_state_hash": "fnv1a64:0000000000000000"}), encoding="utf-8")
                dialog._results_nc.setText(str(other))
                dialog._run_linked_view()
                codes = [dialog._findings.item(r, 1).text() for r in range(dialog._findings.rowCount())]
                detail = dialog._findings.item(0, 2).text()
                print("SMOKE linked_wrong_run", codes, detail[:100])
                # full result validation runs first, so a fabricated manifest is
                # refused at schema/bytes before the state-hash comparison
                assert codes == ["LinkedViewRejected"], codes
                assert any(k in detail for k in ("different run", "manifest", "provenance", "does not exist")), detail
                assert dialog._results_linked.rowCount() == 0
            else:
                print("SMOKE linked_view skipped (no summary/rpt beside results)")
            shutil.rmtree(tdir)
        step("results_page", results)
    if args.c3_summary and hasattr(dialog, "_run_linked_view"):
        def c3():
            import shutil, tempfile as _tf
            summary = Path(args.c3_summary)
            nc = summary.with_name("surface.nc")
            dialog._results_nc.setText(str(nc) if Path(str(nc) + ".manifest.json").is_file() else "")
            dialog._results_summary.setText(str(summary))
            dialog._results_rpt.setText("")
            dialog._results_mdu.setText(args.c3_mdu or "")
            dialog._run_linked_view()
            rows = [(dialog._findings.item(r, 0).text(), dialog._findings.item(r, 1).text(),
                     dialog._findings.item(r, 2).text()[:200]) for r in range(dialog._findings.rowCount())]
            print("SMOKE c3_findings", rows)
            codes = [c for _, c, _ in rows]
            lamps = {c: l for l, c, _ in rows}
            assert codes[0] == "LinkedView" and lamps["LinkedView"] == "pass", rows
            assert lamps.get("C3Contract") == "pass", rows
            assert lamps.get("DFlowNative") == "pass", rows          # provenance_validated (MDU bound)
            assert "DFlowLateralAccounting:NO_GAP" in codes, codes
            assert any("provenance_validated" in d for _, c, d in rows if c == "DFlowNative"), rows
            assert any("bound by bytes" in d for _, c, d in rows if c == "C3Contract"), rows
            engines = [dialog._results_linked.item(r, 0).text() for r in range(dialog._results_linked.rowCount())]
            print("SMOKE c3_link_engines", engines)
            assert "river" in engines and "drainage" in engines, engines
            # a byte-tampered MDU is not this run's river input -> the whole view is refused
            if args.c3_mdu:
                tdir = Path(_tf.mkdtemp(prefix="scau_c3_tamper_"))
                bad = tdir / Path(args.c3_mdu).name
                shutil.copyfile(args.c3_mdu, bad)
                with open(bad, "ab") as fh:
                    fh.write(b"\n# tampered\n")
                dialog._results_mdu.setText(str(bad))
                dialog._run_linked_view()
                codes = [dialog._findings.item(r, 1).text() for r in range(dialog._findings.rowCount())]
                detail = dialog._findings.item(0, 2).text()
                print("SMOKE c3_tampered_mdu", codes, detail[:120])
                assert codes == ["LinkedViewRejected"], codes
                assert "MDU bytes do not match" in detail, detail
                assert dialog._results_linked.rowCount() == 0
                shutil.rmtree(tdir)
        step("c3_linked_view", c3)
    if args.run:
        def run():
            dialog._run()
            rows = [(dialog._findings.item(r, 1).text(), dialog._findings.item(r, 2).text()[-400:])
                    for r in range(dialog._findings.rowCount())]
            print("SMOKE findings", [code for code, _ in rows])
            assert dialog._findings.rowCount() > 0
            if any(code == "NoValidationReport" for code, _ in rows):
                raise RuntimeError("pipeline wrote no validation.json: " + "; ".join(d for _, d in rows))
        step("run_pipeline", run)
        step("data_tree_after_run", lambda: dialog._refresh_data_tree())
        step("report_browser_after_run", lambda: dialog._refresh_reports())
    app.exitQgis()
    print(f"SMOKE result {'ok' if failures == 0 else f'{failures} failure(s)'} output={output}")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main())

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

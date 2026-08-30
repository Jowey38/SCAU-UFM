"""SCAU PreProc Workbench dialog (M287-E1 slice).

Stateless shell over jobio: render user choices -> job_config.json -> run the
pipeline subprocess -> display validation findings -> load map layers. The
dialog holds no computation state; every run re-reads reports from disk.
"""

from __future__ import annotations

from pathlib import Path

from qgis.core import QgsProject, QgsVectorLayer
from qgis.PyQt.QtCore import Qt
from qgis.PyQt.QtWidgets import (
    QCheckBox,
    QDialog,
    QDoubleSpinBox,
    QFileDialog,
    QGridLayout,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QPushButton,
    QTableWidget,
    QTableWidgetItem,
    QVBoxLayout,
)

from . import jobio


class WorkbenchDialog(QDialog):
    def __init__(self, repo_root: str, parent=None) -> None:
        super().__init__(parent)
        self._repo_root = repo_root
        self.setWindowTitle("SCAU PreProc Workbench (M287-E1)")
        self.resize(760, 520)

        grid = QGridLayout()
        self._package_edit = QLineEdit(str(Path(repo_root) / "samples/d5_gis_preproc_template"))
        self._output_edit = QLineEdit()
        self._validator_edit = QLineEdit(
            str(Path(repo_root) / "build/windows-msvc/apps/preproc_cli/Debug/scau_preproc.exe"))
        for row, (label, edit) in enumerate((
            ("输入包目录", self._package_edit),
            ("输出目录", self._output_edit),
            ("validator CLI（可选）", self._validator_edit),
        )):
            grid.addWidget(QLabel(label), row, 0)
            grid.addWidget(edit, row, 1)
            browse = QPushButton("...")
            browse.clicked.connect(lambda _=False, e=edit: self._browse(e))
            grid.addWidget(browse, row, 2)

        self._lc_spin = QDoubleSpinBox()
        self._lc_spin.setRange(0.01, 1000.0)
        self._lc_spin.setValue(8.0)
        self._lc_spin.setSuffix(" m")
        self._recombine_check = QCheckBox("四边形重组 (Blossom)")
        self._recombine_check.setChecked(True)
        self._determinism_check = QCheckBox("确定性复跑校验")
        self._determinism_check.setChecked(True)
        self._coupling_check = QCheckBox("生成耦合映射（阶段 E）")
        options = QHBoxLayout()
        options.addWidget(QLabel("特征长度"))
        options.addWidget(self._lc_spin)
        options.addWidget(self._recombine_check)
        options.addWidget(self._determinism_check)
        options.addWidget(self._coupling_check)
        options.addStretch()

        self._run_button = QPushButton("运行流水线")
        self._run_button.clicked.connect(self._run)
        self._load_button = QPushButton("加载图层")
        self._load_button.clicked.connect(self._load_layers)
        self._export_label = QLabel("导出门禁：未运行")
        buttons = QHBoxLayout()
        buttons.addWidget(self._run_button)
        buttons.addWidget(self._load_button)
        buttons.addWidget(self._export_label)
        buttons.addStretch()

        self._findings = QTableWidget(0, 3)
        self._findings.setHorizontalHeaderLabels(["severity", "code", "detail"])
        self._findings.horizontalHeader().setStretchLastSection(True)
        self._findings.setEditTriggers(QTableWidget.EditTrigger.NoEditTriggers)

        layout = QVBoxLayout(self)
        layout.addLayout(grid)
        layout.addLayout(options)
        layout.addLayout(buttons)
        layout.addWidget(self._findings)

    def _browse(self, edit: QLineEdit) -> None:
        chosen = QFileDialog.getExistingDirectory(self, "选择目录", edit.text())
        if chosen:
            edit.setText(chosen)

    def _current_job(self) -> dict:
        validator = self._validator_edit.text().strip()
        return jobio.build_job_config(
            self._package_edit.text().strip(),
            self._output_edit.text().strip(),
            characteristic_length_m=self._lc_spin.value(),
            recombine=self._recombine_check.isChecked(),
            determinism_check=self._determinism_check.isChecked(),
            coupling_maps=self._coupling_check.isChecked(),
            validator_cli=validator if validator and Path(validator).is_file() else None,
        )

    def _run(self) -> None:
        if not self._output_edit.text().strip():
            self._show_rows([("fatal", "MissingOutputDir", "请选择输出目录")])
            return
        job = self._current_job()
        job_path = jobio.write_job_config(job, job["output_dir"])
        self._run_button.setEnabled(False)
        self._run_button.setText("运行中…")
        try:
            result = jobio.run_pipeline(job_path, self._repo_root)
        finally:
            self._run_button.setEnabled(True)
            self._run_button.setText("运行流水线")
        rows = jobio.findings_rows(result.get("validation"))
        if result["status"] not in ("ok", "fatal") and result.get("stderr"):
            rows.insert(0, ("fatal", result["status"], result["stderr"]))
        self._show_rows(rows)
        can_export = jobio.exportable(result.get("validation"))
        self._export_label.setText(
            "导出门禁：允许 (status=ok)" if can_export else "导出门禁：禁止（存在 fatal/未完成）")

    def _show_rows(self, rows) -> None:
        self._findings.setRowCount(len(rows))
        for r, row in enumerate(rows):
            for c, value in enumerate(row):
                item = QTableWidgetItem(value)
                item.setFlags(item.flags() & ~Qt.ItemFlag.ItemIsEditable)
                self._findings.setItem(r, c, item)

    def _load_layers(self) -> None:
        job = self._current_job()
        loaded = []
        for name, path in jobio.layer_paths(job).items():
            layer = QgsVectorLayer(path, f"scau_{name}", "ogr")
            if layer.isValid():
                QgsProject.instance().addMapLayer(layer)
                loaded.append(name)
        self._show_rows([("info", "LayersLoaded", ", ".join(loaded) or "无可加载图层")])

"""SCAU PreProc Workbench dialog (M287-E1 job page + M287-E3 mesh workbench).

Stateless shell over jobio: render user choices -> job_config.json -> run the
pipeline subprocess -> display validation findings -> load map layers. The
dialog holds no computation state; every run re-reads reports from disk.

E3 (mesh workbench tab): the operator draws breaklines / refinement regions
in an ordinary QGIS scratch layer (or points at an existing GeoJSON); the
shell only serialises that layer to the mesh_controls GeoJSON contract and
passes its path through job_config. After a run, mesh_quality_cells.geojson
is rendered as a graduated heat map; certification stays with the C++ layer.
"""

from __future__ import annotations

import json
from pathlib import Path

from qgis.core import (
    QgsCoordinateReferenceSystem,
    QgsFillSymbol,
    QgsGraduatedSymbolRenderer,
    QgsProject,
    QgsRendererRange,
    QgsSettings,
    QgsVectorLayer,
)
from qgis.PyQt.QtCore import Qt
from qgis.PyQt.QtWidgets import (
    QCheckBox,
    QComboBox,
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
    QTabWidget,
    QVBoxLayout,
    QWidget,
)

from . import jobio

# Fixed good -> bad ramp (5 classes) shared by every heat-map metric.
HEATMAP_COLOURS = ("#1a9850", "#91cf60", "#fee08b", "#fc8d59", "#d73027")


class WorkbenchDialog(QDialog):
    _SETTINGS_KEY = "scau_preproc_workbench/repo_root"
    _CONTROLS_LAYER_NAME = "scau_mesh_controls"

    def __init__(self, repo_root_hint: str, parent=None) -> None:
        super().__init__(parent)
        self.setWindowTitle("SCAU PreProc Workbench (M287-E1/E3)")
        self.resize(820, 640)

        # The plugin may run from a copied profile deployment, so the repo
        # root is user-configured and persisted; the hint is used only when
        # it actually contains the pipeline package.
        saved = QgsSettings().value(self._SETTINGS_KEY, "", type=str)
        initial_root = ""
        for candidate in (saved, repo_root_hint):
            if candidate and jobio.repo_root_valid(candidate):
                initial_root = candidate
                break

        tabs = QTabWidget()
        tabs.addTab(self._build_job_page(initial_root), "作业")
        tabs.addTab(self._build_mesh_page(), "网格工作台")

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
        layout.addWidget(tabs)
        layout.addLayout(buttons)
        layout.addWidget(self._findings)

    # --- page builders ------------------------------------------------------

    def _build_job_page(self, initial_root: str) -> QWidget:
        page = QWidget()
        grid = QGridLayout()
        self._repo_edit = QLineEdit(initial_root)
        self._package_edit = QLineEdit(
            str(Path(initial_root) / "samples/d5_gis_preproc_template") if initial_root else "")
        self._output_edit = QLineEdit()
        self._validator_edit = QLineEdit(
            str(Path(initial_root) / "build/windows-msvc/apps/preproc_cli/Debug/scau_preproc.exe")
            if initial_root else "")
        for row, (label, edit, is_dir) in enumerate((
            ("SCAU-UFM 仓库根目录", self._repo_edit, True),
            ("输入包目录", self._package_edit, True),
            ("输出目录", self._output_edit, True),
            ("validator CLI（可选）", self._validator_edit, False),
        )):
            grid.addWidget(QLabel(label), row, 0)
            grid.addWidget(edit, row, 1)
            browse = QPushButton("...")
            browse.clicked.connect(lambda _=False, e=edit, d=is_dir: self._browse(e, d))
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
        self._confirmations_edit = QLineEdit()
        self._confirmations_edit.setPlaceholderText("确认目录（可选；默认使用输入包 coupling/confirmed/）")
        options = QHBoxLayout()
        options.addWidget(QLabel("特征长度"))
        options.addWidget(self._lc_spin)
        options.addWidget(self._recombine_check)
        options.addWidget(self._determinism_check)
        options.addWidget(self._coupling_check)
        options.addStretch()
        confirmations = QHBoxLayout()
        confirmations.addWidget(QLabel("耦合确认目录"))
        confirmations.addWidget(self._confirmations_edit)
        browse_conf = QPushButton("...")
        browse_conf.clicked.connect(lambda: self._browse(self._confirmations_edit, True))
        confirmations.addWidget(browse_conf)

        layout = QVBoxLayout(page)
        layout.addLayout(grid)
        layout.addLayout(options)
        layout.addLayout(confirmations)
        layout.addStretch()
        return page

    def _build_mesh_page(self) -> QWidget:
        page = QWidget()
        grid = QGridLayout()
        self._controls_enable = QCheckBox("启用网格控制（加密线 / 加密区）")
        self._controls_edit = QLineEdit()
        browse = QPushButton("...")
        browse.clicked.connect(lambda: self._browse_file(self._controls_edit))
        grid.addWidget(self._controls_enable, 0, 0, 1, 3)
        grid.addWidget(QLabel("mesh_controls.geojson"), 1, 0)
        grid.addWidget(self._controls_edit, 1, 1)
        grid.addWidget(browse, 1, 2)

        self._default_size_spin = QDoubleSpinBox()
        self._default_size_spin.setRange(0.0, 1000.0)
        self._default_size_spin.setSpecialValueText("（无）")
        self._default_size_spin.setSuffix(" m")
        self._default_dist_spin = QDoubleSpinBox()
        self._default_dist_spin.setRange(0.0, 10000.0)
        self._default_dist_spin.setSpecialValueText("（无）")
        self._default_dist_spin.setSuffix(" m")
        grid.addWidget(QLabel("默认 size_m（要素未填时）"), 2, 0)
        grid.addWidget(self._default_size_spin, 2, 1)
        grid.addWidget(QLabel("默认 dist_max_m（加密线过渡距离）"), 3, 0)
        grid.addWidget(self._default_dist_spin, 3, 1)

        new_layer = QPushButton("新建控制要素草稿层")
        new_layer.clicked.connect(self._create_controls_layer)
        save_layer = QPushButton("保存草稿层 → GeoJSON")
        save_layer.clicked.connect(self._save_controls_layer)
        precheck = QPushButton("预检控制文件")
        precheck.clicked.connect(self._precheck_controls)
        actions = QHBoxLayout()
        actions.addWidget(new_layer)
        actions.addWidget(save_layer)
        actions.addWidget(precheck)
        actions.addStretch()

        self._metric_combo = QComboBox()
        for metric, (label, _, _) in jobio.HEATMAP_METRICS.items():
            self._metric_combo.addItem(label, metric)
        heatmap = QPushButton("加载质量热力图")
        heatmap.clicked.connect(self._load_heatmap)
        summary = QPushButton("显示网格质量摘要")
        summary.clicked.connect(lambda: self._show_rows(jobio.mesh_quality_summary(self._current_job())))
        heat = QHBoxLayout()
        heat.addWidget(QLabel("热力图指标"))
        heat.addWidget(self._metric_combo)
        heat.addWidget(heatmap)
        heat.addWidget(summary)
        heat.addStretch()

        hint = QLabel(
            "约定：breakline = LineString（可选 size_m + dist_max_m）；refinement_region = Polygon"
            "（必填 size_m ≤ 特征长度）。所有几何规则由生成器 fail-closed 判定，肇事要素写入 "
            "generator.diagnostic.geojson。热力图仅供复查，认证以 validate CLI 为准。")
        hint.setWordWrap(True)

        layout = QVBoxLayout(page)
        layout.addLayout(grid)
        layout.addLayout(actions)
        layout.addLayout(heat)
        layout.addWidget(hint)
        layout.addStretch()
        return page

    # --- helpers ------------------------------------------------------------

    def _browse(self, edit: QLineEdit, is_dir: bool = True) -> None:
        if is_dir:
            chosen = QFileDialog.getExistingDirectory(self, "选择目录", edit.text())
        else:
            chosen, _ = QFileDialog.getOpenFileName(self, "选择文件", edit.text())
        if chosen:
            edit.setText(chosen)

    def _browse_file(self, edit: QLineEdit) -> None:
        chosen, _ = QFileDialog.getSaveFileName(
            self, "mesh_controls.geojson", edit.text() or "mesh_controls.geojson",
            "GeoJSON (*.geojson *.json)", options=QFileDialog.Option.DontConfirmOverwrite)
        if chosen:
            edit.setText(chosen)

    def _controls_path(self) -> str | None:
        if not self._controls_enable.isChecked():
            return None
        return self._controls_edit.text().strip() or None

    def _current_job(self) -> dict:
        validator = self._validator_edit.text().strip()
        default_size = self._default_size_spin.value() or None
        default_dist = self._default_dist_spin.value() or None
        return jobio.build_job_config(
            self._package_edit.text().strip(),
            self._output_edit.text().strip(),
            characteristic_length_m=self._lc_spin.value(),
            recombine=self._recombine_check.isChecked(),
            determinism_check=self._determinism_check.isChecked(),
            coupling_maps=self._coupling_check.isChecked(),
            validator_cli=validator if validator and Path(validator).is_file() else None,
            mesh_controls_geojson=self._controls_path(),
            mesh_controls_default_size_m=default_size,
            mesh_controls_default_dist_max_m=default_dist,
            confirmations_dir=self._confirmations_edit.text().strip() or None,
        )

    def _show_rows(self, rows) -> None:
        self._findings.setRowCount(len(rows))
        for r, row in enumerate(rows):
            for c, value in enumerate(row):
                item = QTableWidgetItem(str(value))
                item.setFlags(item.flags() & ~Qt.ItemFlag.ItemIsEditable)
                self._findings.setItem(r, c, item)

    # --- job page actions ---------------------------------------------------

    def _run(self) -> None:
        if not self._output_edit.text().strip():
            self._show_rows([("fatal", "MissingOutputDir", "请选择输出目录")])
            return
        repo_root = self._repo_edit.text().strip()
        if not jobio.repo_root_valid(repo_root):
            self._show_rows([("fatal", "RepoRootInvalid",
                              "仓库根目录必须包含 python/scau_preproc/pipeline.py：" + repo_root)])
            return
        if self._controls_enable.isChecked() and not self._controls_path():
            self._show_rows([("fatal", "MeshControlsMissing",
                              "已启用网格控制但未指定 mesh_controls.geojson")])
            return
        precheck = [row for row in jobio.mesh_controls_precheck(self._controls_path(), self._lc_spin.value())
                    if row[0] == "fatal"]
        if precheck:
            self._show_rows(precheck)
            return
        QgsSettings().setValue(self._SETTINGS_KEY, repo_root)
        job = self._current_job()
        job_path = jobio.write_job_config(job, job["output_dir"])
        self._run_button.setEnabled(False)
        self._run_button.setText("运行中…")
        try:
            result = jobio.run_pipeline(job_path, repo_root)
        finally:
            self._run_button.setEnabled(True)
            self._run_button.setText("运行流水线")
        rows = jobio.findings_rows(result.get("validation"))
        if result.get("stderr") and (
            result["status"] not in ("ok", "fatal") or result.get("validation") is None
        ):
            rows.insert(0, ("fatal", result["status"], result["stderr"][-2000:]))
        if result["status"] == "ok":
            rows.extend(jobio.mesh_quality_summary(job))
        self._show_rows(rows)
        can_export = jobio.exportable(result.get("validation"))
        self._export_label.setText(
            "导出门禁：允许 (status=ok)" if can_export
            else "导出门禁：禁止（存在 fatal / review / 未确认耦合候选）")

    def _load_layers(self) -> None:
        job = self._current_job()
        loaded = []
        for name, path in jobio.layer_paths(job).items():
            if name in ("mesh_quality_cells", "effective_links"):
                continue  # heat map has its own action; effective_links is tabular (E4 page)
            layer = QgsVectorLayer(path, f"scau_{name}", "ogr")
            if layer.isValid():
                QgsProject.instance().addMapLayer(layer)
                loaded.append(name)
        self._show_rows([("info", "LayersLoaded", ", ".join(loaded) or "无可加载图层")])

    # --- mesh workbench actions ---------------------------------------------

    def _project_crs(self) -> QgsCoordinateReferenceSystem:
        crs = QgsProject.instance().crs()
        # The package frame is projected metres; an unset project CRS falls
        # back to a plain metre engineering CRS so drawn coordinates are stored
        # verbatim (no reprojection happens in the UI layer).
        return crs if crs.isValid() else QgsCoordinateReferenceSystem("EPSG:3857")

    def _create_controls_layer(self) -> None:
        fields = "&".join(f"field={name}:{kind}" for name, kind in jobio.MESH_CONTROL_FIELDS)
        layers = []
        for geometry, kind in (("LineString", "breakline"), ("Polygon", "refinement_region")):
            uri = f"{geometry}?crs={self._project_crs().authid()}&{fields}"
            layer = QgsVectorLayer(uri, f"{self._CONTROLS_LAYER_NAME}_{kind}", "memory")
            layer.setCustomProperty("scau_control_kind", kind)
            QgsProject.instance().addMapLayer(layer)
            layers.append(layer.name())
        self._controls_enable.setChecked(True)
        self._show_rows([("info", "ControlsLayersCreated",
                          "; ".join(layers) + " — 在图层中绘制并填写 control_id/size_m/dist_max_m，"
                          "然后“保存草稿层 → GeoJSON”")])

    def _controls_layers(self) -> list[QgsVectorLayer]:
        return [
            layer for layer in QgsProject.instance().mapLayers().values()
            if isinstance(layer, QgsVectorLayer)
            and layer.customProperty("scau_control_kind") in jobio.MESH_CONTROL_KINDS
        ]

    def _save_controls_layer(self) -> None:
        target = self._controls_edit.text().strip()
        if not target:
            self._show_rows([("fatal", "MeshControlsPathMissing", "请先指定 mesh_controls.geojson 路径")])
            return
        layers = self._controls_layers()
        if not layers:
            self._show_rows([("fatal", "NoControlsLayer", "没有草稿层；先“新建控制要素草稿层”")])
            return
        collection = jobio.mesh_controls_template()
        for layer in layers:
            kind = layer.customProperty("scau_control_kind")
            for index, feature in enumerate(layer.getFeatures()):
                geometry = feature.geometry()
                if geometry.isEmpty():
                    continue
                props = {"control_kind": kind}
                attrs = {f.name(): feature[f.name()] for f in layer.fields()}
                props["control_id"] = str(attrs.get("control_id") or f"{kind}_{index}")
                for key in ("size_m", "dist_max_m"):
                    value = attrs.get(key)
                    if value is not None and str(value) not in ("", "NULL"):
                        props[key] = float(value)
                collection["features"].append({
                    "type": "Feature",
                    "properties": props,
                    "geometry": json.loads(geometry.asJson(6)),
                })
        Path(target).parent.mkdir(parents=True, exist_ok=True)
        Path(target).write_text(json.dumps(collection, indent=2), encoding="utf-8")
        self._controls_enable.setChecked(True)
        rows = [("info", "MeshControlsSaved", f"{len(collection['features'])} 个要素 → {target}")]
        rows.extend(jobio.mesh_controls_precheck(target, self._lc_spin.value()))
        self._show_rows(rows)

    def _precheck_controls(self) -> None:
        path = self._controls_edit.text().strip()
        rows = jobio.mesh_controls_precheck(path, self._lc_spin.value()) if path else [
            ("fatal", "MeshControlsPathMissing", "请先指定 mesh_controls.geojson 路径")]
        self._show_rows(rows)

    def _load_heatmap(self) -> None:
        job = self._current_job()
        path = jobio.layer_paths(job).get("mesh_quality_cells")
        if not path:
            self._show_rows([("info", "NoHeatmap", "未找到 mesh_quality_cells.geojson；先运行流水线")])
            return
        metric = self._metric_combo.currentData()
        layer = QgsVectorLayer(path, f"scau_mesh_quality[{metric}]", "ogr")
        if not layer.isValid():
            self._show_rows([("fatal", "HeatmapInvalid", path)])
            return
        ranges = []
        for colour, (lower, upper, label) in zip(HEATMAP_COLOURS, jobio.heatmap_classes(metric)):
            symbol = QgsFillSymbol.createSimple({"color": colour, "outline_color": "#40000000",
                                                 "outline_width": "0.1"})
            lo = -1.0e300 if lower == float("-inf") else lower
            hi = 1.0e300 if upper == float("inf") else upper
            ranges.append(QgsRendererRange(lo, hi, symbol, label))
        renderer = QgsGraduatedSymbolRenderer(metric, ranges)
        layer.setRenderer(renderer)
        QgsProject.instance().addMapLayer(layer)
        self._show_rows([("info", "HeatmapLoaded", f"{metric} ← {path}")]
                        + jobio.mesh_quality_summary(job))

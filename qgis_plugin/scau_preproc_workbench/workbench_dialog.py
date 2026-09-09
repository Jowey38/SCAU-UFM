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
    QgsCategorizedSymbolRenderer,
    QgsCoordinateReferenceSystem,
    QgsFillSymbol,
    QgsGraduatedSymbolRenderer,
    QgsLineSymbol,
    QgsMarkerSymbol,
    QgsProject,
    QgsRasterLayer,
    QgsRendererCategory,
    QgsRendererRange,
    QgsSettings,
    QgsVectorLayer,
)
from qgis.PyQt.QtCore import Qt
from qgis.PyQt.QtGui import QBrush, QColor
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
    QSpinBox,
    QTableWidget,
    QTableWidgetItem,
    QTabWidget,
    QTreeWidget,
    QTreeWidgetItem,
    QVBoxLayout,
    QWidget,
)

from . import jobio

# Fixed good -> bad ramp (5 classes) shared by every heat-map metric.
HEATMAP_COLOURS = ("#1a9850", "#91cf60", "#fee08b", "#fc8d59", "#d73027")
# Coupling-editor status colours (E4): review/unconfirmed red, confirmed green,
# retargeted blue, rejected grey, drifted orange.
STATUS_COLOURS = {"unconfirmed": "#d73027", "accepted": "#1a9850", "retargeted": "#1f78b4",
                  "rejected": "#9e9e9e", "drifted": "#ff7f00"}
EDITOR_COLUMNS = ("chain", "mapping_id", "swmm_node_id", "building_id", "candidate_cell_index",
                  "cell_index", "exchange_elevation_m", "confidence", "status", "decision_file")


class WorkbenchDialog(QDialog):
    _SETTINGS_KEY = "scau_preproc_workbench/repo_root"
    _CONTROLS_LAYER_NAME = "scau_mesh_controls"

    def __init__(self, repo_root_hint: str, parent=None, iface=None) -> None:
        super().__init__(parent)
        self._iface = iface
        self.setWindowTitle("SCAU PreProc Workbench (M287-E1..E5a / P3)")
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
        tabs.addTab(self._build_data_tree_page(), "数据目录")
        tabs.addTab(self._build_field_mapping_page(), "字段映射")
        tabs.addTab(self._build_mesh_page(), "网格工作台")
        tabs.addTab(self._build_parameter_page(), "参数表")
        tabs.addTab(self._build_coupling_page(), "耦合编辑器")
        tabs.addTab(self._build_report_page(), "门禁与报告")

        self._run_button = QPushButton("运行流水线")
        self._run_button.clicked.connect(self._run)
        self._load_button = QPushButton("加载图层")
        self._load_button.clicked.connect(self._load_layers)
        self._export_label = QLabel("导出门禁：未运行")
        self._export_button = QPushButton("导出案例包")
        self._export_button.setEnabled(False)
        self._export_button.clicked.connect(self._export_case)
        self._export_target_edit = QLineEdit()
        self._export_target_edit.setPlaceholderText("案例包目标目录（B6）")
        buttons = QHBoxLayout()
        buttons.addWidget(self._run_button)
        buttons.addWidget(self._load_button)
        buttons.addWidget(self._export_label)
        buttons.addWidget(self._export_target_edit)
        buttons.addWidget(self._export_button)
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
        self._coupling_mode = QComboBox()
        for mode, label in jobio.COUPLING_MODES:
            self._coupling_mode.addItem(label, mode)
        self._roof_distance_spin = QDoubleSpinBox()
        self._roof_distance_spin.setRange(0.1, 10000.0)
        self._roof_distance_spin.setValue(jobio.DEFAULT_ROOF_NODE_MAX_DISTANCE_M)
        self._roof_distance_spin.setSuffix(" m")
        self._confirmations_edit = QLineEdit()
        self._confirmations_edit.setPlaceholderText("确认目录（可选；默认使用输入包 coupling/confirmed/）")
        self._crs_policy_edit = QLineEdit()
        self._crs_policy_edit.setPlaceholderText("CRS 策略 metadata/crs_policy.json（可选；留空 = 输入包自带策略或 v1 未检查模式）")
        self._rule_table_edit = QLineEdit()
        self._rule_table_edit.setPlaceholderText("DPM 规则表 metadata/dpm_rule_table.json（可选；留空 = v1 占位场）")
        self._terrain_policy_edit = QLineEdit()
        self._terrain_policy_edit.setPlaceholderText(
            "DEM 调理策略 metadata/terrain_condition_policy.json（可选；留空 = 输入包自带策略或 DEM 原样采样）")
        options = QHBoxLayout()
        options.addWidget(QLabel("特征长度"))
        options.addWidget(self._lc_spin)
        options.addWidget(self._recombine_check)
        options.addWidget(self._determinism_check)
        options.addWidget(self._coupling_check)
        options.addStretch()
        coupling = QHBoxLayout()
        coupling.addWidget(QLabel("耦合映射模式"))
        coupling.addWidget(self._coupling_mode)
        coupling.addWidget(QLabel("屋面→检查井最大距离"))
        coupling.addWidget(self._roof_distance_spin)
        coupling.addStretch()
        crs_row = QHBoxLayout()
        crs_row.addWidget(QLabel("CRS 策略"))
        crs_row.addWidget(self._crs_policy_edit)
        browse_crs = QPushButton("...")
        browse_crs.clicked.connect(lambda: self._browse(self._crs_policy_edit, False))
        crs_row.addWidget(browse_crs)
        terrain_row = QHBoxLayout()
        terrain_row.addWidget(QLabel("DEM 调理策略"))
        terrain_row.addWidget(self._terrain_policy_edit)
        browse_terrain = QPushButton("...")
        browse_terrain.clicked.connect(lambda: self._browse(self._terrain_policy_edit, False))
        terrain_row.addWidget(browse_terrain)
        fields = QHBoxLayout()
        fields.addWidget(QLabel("字段派生规则表"))
        fields.addWidget(self._rule_table_edit)
        browse_rules = QPushButton("...")
        browse_rules.clicked.connect(lambda: self._browse(self._rule_table_edit, False))
        fields.addWidget(browse_rules)
        confirmations = QHBoxLayout()
        confirmations.addWidget(QLabel("耦合确认目录"))
        confirmations.addWidget(self._confirmations_edit)
        browse_conf = QPushButton("...")
        browse_conf.clicked.connect(lambda: self._browse(self._confirmations_edit, True))
        confirmations.addWidget(browse_conf)

        layout = QVBoxLayout(page)
        layout.addLayout(grid)
        layout.addLayout(options)
        layout.addLayout(coupling)
        layout.addLayout(crs_row)
        layout.addLayout(terrain_row)
        layout.addLayout(fields)
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

    _OPERATOR_KEY = "scau_preproc_workbench/operator"
    _EDITOR_LAYER_PREFIX = "scau_coupling_"

    def _build_coupling_page(self) -> QWidget:
        page = QWidget()
        top = QGridLayout()
        self._operator_edit = QLineEdit(QgsSettings().value(self._OPERATOR_KEY, "", type=str))
        self._operator_edit.setPlaceholderText("确认者标识（写入 confirmed_by）")
        top.addWidget(QLabel("确认者"), 0, 0)
        top.addWidget(self._operator_edit, 0, 1)
        refresh = QPushButton("刷新候选 / 加载连线图层")
        refresh.clicked.connect(self._load_coupling_layers)
        top.addWidget(refresh, 0, 2)

        self._coupling_table = QTableWidget(0, len(EDITOR_COLUMNS))
        self._coupling_table.setHorizontalHeaderLabels(list(EDITOR_COLUMNS))
        self._coupling_table.horizontalHeader().setStretchLastSection(True)
        self._coupling_table.setEditTriggers(QTableWidget.EditTrigger.NoEditTriggers)
        self._coupling_table.setSelectionBehavior(QTableWidget.SelectionBehavior.SelectRows)
        self._coupling_table.itemSelectionChanged.connect(self._highlight_selected_candidate)

        self._target_cell_spin = QSpinBox()
        self._target_cell_spin.setRange(0, 10_000_000)
        self._target_crest_spin = QDoubleSpinBox()
        self._target_crest_spin.setDecimals(3)
        self._target_crest_spin.setRange(-1000.0, 10000.0)
        self._target_crest_spin.setSpecialValueText("（保留候选值）")
        self._target_crest_spin.setValue(-1000.0)
        pick = QPushButton("取地图选中单元")
        pick.clicked.connect(self._pick_selected_cell)
        self._note_edit = QLineEdit()
        self._note_edit.setPlaceholderText("备注（可选）")
        target = QHBoxLayout()
        target.addWidget(QLabel("目标单元"))
        target.addWidget(self._target_cell_spin)
        target.addWidget(pick)
        target.addWidget(QLabel("交换高程"))
        target.addWidget(self._target_crest_spin)
        target.addWidget(self._note_edit)

        actions = QHBoxLayout()
        for label, decision in (("确认 (accept)", "accept"), ("拒绝 (reject)", "reject"),
                                ("改目标 (retarget)", "retarget")):
            button = QPushButton(label)
            button.clicked.connect(lambda _=False, d=decision: self._decide(d))
            actions.addWidget(button)
        undo = QPushButton("撤销决策")
        undo.clicked.connect(self._undo_decision)
        actions.addWidget(undo)
        actions.addStretch()

        create = QHBoxLayout()
        self._create_chain = QComboBox()
        for chain in jobio.COUPLING_CHAINS:
            self._create_chain.addItem(chain, chain)
        self._create_id = QLineEdit()
        self._create_id.setPlaceholderText("新 mapping_id")
        self._create_node = QLineEdit()
        self._create_node.setPlaceholderText("swmm_node_id")
        self._create_building = QLineEdit()
        self._create_building.setPlaceholderText("building_id（roof 链）")
        create_button = QPushButton("新建链接 (create) ← 目标单元/高程")
        create_button.clicked.connect(self._create_link)
        for widget in (QLabel("新建"), self._create_chain, self._create_id, self._create_node,
                       self._create_building, create_button):
            create.addWidget(widget)

        hint = QLabel(
            "所有决策写入 C4 确认文件（默认输入包 coupling/confirmed/，或作业页指定目录），"
            "生成器候选文件永不修改。红=待确认/review，绿=已确认，蓝=已改目标，灰=已拒绝，橙=候选已漂移。"
            "写入后重新“运行流水线”即可看到有效链接与导出门禁变化。")
        hint.setWordWrap(True)

        layout = QVBoxLayout(page)
        layout.addLayout(top)
        layout.addWidget(self._coupling_table)
        layout.addLayout(target)
        layout.addLayout(actions)
        layout.addLayout(create)
        layout.addWidget(hint)
        return page

    # --- coupling editor actions --------------------------------------------

    def _write_editor_geojson(self, name: str, collection: dict) -> str:
        directory = Path(self._current_job()["output_dir"]) / "coupling" / "editor"
        directory.mkdir(parents=True, exist_ok=True)
        path = directory / f"{name}.geojson"
        path.write_text(json.dumps(collection), encoding="utf-8")
        return str(path)

    def _status_renderer(self, geometry: str) -> QgsCategorizedSymbolRenderer:
        categories = []
        for status, colour in STATUS_COLOURS.items():
            if geometry == "line":
                symbol = QgsLineSymbol.createSimple({"color": colour, "width": "0.8"})
            elif geometry == "point":
                symbol = QgsMarkerSymbol.createSimple({"color": colour, "size": "3",
                                                       "outline_color": "#000000"})
            else:
                symbol = QgsFillSymbol.createSimple({"color": colour + "66", "outline_color": colour,
                                                     "outline_width": "0.6"})
            categories.append(QgsRendererCategory(status, symbol, status))
        return QgsCategorizedSymbolRenderer("status", categories)

    def _load_coupling_layers(self) -> None:
        job = self._current_job()
        if not job["output_dir"]:
            self._show_rows([("fatal", "MissingOutputDir", "请先在作业页选择输出目录")])
            return
        layers = jobio.build_coupling_link_layers(job)
        project = QgsProject.instance()
        for layer in list(project.mapLayers().values()):
            if layer.name().startswith(self._EDITOR_LAYER_PREFIX):
                project.removeMapLayer(layer.id())
        for name, geometry in (("cells", "fill"), ("links", "line"), ("nodes", "point")):
            path = self._write_editor_geojson(name, layers[name])
            layer = QgsVectorLayer(path, f"{self._EDITOR_LAYER_PREFIX}{name}", "ogr")
            if layer.isValid():
                layer.setRenderer(self._status_renderer(geometry))
                project.addMapLayer(layer)
        self._fill_coupling_table(layers)
        self._show_rows(jobio.coupling_editor_rows(job))

    def _fill_coupling_table(self, layers: dict) -> None:
        rows = [f["properties"] for f in layers["cells"]["features"]]
        seen = {(r["chain"], r["mapping_id"]) for r in rows}
        for f in layers["nodes"]["features"]:
            key = (f["properties"]["chain"], f["properties"]["mapping_id"])
            if key not in seen:
                rows.append(f["properties"])
                seen.add(key)
        rows.sort(key=lambda r: (r["chain"], r["mapping_id"]))
        self._coupling_table.setRowCount(len(rows))
        for r, props in enumerate(rows):
            for c, key in enumerate(EDITOR_COLUMNS):
                item = QTableWidgetItem("" if props.get(key) is None else str(props[key]))
                item.setFlags(item.flags() & ~Qt.ItemFlag.ItemIsEditable)
                self._coupling_table.setItem(r, c, item)

    def _selected_candidate(self) -> tuple[str, str] | None:
        row = self._coupling_table.currentRow()
        if row < 0:
            return None
        return (self._coupling_table.item(row, 0).text(), self._coupling_table.item(row, 1).text())

    def _highlight_selected_candidate(self) -> None:
        selected = self._selected_candidate()
        if not selected:
            return
        chain, mapping_id = selected
        expression = f'"chain" = \'{chain}\' AND "mapping_id" = \'{mapping_id}\''
        for layer in QgsProject.instance().mapLayers().values():
            if layer.name().startswith(self._EDITOR_LAYER_PREFIX) and isinstance(layer, QgsVectorLayer):
                layer.selectByExpression(expression)

    def _pick_selected_cell(self) -> None:
        """cell_id of the first selected feature of a quality-cells (heat map)
        layer: the operator clicks the target cell on the map."""
        for layer in QgsProject.instance().mapLayers().values():
            if isinstance(layer, QgsVectorLayer) and layer.name().startswith("scau_mesh_quality"):
                for feature in layer.selectedFeatures():
                    self._target_cell_spin.setValue(int(feature["cell_id"]))
                    self._show_rows([("info", "TargetCellPicked",
                                      f"cell {feature['cell_id']} from {layer.name()}")])
                    return
        self._show_rows([("info", "NoCellSelected", "先加载质量热力图并在地图上选中一个单元")])

    def _operator(self) -> str:
        operator = self._operator_edit.text().strip()
        if operator:
            QgsSettings().setValue(self._OPERATOR_KEY, operator)
        return operator

    def _crest_override(self) -> float | None:
        value = self._target_crest_spin.value()
        return None if value <= self._target_crest_spin.minimum() else value

    def _decide(self, decision: str) -> None:
        selected = self._selected_candidate()
        if not selected:
            self._show_rows([("fatal", "NoCandidateSelected", "先在表中选择一个候选")])
            return
        chain, mapping_id = selected
        target = None
        if decision == "retarget":
            target = {"cell_index": self._target_cell_spin.value()}
            if self._crest_override() is not None:
                target["exchange_elevation_m"] = self._crest_override()
        try:
            path = jobio.write_decision(self._current_job(), chain, mapping_id, decision,
                                        confirmed_by=self._operator(), target=target,
                                        note=self._note_edit.text().strip())
        except ValueError as error:
            self._show_rows([("fatal", "DecisionRejected", str(error))])
            return
        self._load_coupling_layers()
        self._show_rows([("info", "DecisionWritten", f"{decision} {chain}/{mapping_id} -> {path}")]
                        + jobio.coupling_editor_rows(self._current_job()))

    def _undo_decision(self) -> None:
        selected = self._selected_candidate()
        if not selected:
            self._show_rows([("fatal", "NoCandidateSelected", "先在表中选择一个候选")])
            return
        removed = jobio.remove_decision(self._current_job(), selected[1])
        self._load_coupling_layers()
        self._show_rows([("info", "DecisionRemoved" if removed else "NoDecisionFile", selected[1])]
                        + jobio.coupling_editor_rows(self._current_job()))

    def _create_link(self) -> None:
        crest = self._crest_override()
        if crest is None:
            self._show_rows([("fatal", "CreateRejected", "新建链接必须给出交换高程")])
            return
        try:
            path = jobio.write_create_decision(
                self._current_job(), self._create_chain.currentData(), self._create_id.text(),
                confirmed_by=self._operator(), swmm_node_id=self._create_node.text(),
                cell_index=self._target_cell_spin.value(), exchange_elevation_m=crest,
                building_id=self._create_building.text().strip() or None,
                note=self._note_edit.text().strip())
        except ValueError as error:
            self._show_rows([("fatal", "CreateRejected", str(error))])
            return
        self._show_rows([("info", "LinkCreated", f"create -> {path}（重新运行流水线后生效）")])

    # --- P5 parameter tables (E5a) ------------------------------------------

    def _build_parameter_page(self) -> QWidget:
        page = QWidget()
        bar = QHBoxLayout()
        refresh = QPushButton("加载 / 刷新参数表")
        refresh.clicked.connect(self._refresh_parameter_tables)
        lint = QPushButton("预检（闭合律 lint）")
        lint.clicked.connect(self._lint_parameter_tables)
        self._param_editor_edit = QLineEdit(QgsSettings().value(self._OPERATOR_KEY, "", type=str))
        self._param_editor_edit.setPlaceholderText("编辑者标识（写入 revision.edited_by）")
        self._param_note_edit = QLineEdit()
        self._param_note_edit.setPlaceholderText("修订备注（可选）")
        save_rules = QPushButton("保存规则表新版本")
        save_rules.clicked.connect(self._save_rule_table_version)
        save_soil = QPushButton("保存土壤表新版本")
        save_soil.clicked.connect(self._save_soil_table_version)
        for widget in (refresh, lint, QLabel("编辑者"), self._param_editor_edit, self._param_note_edit,
                       save_rules, save_soil):
            bar.addWidget(widget)
        bar.addStretch()

        self._rule_source_label = QLabel("规则表：未加载")
        self._class_table = QTableWidget(0, len(jobio.RULE_CLASS_COLUMNS))
        self._class_table.setHorizontalHeaderLabels(list(jobio.RULE_CLASS_COLUMNS))
        self._class_table.horizontalHeader().setStretchLastSection(True)
        self._interface_table = QTableWidget(0, len(jobio.RULE_INTERFACE_COLUMNS))
        self._interface_table.setHorizontalHeaderLabels(list(jobio.RULE_INTERFACE_COLUMNS))
        self._interface_table.horizontalHeader().setStretchLastSection(True)
        rows_bar = QHBoxLayout()
        for label, table in (("+ 类别行", self._class_table), ("+ 界面行", self._interface_table)):
            button = QPushButton(label)
            button.clicked.connect(lambda _=False, t=table: t.insertRow(t.rowCount()))
            rows_bar.addWidget(button)
        for label, table in (("- 选中类别行", self._class_table), ("- 选中界面行", self._interface_table)):
            button = QPushButton(label)
            button.clicked.connect(lambda _=False, t=table: t.removeRow(t.currentRow()) if t.currentRow() >= 0 else None)
            rows_bar.addWidget(button)
        rows_bar.addStretch()

        self._soil_source_label = QLabel("土壤表：未加载")
        self._soil_table = QTableWidget(0, len(jobio.SOIL_COLUMNS))
        self._soil_table.setHorizontalHeaderLabels(list(jobio.SOIL_COLUMNS))
        self._soil_table.horizontalHeader().setStretchLastSection(True)

        hint = QLabel(
            "浏览 metadata/dpm_rule_table.json（类别 phi_t / Phi_c、界面 omega_edge）与 soil/soil_parameters.csv。"
            "编辑只写出新版本文件（<名>.vNNN.json / .csv，含 revision 块），原文件永不改写；"
            "编辑后的规则表 approval 自动回落为 synthetic_unapproved（M281：参数值须数据所有方重新批准）。"
            "此处 lint 仅为预检，认证以流水线为准。要让流水线使用新版本，请在作业页把“字段派生规则表”指向它。")
        hint.setWordWrap(True)

        layout = QVBoxLayout(page)
        layout.addLayout(bar)
        layout.addWidget(self._rule_source_label)
        layout.addWidget(QLabel("类别（phi_t、Phi_c.xx/xy/yy）"))
        layout.addWidget(self._class_table)
        layout.addWidget(QLabel("界面（类别对 → omega_edge）"))
        layout.addWidget(self._interface_table)
        layout.addLayout(rows_bar)
        layout.addWidget(self._soil_source_label)
        layout.addWidget(self._soil_table)
        layout.addWidget(hint)
        return page

    @staticmethod
    def _fill_table(table: QTableWidget, columns, rows: list[dict]) -> None:
        table.setRowCount(len(rows))
        for r, row in enumerate(rows):
            for c, key in enumerate(columns):
                value = row.get(key)
                table.setItem(r, c, QTableWidgetItem("" if value is None else str(value)))

    @staticmethod
    def _table_rows(table: QTableWidget, columns) -> list[dict]:
        rows = []
        for r in range(table.rowCount()):
            row = {}
            for c, key in enumerate(columns):
                item = table.item(r, c)
                row[key] = item.text().strip() if item is not None else ""
            if any(row.values()):
                rows.append(row)
        return rows

    def _refresh_parameter_tables(self) -> None:
        job = self._current_job()
        if not job["package"]:
            self._show_rows([("fatal", "MissingPackage", "请先在作业页选择输入包目录")])
            return
        rule_path = jobio.rule_table_path(job)
        if rule_path and rule_path.is_file():
            table = jobio.load_rule_table(rule_path)
            self._rule_source_label.setText(f"规则表：{rule_path}")
            self._fill_table(self._class_table, jobio.RULE_CLASS_COLUMNS, jobio.rule_class_rows(table))
            self._fill_table(self._interface_table, jobio.RULE_INTERFACE_COLUMNS, jobio.rule_interface_rows(table))
        else:
            self._rule_source_label.setText("规则表：无（v1 占位场）")
            self._class_table.setRowCount(0)
            self._interface_table.setRowCount(0)
        soil_path = jobio.soil_table_path(job)
        if soil_path.is_file():
            self._soil_source_label.setText(f"土壤表：{soil_path}")
            self._fill_table(self._soil_table, jobio.SOIL_COLUMNS, jobio.load_soil_rows(soil_path))
        else:
            self._soil_source_label.setText(f"土壤表：缺失 {soil_path}")
            self._soil_table.setRowCount(0)
        self._show_rows(jobio.parameter_table_rows(job))

    def _edited_rule_table(self) -> dict | None:
        rule_path = jobio.rule_table_path(self._current_job())
        if not rule_path or not rule_path.is_file():
            self._show_rows([("fatal", "NoRuleTable", "没有可编辑的规则表；先在作业页指定 dpm_rule_table.json")])
            return None
        base = jobio.load_rule_table(rule_path)
        try:
            return jobio.rule_table_from_rows(
                base, self._table_rows(self._class_table, jobio.RULE_CLASS_COLUMNS),
                self._table_rows(self._interface_table, jobio.RULE_INTERFACE_COLUMNS))
        except ValueError as error:
            self._show_rows([("fatal", "RuleTableInvalid", str(error))])
            return None

    def _lint_parameter_tables(self) -> None:
        table = self._edited_rule_table()
        rows = jobio.lint_rule_table(table) if table else []
        rows.extend(jobio.lint_soil_rows(self._table_rows(self._soil_table, jobio.SOIL_COLUMNS)))
        self._show_rows(rows)

    def _param_editor(self) -> str:
        editor = self._param_editor_edit.text().strip()
        if editor:
            QgsSettings().setValue(self._OPERATOR_KEY, editor)
        return editor

    def _save_rule_table_version(self) -> None:
        table = self._edited_rule_table()
        if table is None:
            return
        try:
            path = jobio.write_rule_table_version(jobio.rule_table_path(self._current_job()), table,
                                                 edited_by=self._param_editor(), note=self._param_note_edit.text().strip())
        except ValueError as error:
            self._show_rows([("fatal", "RuleTableRejected", str(error))])
            return
        self._show_rows([("info", "RuleTableVersionWritten",
                          f"{path}（approval 已回落为 synthetic_unapproved；在作业页选择该文件后重新运行流水线）")]
                        + jobio.lint_rule_table(table))

    def _save_soil_table_version(self) -> None:
        try:
            path = jobio.write_soil_table_version(jobio.soil_table_path(self._current_job()),
                                                 self._table_rows(self._soil_table, jobio.SOIL_COLUMNS),
                                                 edited_by=self._param_editor())
        except ValueError as error:
            self._show_rows([("fatal", "SoilTableRejected", str(error))])
            return
        self._show_rows([("info", "SoilTableVersionWritten",
                          f"{path}（流水线仍读取 soil/soil_parameters.csv；替换原件属于输入包治理动作，需数据所有方批准）")])

    # --- P3 field mapping --------------------------------------------------

    _MAPPING_COLUMNS = ("dataset", "layer", "source_field", "source_type", "sample", "target")

    def _build_field_mapping_page(self) -> QWidget:
        page = QWidget()
        bar = QHBoxLayout()
        refresh = QPushButton("发现源字段 / 刷新")
        refresh.clicked.connect(self._refresh_field_mapping)
        self._mapping_editor_edit = QLineEdit(QgsSettings().value(self._OPERATOR_KEY, "", type=str))
        self._mapping_editor_edit.setPlaceholderText("编辑者标识")
        self._mapping_note_edit = QLineEdit()
        self._mapping_note_edit.setPlaceholderText("备注（可选）")
        save = QPushButton("保存映射新版本")
        save.clicked.connect(self._save_field_mapping)
        for widget in (refresh, QLabel("编辑者"), self._mapping_editor_edit, self._mapping_note_edit, save):
            bar.addWidget(widget)
        bar.addStretch()
        self._mapping_table = QTableWidget(0, len(self._MAPPING_COLUMNS))
        self._mapping_table.setHorizontalHeaderLabels(list(self._MAPPING_COLUMNS))
        self._mapping_table.horizontalHeader().setStretchLastSection(True)
        self._mapping_table.setEditTriggers(QTableWidget.EditTrigger.NoEditTriggers)
        hint = QLabel(
            "源字段自动从输入包数据集发现（GeoJSON 属性 / CSV 列 / DEM=elevation）；目标下拉只允许 manifest.canonical_targets "
            "中的 canonical 字段或“(unmapped)”，一个 canonical 目标至多一个来源。保存写出 metadata/field_mapping.vNNN.json"
            "（声明性契约，供 M287-D 真实数据 importer 消费；不改变当前合成流水线的计算）。")
        hint.setWordWrap(True)
        layout = QVBoxLayout(page)
        layout.addLayout(bar)
        layout.addWidget(self._mapping_table)
        layout.addWidget(hint)
        return page

    def _refresh_field_mapping(self) -> None:
        job = self._current_job()
        if not job["package"]:
            self._show_rows([("fatal", "MissingPackage", "请先在作业页选择输入包目录")])
            return
        try:
            targets = jobio.canonical_targets(job)
            rows = jobio.field_mapping_rows(job)
        except ValueError as error:
            self._show_rows([("fatal", "CanonicalTargetsInvalid", str(error))])
            return
        self._mapping_table.setRowCount(len(rows))
        for r, row in enumerate(rows):
            for c, key in enumerate(self._MAPPING_COLUMNS[:-1]):
                self._mapping_table.setItem(r, c, QTableWidgetItem(str(row.get(key, ""))))
            combo = QComboBox()
            combo.addItem(jobio.UNMAPPED, jobio.UNMAPPED)
            for target in targets:
                combo.addItem(target, target)
            combo.setCurrentIndex(max(0, combo.findData(row["target"])))
            self._mapping_table.setCellWidget(r, len(self._MAPPING_COLUMNS) - 1, combo)
        self._show_rows(jobio.field_mapping_summary(job))

    def _mapping_rows_from_table(self) -> list[dict]:
        rows = []
        for r in range(self._mapping_table.rowCount()):
            row = {key: self._mapping_table.item(r, c).text() for c, key in enumerate(self._MAPPING_COLUMNS[:-1])}
            combo = self._mapping_table.cellWidget(r, len(self._MAPPING_COLUMNS) - 1)
            row["target"] = combo.currentData() if combo is not None else jobio.UNMAPPED
            rows.append(row)
        return rows

    def _save_field_mapping(self) -> None:
        editor = self._mapping_editor_edit.text().strip()
        if editor:
            QgsSettings().setValue(self._OPERATOR_KEY, editor)
        try:
            path = jobio.write_field_mapping_version(self._current_job(), self._mapping_rows_from_table(),
                                                    edited_by=editor, note=self._mapping_note_edit.text().strip())
        except ValueError as error:
            self._show_rows([("fatal", "FieldMappingRejected", str(error))])
            return
        self._show_rows([("info", "FieldMappingVersionWritten", str(path))] + jobio.field_mapping_summary(self._current_job()))

    # --- E2 data tree / E5 report browser -----------------------------------

    _LAMP_TEXT = {"pass": "● Pass", "review": "● Review", "fatal": "● Fatal", "missing": "○ Missing"}
    _LAMP_COLOUR = {"pass": "#1a9850", "review": "#ff7f00", "fatal": "#d73027", "missing": "#9e9e9e"}

    def _build_data_tree_page(self) -> QWidget:
        page = QWidget()
        self._tree = QTreeWidget()
        self._tree.setColumnCount(4)
        self._tree.setHeaderLabels(["数据集 / 产物", "状态", "说明", "路径"])
        self._tree.itemDoubleClicked.connect(self._tree_item_activated)
        refresh = QPushButton("刷新目录树")
        refresh.clicked.connect(self._refresh_data_tree)
        self._tree_summary = QLabel("未加载")
        bar = QHBoxLayout()
        bar.addWidget(refresh)
        bar.addWidget(self._tree_summary)
        bar.addStretch()
        hint = QLabel("双击 GeoJSON 数据集或诊断产物即加载为图层。状态灯来自输入包清单（CRS/单位/存在性）"
                      "与 validation.json 的对象级 finding；不做任何额外校验。")
        hint.setWordWrap(True)
        layout = QVBoxLayout(page)
        layout.addLayout(bar)
        layout.addWidget(self._tree)
        layout.addWidget(hint)
        return page

    def _refresh_data_tree(self) -> None:
        job = self._current_job()
        if not job["package"]:
            self._show_rows([("fatal", "MissingPackage", "请先在作业页选择输入包目录")])
            return
        tree = jobio.data_tree(job)
        self._tree.clear()
        for group in tree:
            parent = QTreeWidgetItem([group["group"], "", "", ""])
            parent.setExpanded(True)
            worst = "pass"
            for item in group["items"]:
                child = QTreeWidgetItem([item["id"], self._LAMP_TEXT[item["lamp"]], item["detail"], item["path"] or ""])
                child.setForeground(1, QBrush(QColor(self._LAMP_COLOUR[item["lamp"]])))
                child.setData(0, Qt.ItemDataRole.UserRole, item.get("layer"))
                parent.addChild(child)
                order = ("pass", "missing", "review", "fatal")
                if order.index(item["lamp"]) > order.index(worst):
                    worst = item["lamp"]
            parent.setText(1, self._LAMP_TEXT[worst])
            parent.setForeground(1, QBrush(QColor(self._LAMP_COLOUR[worst])))
            self._tree.addTopLevelItem(parent)
        for column in range(4):
            self._tree.resizeColumnToContents(column)
        summary = jobio.data_tree_summary(tree)
        self._tree_summary.setText("  ".join(f"{self._LAMP_TEXT[k]}: {v}" for k, v in summary.items()))

    def _tree_item_activated(self, item: QTreeWidgetItem, _column: int) -> None:
        path = item.data(0, Qt.ItemDataRole.UserRole)
        if not path:
            return
        if str(path).endswith(".asc"):
            layer = QgsRasterLayer(str(path), f"scau_{item.text(0)}")
        else:
            layer = QgsVectorLayer(str(path), f"scau_{item.text(0)}", "ogr")
        if layer.isValid():
            QgsProject.instance().addMapLayer(layer)
            self._show_rows([("info", "LayerLoaded", str(path))])
        else:
            self._show_rows([("fatal", "LayerInvalid", str(path))])

    def _build_report_page(self) -> QWidget:
        page = QWidget()
        self._report_tabs = QTabWidget()
        self._report_tables: dict[str, QTableWidget] = {}
        for key, title in (("import", "接入 (A)"), ("terrain_mesh", "地形/网格 (B-C)"), ("field", "字段 (D)"),
                           ("coupling", "耦合 (E/E')"), ("export", "导出 (F)"), ("reproducibility", "可复现")):
            table = QTableWidget(0, 2)
            table.setHorizontalHeaderLabels(["项", "值"])
            table.horizontalHeader().setStretchLastSection(True)
            table.setEditTriggers(QTableWidget.EditTrigger.NoEditTriggers)
            self._report_tables[key] = table
            self._report_tabs.addTab(table, title)
        self._findings_table = QTableWidget(0, 6)
        self._findings_table.setHorizontalHeaderLabels(["severity", "code", "feature_id", "kind", "violation", "detail"])
        self._findings_table.horizontalHeader().setStretchLastSection(True)
        self._findings_table.setEditTriggers(QTableWidget.EditTrigger.NoEditTriggers)
        self._findings_table.setSelectionBehavior(QTableWidget.SelectionBehavior.SelectRows)
        refresh = QPushButton("刷新报告")
        refresh.clicked.connect(self._refresh_reports)
        locate = QPushButton("定位选中 finding 的对象")
        locate.clicked.connect(self._locate_finding)
        self._gate_summary = QLabel("门禁：未运行")
        bar = QHBoxLayout()
        bar.addWidget(refresh)
        bar.addWidget(locate)
        bar.addWidget(self._gate_summary)
        bar.addStretch()
        layout = QVBoxLayout(page)
        layout.addLayout(bar)
        layout.addWidget(self._report_tabs)
        layout.addWidget(QLabel("Findings（对象级；点击“定位”在地图上选中并缩放）"))
        layout.addWidget(self._findings_table)
        return page

    def _refresh_reports(self) -> None:
        job = self._current_job()
        if not job["output_dir"]:
            self._show_rows([("fatal", "MissingOutputDir", "请先在作业页选择输出目录")])
            return
        for key, rows in jobio.report_pages(job).items():
            table = self._report_tables[key]
            table.setRowCount(len(rows))
            for r, (k, v) in enumerate(rows):
                table.setItem(r, 0, QTableWidgetItem(k))
                table.setItem(r, 1, QTableWidgetItem(v))
        validation = jobio.load_validation_for(job)
        rows = jobio.findings_table(validation)
        self._findings_table.setRowCount(len(rows))
        for r, row in enumerate(rows):
            for c, key in enumerate(("severity", "code", "feature_id", "kind", "violation", "detail")):
                self._findings_table.setItem(r, c, QTableWidgetItem("" if row.get(key) is None else str(row[key])))
        can_export = jobio.exportable(validation)
        status = (validation or {}).get("status")
        self._gate_summary.setText(
            f"门禁：{'允许导出' if can_export else '禁止导出'}（status={status}, findings={len(rows)}）")
        self._export_button.setEnabled(can_export)

    def _locate_finding(self) -> None:
        row = self._findings_table.currentRow()
        if row < 0:
            self._show_rows([("fatal", "NoFindingSelected", "先在 findings 表中选择一行")])
            return
        kind = self._findings_table.item(row, 3).text() or None
        feature_id = self._findings_table.item(row, 2).text() or None
        located = jobio.locate_object(self._current_job(), kind, feature_id)
        if not located:
            self._show_rows([("info", "NoLocator", f"kind={kind!r} feature_id={feature_id!r} 没有可定位的图层"
                                                    "（耦合对象需先在耦合编辑器刷新连线图层）")])
            return
        name, path, expression = located
        project = QgsProject.instance()
        layer = next((l for l in project.mapLayers().values() if l.name() == name), None)
        if layer is None:
            layer = QgsVectorLayer(path, name, "ogr")
            if not layer.isValid():
                self._show_rows([("fatal", "LayerInvalid", path)])
                return
            project.addMapLayer(layer)
        layer.selectByExpression(expression)
        if self._iface is not None and layer.selectedFeatureCount() > 0:
            self._iface.mapCanvas().zoomToSelected(layer)
        self._show_rows([("info", "Located", f"{name}: {expression} -> {layer.selectedFeatureCount()} feature(s)")])

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
            coupling_mode=self._coupling_mode.currentData(),
            roof_node_max_distance_m=self._roof_distance_spin.value(),
            dpm_rule_table=self._rule_table_edit.text().strip() or None,
            crs_policy=self._crs_policy_edit.text().strip() or None,
            terrain_policy=self._terrain_policy_edit.text().strip() or None,
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
        self._export_button.setEnabled(can_export)

    def _export_case(self) -> None:
        """B6: re-runs the pipeline with export_case so the exporter's own gate
        (validation status, effective links complete, hashes) decides; the UI
        never assembles the package itself."""
        target = self._export_target_edit.text().strip()
        if not target:
            self._show_rows([("fatal", "MissingExportTarget", "请填写案例包目标目录")])
            return
        repo_root = self._repo_edit.text().strip()
        job = self._current_job()
        job["export_case"] = {"target_dir": target, "force": True}
        job_path = jobio.write_job_config(job, job["output_dir"])
        result = jobio.run_pipeline(job_path, repo_root)
        rows = jobio.findings_rows(result.get("validation"))
        if result.get("stderr") and result.get("validation") is None:
            rows.insert(0, ("fatal", result["status"], result["stderr"][-2000:]))
        self._show_rows(rows)
        self._export_button.setEnabled(jobio.exportable(result.get("validation")))

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

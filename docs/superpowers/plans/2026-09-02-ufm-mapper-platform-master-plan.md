# UFM-Mapper 平台总体规划 v3（RAS Mapper 式现代化 GIS 全流程数据加工与处理平台）

日期：2026-09-02
状态：PLANNED（总体蓝图；不改变 M281 D-5 BLOCKED 决策；不修改任何已激活 gate）
层级关系：
- 本文件 = **平台全景蓝图**（产品定义、能力矩阵、架构、契约、阶段、里程碑、验收）；
- `2026-09-02-ufm-mapper-gis-platform-plan.md`（v2）= 参考蓝图与现状的**差距裁决**，其裁决结论本文件全部继承；
- `2026-08-26-m287-gis-preproc-workbench-design.md`（v1.1）= 前处理工作台**治理约束与原始设计**；
- `2026-09-03-ufm-mapper-1d-network-authoring-plan.md`（N 系列）= 项目方 2026-09-03 选定的范围扩展：
  SWMM 管网作者化 + D-Flow FM 河网草绘，其治理修订（§5）已并入本文件；
- 凡冲突处，以主 Spec / 稳定性协议 / 符号表 > v1.1 §2 治理约束 > v2 裁决 > 本文件为序。

---

## 0. 一页摘要

UFM-Mapper 是 SCAU-UFM 的 RAS Mapper 等价物：把原始 GIS 图层（DEM、土地利用、
建筑、边界、SWMM 管网、D-Flow FM 河网）加工为模型可直接消费的**标准案例包**
（STCF v5/CF-UGRID 网格 + 物理场 + 三链路耦合映射 + SimDriver 配置片段），并在
运行后把结果**只读**制图回 GIS。三条铁律（v1.1/v2 已实证）：

1. **CLI 为本，UI 为壳**——UI 只生成 `job_config.json`、调用子进程、重读报告；
2. **校验只有一份**——拓扑/schema/数值校验的权威实现在 C++（`libs/mesh`、
   `libs/stcf`、`scau_preproc validate`），Python/UI 不复制；
3. **Fail-Closed**——缺件、越界、退化、超时一律阻断并产出可定位诊断；禁止部分
   网格、静默修复、静默降级。

截至 2026-09-02 已落地：M287-A/B/C（剖分 spike、流水线 v1、三链路映射生成器）、
E1（作业页）、**B4 + E3（网格控制 + 网格工作台，G32）**；GoldenSuite G1–G32。
未落地的完整平台能力按本文件 §8 路线图推进，主线终点是 **M287-F 三模型全耦合真实
案例发布门禁** 与 **M288 结果制图线**。

---

## 1. 产品定义

### 1.1 使用者与场景

| 角色 | 典型任务 | 平台入口 |
|---|---|---|
| 模型工程师 | 从城市 GIS 数据建一个可运行案例；调网格；确认耦合关系；导出 | QGIS 插件（所见即所得） |
| 数据提供方/审核员 | 检查输入契约（CRS/单位/授权/字段）；处置 review 项 | 数据目录页 + 门禁页 |
| CI / 批处理 | 无人值守重建案例包、比对哈希、跑 GoldenSuite | `scau_preproc` CLI + `job_config.json` |
| 结果分析人员 | 淹没深度/范围/到达时间制图；点时序；与 SWMM/D-Flow 结果联看 | 结果制图页（M288） |

### 1.2 与 RAS Mapper 的能力对齐矩阵

| RAS Mapper 能力 | UFM-Mapper 对应 | 状态 | 归属 |
|---|---|---|---|
| Terrain 图层（多 DEM 合并、优先级） | DEM 接入 + 采样 → `z_b`；多 DEM 拼接 | 单 DEM DONE；拼接 = B3b | B3 |
| Terrain 修改（Terrain Modification：渠道切割、堤防抬升） | DEM 调理（填洼/水系强制/局部抬升）须 `terrain_condition_policy.json` 授权 | 未做 | B3 |
| Land Cover → Manning LUT | landcover → `manning_n` LUT | DONE（G30） | — |
| Infiltration layer | soil → Green-Ampt LUT | 占位 | B5 |
| 2D Flow Area + Breaklines + Refinement Regions | `mesh_controls.geojson`（breakline / refinement_region） | **DONE（G32）** | — |
| Mesh 质量检查（cell 尺寸、边角） | `mesh_quality.json` + `mesh_quality_cells.geojson` 热力图 | **DONE（E3）** | — |
| Geometry：河道中心线/结构（几何） | **N2 河网草绘**：节点/河段/地表交界候选 → UGRID 1D 网络 + MDU 模板；水力参数 `provider_required` | 未做 | N2 |
| Geometry：断面/糙率/边界水力参数 | 归 D-Flow FM 原生工具或 provider；平台只留显式占位并阻断导出 | 不做 | — |
| 1D 管网建模（SWMM 编辑器等价物） | **N1 管网作者化**：井/排放口/管道图层 ↔ 受治理 `.inp` 写出 + 真实引擎解析；既有 `.inp` 导入编辑 | 未做 | N1 |
| SA/2D Connections（1D↔2D 连接） | 三链路耦合映射 + 确认契约 + 编辑器；无显式表时空间候选模式（全 review）；编辑器新建链接（必经确认）；作者化节点复用同一路径 | 候选 DONE；确认/空间候选/编辑 = C4/C5/E4 | C4/C5/E4 |
| Projection 管理 | CRS 治理重投影（米制直角，拒绝 lat/lon） | rejection-only | B2 |
| Results：Depth/WSE/Velocity/Arrival Time/Duration 栅格与动画 | 时序 NetCDF 契约 + `scau_results` CLI + 结果页 | 完全缺失 | M288 |
| Profile Lines / Time series plots | 断面/点时序抽取 | 缺失 | M288-B/C |
| RAS Mapper 层的"计算不修改数据" | 结果层只读；派生制图确定性且带来源哈希 | 原则已定 | M288 |
| 项目文件（.prj/.rasmap） | `job_config.json` + `pipeline_manifest.json` +（新增）`project.ufm.json` 工程索引 | 索引缺失 | E7 |
| WebGIS | 不做（远期可选，无状态管道保证低成本加壳） | — | 远期 |

---

## 2. 系统架构

### 2.1 分层

```text
┌─ L4 前端（QGIS 插件，Python，GPL 壳；零计算状态）──────────────────────────────┐
│ P1 项目向导 │ P2 数据目录树 │ P3 字段映射 │ P4 网格工作台(E3✔) │ P5 参数表      │
│ P6 耦合编辑器 │ P7 质量门禁 │ P8 导出与复现 │ P9 结果制图(M288-C)             │
└──────────────┬─────────────────────────────────────────────────────────────────┘
               │ 唯一通道：job_config.json → 子进程 CLI → 重读 JSON/GeoJSON 报告
┌──────────────▼─────────────────────────────────────────────────────────────────┐
│ L3 编排层（python/scau_preproc；可无头运行）                                     │
│  A 接入契约 │ B 地形/几何 │ C 受控剖分(gmsh 子进程) │ D 字段派生               │
│  E 耦合映射 │ E' 确认合并 │ F 权威校验 + 案例包原子导出 │ R 结果后处理(M288-B) │
└──────────────┬─────────────────────────────────────────────────────────────────┘
               │ C++ 权威校验（唯一一份）：libs/mesh · libs/stcf · stcf_bridge
┌──────────────▼─────────────────────────────────────────────────────────────────┐
│ L2 案例包 case/（§4 文件契约；SHA-256 清单；可复现命令）                          │
└──────────────┬─────────────────────────────────────────────────────────────────┘
               │ apps/sim_driver（RuntimeConfig v2 消费 simdriver_links.conf）
┌──────────────▼─────────────────────────────────────────────────────────────────┐
│ L1 运行与结果（M288-A 受治理时序输出，默认关闭）                                  │
└────────────────────────────────────────────────────────────────────────────────┘
```

### 2.2 不变式（继承并重申）

- 许可证边界：QGIS（GPL）与 gmsh（GPL）只经"子进程 + 文件"接触；模型内核与
  后端不链接二者；
- `Surface2DCore` 不依赖任何 1D 引擎 ABI；SWMM 与 D-Flow FM 原生对象互不可见；
- 前处理只产出数据与映射；`Q_limit`、`V_limit`、`mass_deficit_account`、
  rollback/replay、仲裁永远归 CouplingLib；`IDFlowFMEngine` 不承载任何耦合语义；
- 机器可读字段名遵循符号表；`phi_t`/`Phi_c`/`phi_e_n`/`omega_edge` 分立；
- 结果层只读运行产物，不做物理量再计算/"修正"；派生制图确定性并携带来源哈希；
- 任何对输入数据的修改（几何修复、DEM 调理、CRS 变换）= "修复"，须 policy
  显式授权 + 逐项审计记录，否则 fatal。

### 2.3 模块清单（目标形态）

| 模块 | 位置 | 现状 |
|---|---|---|
| `scau_preproc.pipeline` 编排器 | `python/scau_preproc/pipeline.py` | v1 + B4 |
| `meshgen` / `mesh_controls` | `python/scau_preproc/` | DONE |
| `coupling_maps` 三链路生成器 | `python/scau_preproc/coupling_maps.py` | DONE（dflowfm 占位） |
| `crs_governance`（B2） | `python/scau_preproc/crs_governance.py` | 待建 |
| `terrain_condition`（B3） | `python/scau_preproc/terrain_condition.py` | 待建 |
| `field_derivation`（B5：soil LUT、DPM 规则表） | `python/scau_preproc/field_derivation.py` | 待建（当前占位在 meshgen.assign_fields） |
| `confirmations`（C4） | `python/scau_preproc/confirmations.py` | 待建 |
| `case_export`（B6） | `python/scau_preproc/case_export.py` | 待建（须消费 N1 模式与 N2 `provider_required`） |
| `inp_io` / `swmm_author`（N1） | `python/scau_preproc/` + `scau_preproc swmm-parse` | 待建 |
| `dflowfm_author`（N2） | `python/scau_preproc/`（从 `tools/dflowfm/generate_single_reach_1d.py` 提炼）+ `scau_preproc ugrid1d-validate` | 待建 |
| `scau_results` CLI（M288-B） | `python/scau_results/` 或 `apps/results_cli` | 待建 |
| 权威校验 | `apps/preproc_cli`（`generate/validate`）、`libs/stcf`、`libs/mesh` | DONE；B6 需 `validate --case-dir` |
| QGIS 插件 | `qgis_plugin/scau_preproc_workbench/`（`jobio.py` 纯层） | E1/E3 DONE |
| 部署/打包 | `tools/qgis/deploy_plugin.sh` | 拷贝部署 |

---

## 3. 全流程流水线（阶段 × 输入 × 输出 × 检查 × 人工边界）

编号沿用 v1.1/v2 的 A–G，并新增 E'（确认合并）与 R（结果后处理）。

| 阶段 | 输入 | 自动处理 | 输出 | Fail-Closed 检查 | 必须人工确认 |
|---|---|---|---|---|---|
| **A 数据接入契约** | 输入包（`manifest.json` + 数据集） | 7 项必需文件存在性；`canonical_targets` 合法；`geometry_clean_policy` 版本化；**B2** CRS/单位读取与受控重投影 | `validation.json`（import 段）、重投影审计样本 | 缺件、未知目标字段、地理坐标、单位非米、未授权样本 | 授权、垂直基准、无效几何处置 |
| **B 地形与几何** | DEM、边界、建筑 | 边界裁剪、DEM 采样 → `z_b`、建筑 hole 提纯、自相交/未闭合检测；**B3** 填洼/水系强制/局部改造（policy 授权） | 诊断 GeoJSON、`terrain_condition_report.json`、洼地/改动量栅格 | 退化几何、NoData 采样、未授权调理 | 建筑阻塞/开口语义、DEM 调理方案 |
| **C 网格剖分**（✔） | 边界 + hole + `mesh_controls.geojson` | 约束剖分、size fields、embed、拓扑构建、质量报告、逐单元诊断、超时 kill | `case.stcf.nc`（几何部分）、`mesh_quality.json`、`mesh_quality_cells.geojson` | 部分网格、非流形、约束线未保留、控制要素违规 | 尺寸/加密策略、review 项处置 |
| **D 字段派生** | landcover、soil、buildings、`dpm_rule_table.json` | `manning_n` LUT（✔）；**B5** soil → Green-Ampt LUT、规则表 → `phi_t`/`Phi_c`/`omega_edge`/`phi_e_n` | 字段写入 `case.stcf.nc`、`field_derivation_report.json` | 未映射对象、`theta_i < theta_s <= 1` 违反、`validate_dpm_consistency` 违反、规则表未批准 | 参数表/规则表本身 |
| **E 耦合映射**（✔候选） | SWMM `.inp`、映射 CSV、建筑、河网 provider | 三链路候选（显式 ID > 空间包含 > 近邻，带距离/置信度） | `coupling/*_mapping.json`、`mapping_report.json`、`simdriver_links.conf` | 节点落孔洞、引用未知对象、无 provider 证据 | 所有 `confidence: review`、一对多/多对一、交换高程 |
| **E' 确认合并**（C4） | 候选 + `coupling/confirmed/*.json` | 哈希级一致性校验（候选漂移即 review）；确认结果覆盖候选生成最终链接 | `coupling/effective_links.json`、`simdriver_links.conf`（最终） | 确认引用的候选已漂移、确认者/时间缺失 | 无（此阶段消费人工结果） |
| **F 校验与导出**（B6） | 全部产物 | C++ 权威校验（`validate --case-dir`）→ 组装 `case/` → 原子 rename → 包级 SHA-256 → 可复现命令 | `case/`（§4） | 任何 fatal、未确认 review、哈希不一致 | 无（门禁自动） |
| **G 运行**（既有 SimDriver） | `case/` | SimDriver 冷启动消费 | `run_summary.json`；**M288-A** 时序 NetCDF | 配置不完整、引擎不可用 | 发布判定走稳定性协议 |
| **R 结果后处理**（M288-B） | 时序 NetCDF + run manifest | 最大深度/范围包络、到达时间、持续时间、点/断面时序、GeoTIFF 栅格化 | `results/*.nc`、`results/*.json`、GeoTIFF | 来源哈希不匹配、变量集不完整 | 结果解读 |

**人工确认的实现原则**（贯穿 E'/P6/P7）：人工结果是**独立版本化输入文件**，
生成器输出永远只读；重跑流水线时对确认与候选做哈希一致性校验。

---

## 4. 文件契约（单一契约清单，以落地为准）

### 4.1 现行契约（已定版；只能扩展不能另起）

```text
job_config.json                       # schema v1（可选块：mesh_controls）
validation.json                       # status + findings（severity/code/detail/objects[]）
pipeline_manifest.json                # 源/配置/案例 SHA-256、质量摘要、mesh_controls 哈希、可复现命令
mesh/case.stcf.nc                     # STCF v5 + CF-UGRID
mesh/mesh_quality.json                # 全局质量 + per_cell_diagnostics + mesh_controls 报告
mesh/mesh_quality_cells.geojson       # 逐单元显示诊断（非认证）
mesh_controls.geojson                 # mesh_controls_schema_version = 1
generator.diagnostic.geojson          # 肇事要素（feature_id/kind/violation）
coupling/surface_swmm_mapping.json    # coupling_mapping_schema_version = 1
coupling/roof_drain_mapping.json
coupling/surface_dflowfm_mapping.json # 当前 status = provider_required
coupling/mapping_report.json
simdriver_links.conf                  # SimDriver RuntimeConfig version = 2 片段
```

### 4.2 规划增量契约（随对应切片定版；均需 schema_version 字段）

```text
metadata/crs_policy.json                 # B2：目标 CRS（EPSG/WKT）、proj pipeline 字符串、允许的源 CRS 集合
metadata/terrain_condition_policy.json   # B3：调理授权（fill/enforce/modify）、算法与参数版本、审计要求
metadata/dpm_rule_table.json             # B5：规则表格式（先行）；值随 D-5 批准
metadata/soil_lut.json                   # B5：soil_type → K_s/psi_f/theta_s/theta_i（替代 CSV 或与之并存需裁决）
coupling/confirmed/<mapping_id>.json     # C4 ✔：confirmation_schema_version=1 {chain, mapping_id, decision: accept|reject|retarget|create, candidate_sha256, confirmed_by, timestamp, target}
swmm/drainage_network.geojson            # N1：drainage_network_schema_version = 1（井/排放口/管道 + 白名单 OPTIONS）
swmm/model.inp.authoring.json            # N1：{mode: authored|external, 哈希, writer_version, engine_parse}
dflowfm/river_sketch.geojson             # N2：river_sketch_schema_version = 1（节点/河段/交界候选）
dflowfm/<case>_net.nc + <case>.mdu       # N2：UGRID 1D 网络 + 模板（水力键 TODO_PROVIDER）
dflowfm/authoring_manifest.json          # N2：provider_required 清单（非空即导出禁用）
coupling/effective_links.json            # C4 ✔：合并后的最终链接（只由 E' 产出）+ effective/simdriver_links.conf
case/manifest 扩展                       # B6：pipeline_manifest.json 增 package_files[{path, sha256}] + reproduce[]
project.ufm.json                         # E7：工程索引（输入包、作业列表、最近产物、UI 状态快照——可再生、非权威）
results/<run_id>/surface_timeseries.nc   # M288-A：CF/UGRID 时序（h, eta, hu, hv, wet_mask）
results/<run_id>/results_manifest.json   # M288-A/B：运行 manifest 哈希、变量集、写出频率
results/<run_id>/max_depth.nc|.tif       # M288-B：确定性派生制图（携带来源哈希）
results/<run_id>/arrival_time.nc|.tif
results/<run_id>/timeseries/<point_id>.csv
```

### 4.3 明确不采纳（继承 v2 裁决）

`case_manifest.yaml`（由 `pipeline_manifest.json` 承载）、`*_mapping.csv` 作为输出
（输入 CSV 保留，输出为 JSON + conf）、任何 UI 私有后端协议。

### 4.4 `job_config.json` 目标全貌（schema v1 的可选块累积；不升版）

```json
{
  "job_config_schema_version": 1,
  "package": "...", "output_dir": "...", "case_name": "case.stcf.nc",
  "characteristic_length_m": 8.0, "recombine": true, "determinism_check": true,
  "validator_cli": "...",
  "mesh_controls": {"geojson": "...", "default_size_m": 3.0, "default_dist_max_m": 12.0},
  "crs": {"policy": "metadata/crs_policy.json"},                                  // B2
  "terrain_condition": {"policy": "metadata/terrain_condition_policy.json"},      // B3
  "field_derivation": {"dpm_rule_table": "...", "soil_lut": "..."},               // B5
  "coupling_maps": true,
  "confirmations_dir": "coupling/confirmed",                                      // C4 ✔（缺省 = 输入包 coupling/confirmed/）
  "export_case": {"target_dir": "case/", "require_all_confirmed": true},         // B6
  "drainage_network": {"mode": "authored", "geojson": "...", "generate_coupling_candidates": true}, // N1
  "river_sketch": {"geojson": "...", "case_name": "river", "dll_smoke": false}     // N2
}
```

---

## 5. 前端（QGIS 插件）页面规格

UI 纪律（全切片强制）：每页只做"渲染选择 → job_config → 子进程 → 重读报告"；
新增逻辑先落 `jobio.py` 纯层（可 headless 测试）；仓库根目录经 QgsSettings
持久化 + fail-closed 校验；每页有离屏冒烟脚本（沿用 E3 的 `python-qgis.bat`
+ `QT_QPA_PLATFORM=offscreen` 配方）。

| 页 | 名称 | 核心交互 | 消费/产出 | 依赖 | 切片 |
|---|---|---|---|---|---|
| P1 | 项目向导 | 选择输入包/输出目录/仓库根；CRS 与单位显示；完整性 7 项灯 | `manifest.json`、`crs_policy.json` | B2 | E2 |
| P2 | 数据目录树 | Terrain / Land Cover / Geometries / 1D Networks 分组；逐层 Pass/Review/Fatal 灯；点击 finding 定位到对象 | `validation.json.findings[].objects` | — | E2 |
| P3 | 字段映射 | 源字段 → canonical 目标（下拉只允许 canonical 集合） | `manifest.canonical_targets`、`field_dictionary.csv` | — | E2 |
| P4 | 网格工作台（✔） | 草稿层绘制 → `mesh_controls.geojson`；热力图；诊断层 | 已落地 | — | E3 ✔ |
| P5 | 语义与物理参数表 | 浏览/编辑 soil LUT、Manning LUT、DPM 规则表（编辑即写新版本文件，不改原文件） | `soil_lut.json`、`dpm_rule_table.json` | B5 | E5a |
| P6 | 耦合关系编辑器 | 地图高亮 1D 节点↔2D 单元连线；`review` 标红；确认/拒绝/改目标 → 写 `coupling/confirmed/*.json` | 映射 JSON、确认契约 | C4 | E4 |
| P7 | 质量门禁 | 全量报告浏览器（import/terrain/topology/field/coupling/reproducibility 分页）；fatal/review/pass 计数；点击定位 | 全部报告 | — | E5 |
| P8 | 导出与复现 | 一键 Run Pipeline；导出按钮在 fatal/未确认时禁用（复用 `jobio.exportable`）；显示可复现命令与包哈希 | B6 导出器 | B6 | E5 |
| P9 | 结果制图 | 时间轴播放；深度/流速分级渲染；点击取时序；与 SWMM `.out`/D-Flow map 只读联看 | M288-A/B 产物 | M288-B | M288-C |
| P10 | 管网工作台 | junction/outfall/conduit 草稿层；端点吸附；`.inp` 导入；保存 → `drainage_network.geojson` → 写出 + 真实引擎解析结果行；耦合角色着色 | N1 产物 | N1-B | N1-D |
| P11 | 河网草绘 | river_node/river_branch/surface_interface_candidate 草稿层；吸附；导出 net/mdu；`provider_required` 计数灯 | N2 产物 | N2-B | N2-D |
| — | 打包收口 | 图标、翻译、Plugin Reloader 说明、QGIS 3.40/4.x 双轨冒烟清单、`project.ufm.json` | — | E2–E5 | E6/E7 |

---

## 6. 无头运行、批处理与 CI

- **单命令入口**：`py -3 -m scau_preproc.pipeline job.json`（既有）；新增
  `py -3 -m scau_preproc.export job.json`（B6）与 `py -3 -m scau_results <cmd>`（M288-B）。
- **批处理**：`jobs/*.json` 目录级循环 + 汇总 `batch_report.json`（每作业 status/哈希）；
  失败不阻断其余作业但汇总 exit 非零。
- **CI 形态**：hosted lane 无 gmsh → golden 只加载**已提交 fixture**（G30/G32 模式）；
  重生成证据在流水线侧（`determinism_check`）。B7 引入受治理 Linux gmsh 后，
  增加 `preproc-regeneration` 自托管/容器 lane 对比双平台哈希。
- **门禁纪律**：三处同步（`goldensuite.json` + `check_manifest.py` REQUIRED +
  未加引号 `LABELS golden`）；非门禁候选用 `golden_candidate`。

---

## 7. 溯源、可复现与治理

- 每个产物文件在 `pipeline_manifest.json` 有 SHA-256；`reproduce` 给出精确命令；
- 所有"修复类"操作（几何、DEM、CRS）逐项记录 `{object_id, rule_id, original, repaired}`；
- 人工确认带 `confirmed_by / timestamp / candidate_sha256`；候选漂移即 review；
- 参数表/规则表为**版本化文件**，UI 编辑生成新版本而非就地改写；
- 稳定性协议状态（`ABORT / REVIEW_REQUIRED / BLOCK_MERGE / BLOCK_RELEASE`）在
  报告与门禁页原样保留，不做 UI 侧"翻译"或弱化。

---

## 8. 路线图（里程碑 · 依赖 · 出口标准）

编号为规划占位；落地当刻取最小空闲 M/G 号。每切片按"设计 → failure-revealing
候选 → 实现 → 专属 GoldenTest → manifest/CI → 证据文档"推进。

### 8.1 已完成

| 切片 | 内容 | Gate | 证据 |
|---|---|---|---|
| M287-A | gmsh 剖分 spike | — | 2026-08-27 evidence |
| M287-B | 流水线 v1 | G30 | 2026-08-27 evidence |
| M287-C | 三链路映射生成器 + SimDriver 消费 | G31 | 2026-08-28 evidence |
| M287-E1 | QGIS 作业页 | — | 2026-08-30 evidence |
| **M287-B4 + E3** | 网格控制 + 网格工作台 | **G32** | 2026-09-02 evidence |
| **M287-C4** | 耦合确认契约 + 阶段 E'（effective links、漂移检测、review 门禁） | **G33** | 2026-09-04 evidence |

### 8.2 阶段 I ——"候选 → 确认 → 固化"闭环（当前主线，下一步）

| 切片 | 内容 | 依赖 | 出口标准 / Gate 候选 |
|---|---|---|---|
| **C4 确认契约** | `coupling/confirmed/*.json` schema；E' 合并阶段；候选哈希漂移检测；`effective_links.json` → `effective/simdriver_links.conf` | C ✔ | **DONE 2026-09-04（G33 `ci_gate:true`）**：`superpowers/specs/2026-09-04-m287-c4-confirmation-contract-evidence.md` |
| **C5 空间候选模式** | 映射模式 `explicit_ids` / `spatial_candidates` / `mixed`（互斥声明进 `job_config.coupling_maps.mode`，记录进 `mapping_report.json` 与 manifest）：无显式表时对每个 SWMM 节点按单元包含生成候选（落孔洞仍 fatal、边界外 review）；屋面按建筑质心→最近节点（距离阈值版本化）；显式表部分覆盖时表内 `high`、表外 `review` 分别计数；`exchange_elevation_m` 取单元 `z_b` 并标注 `placeholder_source`；全部 `needs_confirmation`，SimDriver 在未确认前拒绝消费 | C4 | golden：合成包移除映射 CSV → 全 review 候选；C4 确认后消费与 G31 断言一致；负向：节点落孔洞 fatal、未确认候选被 SimDriver 拒绝 |
| **E4 耦合编辑器** | P6 页：连线图层、review 标红、确认/拒绝/改目标写契约文件；**新建链接**（任意 1D 节点拖到 2D 单元 → `decision: create` 候选，必经确认）；不改生成器输出 | C4, C5 | 离屏冒烟；headless `jobio` 测试；确认文件与契约 schema 一致 |
| **E2 数据目录树 + P1/P3** | 分组树、状态灯、finding → 对象定位、字段映射下拉 | A 报告已含对象 ID | 离屏冒烟；三类状态灯断言 |

### 8.3 阶段 II —— 输入治理与真实字段（可与阶段 I 并行）

| 切片 | 内容 | 依赖 | 出口标准 / Gate 候选 |
|---|---|---|---|
| **B2 CRS 治理** | pyproj 受控重投影；`crs_policy.json`；proj pipeline 字符串版本化进 manifest；地理坐标一律 fatal；重投影审计样本 | — | 重投影确定性 golden（同平台逐位）；lat/lon 拒绝负向证据 |
| **B3 DEM 调理** | Priority-Flood 填洼（算法/参数版本化）、可选水系强制、局部改造；`terrain_condition_policy.json` 授权；洼地/改动量诊断栅格；多 DEM 拼接 | B2 | 调理前后差异审计 golden；关闭策略时逐位不变 |
| **B5 字段真实派生（格式先行）** | `soil_lut.json`、`dpm_rule_table.json` schema + 校验器；合成规则表驱动 `phi_t`/`Phi_c`/`omega_edge`/`phi_e_n` 派生；未映射对象 fail-closed | — | 收紧 G30 占位断言为规则表派生值断言；`validate_dpm_consistency` 全过 |
| **E5a 参数表页** | P5：LUT/规则表浏览与版本化编辑 | B5 | 离屏冒烟；编辑产生新版本文件 |

### 8.4 阶段 III —— 前处理收口

| 切片 | 内容 | 依赖 | 出口标准 / Gate 候选 |
|---|---|---|---|
| **B6 案例包导出器** | 组装 `case/`；先全量 C++ 校验（`validate --case-dir`）后原子 rename；`swmm/model.inp`、`dflowfm/*` 原样复制；包级 SHA-256 + 可复现命令 | C4 | golden：导出包被 `validate` + SimDriver 冷启动直接消费；重复导出逐位一致 |
| **E5 门禁与导出中心** | P7 + P8：报告浏览器分页；导出禁用逻辑；一键导出 | B6 | 离屏冒烟：fatal/未确认 → 禁用；ok → 导出成功 |
| **B7 双平台确定性** | 受治理 Linux gmsh（third_party 治理）；跨平台哈希实验 | — | 证据决定门禁形态："逐位一致"或"记录在案的 1e-12 容差" |
| **E6/E7 打包收口** | 图标、翻译、双轨冒烟清单；`project.ufm.json` 工程索引 | E2–E5 | 双版本 QGIS 冒烟清单勾完 |

### 8.4b 阶段 III' —— 1D 网络作者化（N 系列；详见 `2026-09-03-ufm-mapper-1d-network-authoring-plan.md`）

| 切片 | 内容 | 依赖 | 出口标准 / Gate 候选 |
|---|---|---|---|
| **N1-A** `.inp` 白名单 IO | 读写 + 语义比较；非白名单节 fatal | — | `swmm_inp_roundtrip` 候选 |
| **N1-B** 作者化写出器 | `swmm_author` + `scau_preproc swmm-parse`（真实 SwmmEngine 干跑）；模式互斥、原件保护 | N1-A | `swmm_authored_network_parse`（可门禁：SWMM 已嵌入） |
| **N1-C** 耦合候选生成 | 作者化节点直接复用 C5 空间候选模式（`coupling_role` 仅作过滤/着色） | N1-B, C5 | C4 确认后 SimDriver 消费 |
| **N1-D** P10 管网工作台页 | 草稿层/吸附/导入/写出 | N1-B | 离屏冒烟 |
| **N2-A** 河网草绘契约 + UGRID 写出 | `river_sketch.geojson`；`dflowfm_author`；`ugrid1d-validate` | — | `dflowfm_sketched_network_roundtrip` 候选 |
| **N2-B** MDU 模板 + provider_required 门禁 | 模板；`authoring_manifest`；导出禁用；自托管 DLL 干跑 | N2-A | 负向门禁；DLL 干跑 `golden_candidate` |
| **N2-C** 交界候选 → 映射候选 | 并入 C3 | N2-A, C3 | `provider_required` 负向保持 |
| **N2-D** P11 河网草绘页 | 草稿层/吸附/导出/占位灯 | N2-B | 离屏冒烟 |

### 8.5 阶段 IV —— 耦合链路接线（按 CouplingLib 就绪度插入）

| 切片 | 内容 | 依赖 | 出口标准 / Gate 候选 |
|---|---|---|---|
| **C2 roof 链 SimDriver 接线** | run_loop 增 roof 配置键；消费 `roof_drain_mapping` 确认结果 → RoofSwmmStepDriver | C4 | golden：合成包 roof 映射驱动 mock/real SWMM 多子步；守恒复用 M247 闭合框架 |
| **C3 D-Flow FM 边界链** | 依赖 M273/M276 边界契约；`surface_dflowfm_mapping.json` 从 `provider_required` 升级；中心线不得单独推断边界 | provider 证据 | golden：合成单河段端到端 + river link 消费；无 provider 时保持 `provider_required` 负向测试 |

### 8.6 阶段 V —— 结果制图线（M288）

| 切片 | 内容 | 依赖 | 出口标准 / Gate 候选 |
|---|---|---|---|
| **M288-A 输出契约** | SimDriver/Surface2D 受治理时序 writer（CF/UGRID：`h`、`eta`、`hu`/`hv`、`wet_mask`）；RuntimeConfig 显式声明频率/变量集；**默认关闭** | 独立 failure-revealing 评审 | schema + 写读 roundtrip golden；关闭态逐位零影响证据（GoldenSuite 全绿） |
| **M288-B `scau_results` CLI** | 最大深度/范围、到达时间、持续时间、点/断面时序、GeoTIFF；全部确定性并携带运行 manifest 哈希 | M288-A | 合成案例两次后处理逐位一致 |
| **M288-C 结果页** | P9：时间轴、分级渲染、点击取时序；SWMM `.out`/D-Flow map 只读联看 | M288-B | 无状态管道合规；操作者核查清单 |

### 8.7 阶段 VI —— 真实数据与发布

| 切片 | 内容 | 依赖 | 出口标准 |
|---|---|---|---|
| **M287-D 真实数据绑定** | importer spike → 拓扑/字段验证 → STCF 绑定 → 真实小样 golden；灌入批准的规则表参数值 | **M281 解锁**（授权样本 + 格式契约） | 真实小样 golden |
| **M287-F 收口** | 三模型全耦合案例包（B6 + C2/C3 + D）+ 全系统质量审计（复用 M270/G24）+ M288 冒烟 + GoldenSuite/CI 发布门禁对齐 | 全部 | 稳定性协议 evidence 状态全链路保留 |

### 8.8 关键路径与并行建议

```text
C4 ─► C5 ─► E4 ─► B6 ─► E5 ─► (C2, C3) ─► M287-F
 │            ▲
 └─ E2 ──────┘        B2 ─► B3 ─┐
                      B5 ─► E5a ┘ (并行于 C4/E4；在 B6 前汇合)
N1-A ─► N1-B ─► N1-C(复用 C5) ─► N1-D        （N1-A/B 与 C4/C5/E4 并行；N1-B 契约先于 B6 定版）
N2-A ─► N2-B ─► N2-D；N2-C 等 C3               （N2-A 与 B2/B3 并行）
M288-A(设计评审可在 B6 后并行启动) ─► M288-B ─► M288-C ─► M287-F
M287-D 独立等待 M281；B7 独立实验
```

建议下一切片（C4 已落地）：**C5 → E4**（确认契约 → 空间候选模式 → 耦合编辑器）——它们
是导出门禁语义完整的前提，也是 RAS Mapper "SA/2D Connections 手动确认"体验的对应物。

---

## 9. 验收标准（平台级 Definition of Done）

1. 从 D-5 合成包出发，**仅经 UI 操作**即可完成：接入 → 网格（含控制）→ 字段 →
   耦合候选 → 人工确认 → 导出 `case/` → SimDriver 冷启动运行 → 结果制图；
2. 同一 `job_config.json` + 同一输入在无头模式重跑，`case/` 包级哈希逐位一致
   （同平台；跨平台按 B7 证据口径）；
3. 任意一处 fatal 或未确认 review 均使导出按钮禁用且 CLI exit 非零；
4. 每个产物可追溯到输入哈希、配置哈希、确认文件哈希与精确复现命令；
5. GoldenSuite 覆盖：G30/G32（网格）、C4/B6/C2/C3/M288 各自 golden 均 `ci_gate:true`
   且三处 manifest 同步；
6. 平台全程不实现/不配置任何运行时交换、仲裁、账本语义；结果层不做物理量再计算；
7. 作者化 1D 网络（N1/N2）产物模式与哈希进入清单；作者化 `.inp` 必过真实引擎解析；河网水力占位未补齐时导出禁用。

---

## 10. 明确不做（继承 v1.1 §9 / v2 §9，增补）

- 不做 WebGIS 第二前端（远期可选）；
- 不做通用 GIS 编辑器（几何编辑用 QGIS 原生工具，平台只消费保存后的文件；N 系列只提供白名单元素/属性的草稿层与写出器）；
- 不实现 1D 河道**水力**建模（断面/糙率/边界水力参数/初始条件归 D-Flow FM 原生工具或 provider；几何草绘由 N2 纳入，水力字段 `provider_required` 阻断导出）；
- 不在作者化 SWMM `.inp` 中生成任何产流节（SUBCATCHMENTS/RAINGAGES/INFILTRATION）——城市产流归 Surface2D（M247）；
- 不在 UI 持有 CLI 之外的私有后端协议；
- 不以空间近邻作为耦合关系最终权威；
- 不在授权样本到位前声明真实城市数据支持；
- 不让 M288-A writer 默认开启或进入发布路径，直至取得"关闭态零影响"证据。

---

## 11. 风险与缓解

| 风险 | 缓解 |
|---|---|
| gmsh 跨平台浮点差异 | B7 先出证据，门禁形态二选一；此前所有确定性 golden 同平台口径 |
| DPM 规则表参数依赖 D-5 批准 | B5 格式与校验先行 + 合成规则表 golden；D 解锁后只灌值 |
| 人工确认与候选漂移导致"确认失效" | C4 哈希级一致性校验；漂移即 review，导出禁用 |
| QGIS 3.40/4.x API 漂移、拷贝部署、宿主环境污染 | `qgisMaximumVersion` 显式；QgsSettings 持久化仓库根；`jobio.subprocess_env()` 清洗 `PYTHONHOME/PYTHONPATH`；离屏冒烟脚本进仓库 |
| M288-A 触碰 solver/driver 主干 | 默认关闭 + 关闭态逐位零影响证据 + 独立 failure-revealing 评审 |
| roof/dflowfm 接线跨 CouplingLib 里程碑 | preproc 侧只定消费格式；交换语义变更走 CouplingLib 自己的 golden 线 |
| 并行会话 M/G 编号冲突 | 编号落地当刻取最小空闲号；本文件编号一律视为占位 |
| 参数表/规则表被就地改写破坏溯源 | UI 编辑只产生新版本文件；manifest 记录所用版本哈希 |

---

## 12. 开放决策（需项目方拍板；未决前按默认值执行）

| 决策 | 选项 | 默认 |
|---|---|---|
| soil 参数载体 | 保留 `soil_parameters.csv` / 新增 `soil_lut.json` / 二者并存 | 保留 CSV 为输入，B5 派生报告为 JSON；不双契约 |
| 控制要素触碰边界/建筑 | 拒绝（现状）/ 自动分割嵌入 | 拒绝；分割需单独确定性证据 |
| 时序输出格式 | CF/UGRID NetCDF / 自定义二进制 | NetCDF（与 STCF 同栈） |
| 结果栅格化 | GeoTIFF（GDAL）/ 仅 NetCDF | 两者，GeoTIFF 可选 |
| 跨平台门禁口径 | 逐位 / 1e-12 容差 | 由 B7 证据决定 |

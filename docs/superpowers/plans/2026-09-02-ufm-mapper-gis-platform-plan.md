# UFM-Mapper：RAS Mapper 式现代化 GIS 全流程数据加工与处理平台总体规划（M287 计划 v2 扩展）

日期：2026-09-02
状态：PLANNED（规划文档；不改变 M281 D-5 BLOCKED 决策；不修改任何已激活 gate）
关系：本文件是 `docs/superpowers/plans/2026-08-26-m287-gis-preproc-workbench-design.md`（v1.1）
的扩展修订，吸收《类似HEC-RAS 中 RAS Mapper的现代化GIS全流程数据加工与处理平台.md》
（下称"参考蓝图"）的能力清单，并以 2026-09-02 已落地的 M287-A/B/C/E1 证据为基线。
凡与 v1.1 治理约束（§2）冲突之处，以 v1.1 与主 Spec / 稳定性协议 / 符号表为准。

## 1. 定位与总体理念

UFM-Mapper 是 SCAU-UFM 的"RAS Mapper 等价物"：**所见即所得的空间交互 +
无头（headless）自动化数据加工 + 100% 可复现的标准化输出**。三条已被
M287-B/E1 实证的原则继续作为铁律：

1. **CLI 为本，UI 为壳**：QGIS 插件只做"渲染用户选择 → `job_config.json` →
   子进程 CLI → 重读报告刷新图层"的无状态管道；UI 不持有任何计算状态。
2. **校验只有一份**：拓扑 / schema / 数值校验保持 C++ 权威实现
   （`libs/mesh`、`libs/stcf`、`scau_preproc validate`），Python 与 UI 不复制。
3. **Fail-Closed**：缺件、越界、退化几何、超时一律阻断并产出可定位的
   诊断产物（findings + 诊断 GeoJSON）；禁止不完整网格、禁止静默修复、
   禁止静默降级。

范围扩展（相对 v1.1）：从"前处理工作台"扩展为"全流程"——在既有
数据接入 → 网格 → 字段 → 耦合 → 导出之外，新增 **运行结果制图层
（Results Mapper，M288 线）**，对应 RAS Mapper 的 Results 树。前处理侧
边界不变：仍只产出数据与映射文件，运行时耦合语义（`Q_limit`、`V_limit`、
`mass_deficit_account`、rollback/replay、仲裁）永远归 CouplingLib。

## 2. 现状基线（截至 2026-09-02）

| 阶段 | 内容 | 状态 | 证据 |
|---|---|---|---|
| M287-A | Gmsh 剖分 spike（约束剖分、超时/退化 fail-closed、诊断 GeoJSON） | DONE | `superpowers/specs/2026-08-27-m287a-gmsh-meshgen-spike-evidence.md` |
| M287-B | 流水线 v1（阶段 A-D CLI 化，合成包 → STCF 案例；G30 `ci_gate:true`） | DONE | `superpowers/specs/2026-08-27-m287b-preproc-pipeline-evidence.md` |
| M287-C | 三链路耦合映射生成器 + SimDriver 消费（G31 `ci_gate:true`） | DONE | `superpowers/specs/2026-08-28-m287c-coupling-maps-evidence.md` |
| M287-D | 真实数据绑定 | BLOCKED（M281：授权样本 + 格式契约） | `superpowers/specs/2026-08-18-m281-city-data-import-blocked-decision.md` |
| M287-E | QGIS 插件（E1 作业页 DONE；E3 网格工作台页 DONE 2026-09-02） | IN PROGRESS | `superpowers/specs/2026-08-30-m287e1-qgis-plugin-evidence.md`、`superpowers/specs/2026-09-02-m287-b4-e3-mesh-controls-evidence.md` |
| M287-B4 | 网格控制（加密线/加密区/约束线 → gmsh embed + size fields；G32 `ci_gate:true`） | DONE 2026-09-02 | `superpowers/specs/2026-09-02-m287-b4-e3-mesh-controls-evidence.md` |
| M287-F | 全耦合真实案例 + 发布门禁 | 未启动 | — |

已有资产（新切片必须复用，不得重写）：

- `python/scau_preproc/{meshgen,pipeline,coupling_maps}.py` — 流水线 v1 与映射生成器；
- `qgis_plugin/scau_preproc_workbench/` — 无状态插件壳（`jobio.py` 无 qgis 依赖）；
- `samples/d5_gis_preproc_template/` — 合成规范输入包（唯一开发/测试数据源，直至 M281 解锁）；
- `apps/preproc_cli`（`scau_preproc generate/validate`）、`libs/stcf`（schema v5 +
  UGRID 拓扑 + import_contract）、`libs/mesh`（build_mesh + quality）、
  `libs/surface2d/stcf_bridge`（加载自检）；
- G21/G22/G30/G31 golden 与 manifest 检查器。

已知遗留（本规划的直接输入）：

- DPM（`phi_t`/`Phi_c`/`omega_edge`/`phi_e_n`）与土壤字段仍是记录在案的
  **占位规则**（G30 断言 unit placeholders）；
- 双平台（windows-msvc vs linux-gcc）哈希对比实验 OPEN（缺受治理 Linux gmsh 环境）；
- dflowfm 链路按 fail-closed 输出 `provider_required`（零关系）；
- roof 链路映射 JSON 已生成，但 SimDriver run_loop 尚无 roof 配置键，未接线；
- `exchange_width` / `priority_weight` 为记录在案的占位值；
- 无 CRS 自动重投影（v1 为 rejection-only 契约校验）；
- 无 DEM 填洼/水系强制；UI 无耦合编辑器（加密线/加密区与质量热力图已于 2026-09-02 落地）；
- 无任何运行结果制图能力。

## 3. 与参考蓝图的差距裁决

参考蓝图的能力项逐条对照现状，归属到里程碑；**凡参考蓝图与已落地契约
冲突处，一律以落地契约为准**（避免双契约）：

| # | 参考蓝图能力 | 现状 | 归属 |
|---|---|---|---|
| 1 | 数据树 / 图层目录 + 状态灯（Pass/Review/Fatal） | E1 仅单作业页 | E2 |
| 2 | CRS 强制重投影为米制直角坐标、拒绝 Lat/Lon | 契约校验 rejection-only，无自动重投影 | B2 |
| 3 | DEM Null 采样与高程提取（z_b） | DONE（G30 锁定 DEM 采样域） | — |
| 4 | DEM 填洼（fill）与水系强制 | 无 | B3 |
| 5 | 建筑 hole 提纯 + 自相交/退化诊断图层 | DONE（rejection-only + 诊断 GeoJSON） | — |
| 6 | 局部加密线/加密区手绘 → 受控剖分 | DONE 2026-09-02（`mesh_controls` 契约 + G32） | — |
| 7 | 正交性/偏斜度热力图渲染 | DONE 2026-09-02（`mesh_quality_cells.geojson` + 分级渲染） | — |
| 8 | 子进程超时拦截、禁止 partial mesh | DONE（policy 化超时 + `partial_mesh_output: forbidden`） | — |
| 9 | manning_n 从 landcover LUT 派生 | DONE（G30 锁定 landcover Manning） | — |
| 10 | Green-Ampt 土壤参数派生 + `theta_i < theta_s <= 1` 校验 | stcf 层有校验，派生为占位 | B5 |
| 11 | DPM 障碍物张量派生（`phi_t`、`Phi_c` 等） | 占位（规则表须项目方批准，v1.1 §4 红线） | B5（格式先行）+ D |
| 12 | SWMM 节点落网格硬卡口（孔洞内 fatal） | DONE（G31 用 C++ 复核 containment） | — |
| 13 | roof ↔ 雨水口候选（距离 + 置信度 + 人工确认） | DONE（`needs_confirmation`）；SimDriver 未消费 | C2 |
| 14 | 河网 ↔ 地表边界映射 | `provider_required` 占位 | C3 |
| 15 | 耦合关系可视化编辑器（高亮连线、低置信度标红、确认/拒绝） | 无 | E4 |
| 16 | 门禁与导出中心（fatal/未确认时导出禁用；一键 Run Pipeline） | E1 有门禁标签与运行按钮；无报告浏览器/完整包导出 | E5 + B6 |
| 17 | `case_manifest.yaml`（哈希 + 可复现命令） | **已由 `pipeline_manifest.json` 承载**；不新增 YAML 双契约 | — |
| 18 | `*_mapping.csv` | **已由 `*_mapping.json`（schema v1）+ `simdriver_links.conf`（version=2）承载** | — |
| 19 | WebGIS 前端 | 不做（见 §9；无状态 CLI 管道使远期加壳成本可控） | 远期可选 |
| 20 | Results 制图（RAS Mapper Results 树：淹没深度/范围/时序/动画） | 完全缺失 | M288 |

## 4. 系统架构（沿用 v1.1 §3，增补第四层）

```text
┌─ 前端（QGIS 插件，Python，GPL 壳）────────────────────────────┐
│ E1 作业页 │ E2 数据目录树 │ E3 网格工作台 │ E4 耦合编辑器      │
│ E5 门禁与导出中心 │ M288-C 结果制图页（时间轴/深度渲染）       │
└──────────────┬────────────────────────────────────────────────┘
               │ 无状态管道：job_config.json → 子进程 CLI → 重读 JSON 报告
┌──────────────▼────────────────────────────────────────────────┐
│ 后端编排（python/scau_preproc + apps/preproc_cli）             │
│ A 数据接入契约 │ B 地形/几何 │ C 受控剖分(gmsh 子进程)         │
│ D 字段派生 │ E 耦合映射 │ F 校验+原子导出                      │
└──────────────┬────────────────────────────────────────────────┘
               │ C++ 权威校验（唯一一份）：libs/mesh + libs/stcf + stcf_bridge
┌──────────────▼────────────────────────────────────────────────┐
│ 标准案例包 case/ （§7 文件契约）                                │
└──────────────┬────────────────────────────────────────────────┘
               │ apps/sim_driver 运行（RuntimeConfig v2 消费 simdriver_links.conf）
┌──────────────▼────────────────────────────────────────────────┐
│ 结果制图层（M288，新增）：受治理时序输出契约 → scau_results CLI │
│ → 淹没深度/范围/到达时间栅格 + 点时序 → QGIS 渲染               │
└────────────────────────────────────────────────────────────────┘
```

不变式（全部沿用 v1.1，此处重申适用于新层）：

- 许可证边界：QGIS（GPL）与 gmsh（GPL）都只经"子进程 + 文件"接触，
  模型内核与后端不与二者链接；
- 结果制图层**只读**运行产物，不做任何物理量再计算或"修正"；派生制图
  （如最大深度包络）必须确定性并携带来源哈希；
- 机器可读字段名一律遵循符号表；`phi_t`/`Phi_c`/`phi_e_n`/`omega_edge`
  保持分立字段。

## 5. 全流程流水线（阶段视图 × 现状 × 人工确认边界）

| 阶段 | 自动完成 | 必须人工确认 | 现状 / 缺口 |
|---|---|---|---|
| A 数据接入 | 契约校验（授权/CRS/单位/字段映射/几何策略）、fatal/review/pass 分级 | 授权、垂直基准、无效几何处置 | v1 DONE；B2 增加受控重投影 |
| B 地形与几何 | 边界裁剪、DEM 采样 → z_b、建筑 hole 提纯、叠加检测 | 建筑阻塞/开口语义、**DEM 填洼与水系强制方案** | 采样 DONE；填洼/水系强制 = B3 |
| C 网格剖分 | 约束剖分、拓扑构建、质量报告、超时/退化 fail-closed | 尺寸/加密策略、质量 review 项处置 | v1 DONE；加密线/区 + 热力图 DONE（B4/E3，G32） |
| D 字段派生 | z_b、manning_n、soil → Green-Ampt LUT、规则表 → DPM 场 | 参数表/规则表本身、未映射对象处置 | manning DONE；soil/DPM 真实派生 = B5 |
| E 耦合映射 | 三链路候选（显式 ID > 空间包含 > 近邻，带距离/置信度） | 所有低置信度、一对多/多对一、交换高程 | DONE；roof 消费 = C2；dflowfm = C3；确认回写 = E4 |
| F 校验与导出 | C++ 权威校验 → 原子写出 + 哈希清单 + 可复现命令 | 存在 fatal 或未确认 review 时禁止导出 | 校验/manifest DONE；完整 case 包组装器 = B6 |
| G 运行与结果 | SimDriver 运行、时序输出、淹没制图产物 | 结果解读、发布判定（走稳定性协议 gate） | 全部缺失 = M288 |

## 6. 里程碑分解

推进纪律沿用 v1.1 §8：每切片按"设计 → failure-revealing 候选 → 实现 →
专属 GoldenTest → manifest/CI"推进；UI 切片必须晚于其消费的后端契约定版；
M 编号/G 编号在落地当刻取当时最小空闲号（避免并行会话冲突，下列编号为
规划占位）。

### 6.1 后端加工线（B 系列切片）

| 切片 | 内容 | 出口标准 |
|---|---|---|
| M287-B2 CRS 治理重投影 | pyproj 受控重投影到项目米制 CRS；变换管道字符串（`proj pipeline`）显式版本化进 `job_config.json` 与 manifest；地理坐标输出一律 fatal；重投影前后坐标审计样本进报告 | 重投影确定性 golden 候选（同平台逐位）；lat/lon 输入被拒的负向证据 |
| M287-B3 DEM 调理 | 填洼（Priority-Flood 或等效，算法与参数版本化）与可选水系强制；**对 DEM 的一切修改视同"修复"**：须 `geometry_clean_policy.json` 同级的 `terrain_condition_policy.json` 显式授权，输出洼地/改动量诊断栅格图层供 QGIS 复查，未授权即 fatal | 调理前后差异审计报告 + 诊断图层；关闭策略时流水线行为逐位不变 |
| M287-B4 网格控制升级 | 加密折线/加密多边形/内部保留约束线（breaklines）→ gmsh size fields；尺寸函数参数入 `job_config.json`；退化约束仍走既有 fail-closed + 诊断 GeoJSON | **DONE 2026-09-02**：`mesh_controls` GeoJSON 契约 v1、逐位确定性、约束线网格边链断言（Python + G32 C++ 独立复核）、零控制路径与 G30 逐位一致 |
| M287-B5 物理字段真实派生 | soil → Green-Ampt LUT 填充（复用 stcf 层 `theta` 校验）；建筑/障碍 → DPM 规则表驱动派生（`phi_t`、`Phi_c` 张量、`omega_edge`、`phi_e_n`）。**规则表格式与校验先行**（合成规则表驱动开发与 golden）；真实参数值属 M287-D 契约范围，工具不得自行推断 | 替换 G30 占位域断言为规则表派生值断言；`validate_dpm_consistency`（closure_laws）全过；未映射对象 fail-closed |
| M287-B6 案例包一键导出器 | 组装 §7 的 `case/` 目录：先全量校验、后原子 rename；`swmm/model.inp` 与 `dflowfm/*` 原样复制（不篡改）；包级 SHA-256 清单 + 可复现 CLI 命令写入 `pipeline_manifest.json` | 导出包被 `scau_preproc validate` + SimDriver 冷启动直接消费；重复导出逐位一致 |
| M287-B7 双平台确定性收口 | 受治理 Linux gmsh 环境（进入 third_party 治理）；执行 v1.1 遗留的跨平台哈希实验 | 以证据决定跨平台门禁形态："逐位一致" 或 "记录在案的 1e-12 级容差"（v1.1 §5 原文约定） |

### 6.2 耦合映射线（C 系列切片）

| 切片 | 内容 | 出口标准 |
|---|---|---|
| M287-C2 roof 链 SimDriver 接线 | run_loop 增加 roof 配置键，消费 `roof_drain_mapping.json` 确认结果 → RoofSwmmStepDriver（既有 M247-D/PR#35 座架）；preproc 侧只固化消费格式，不碰交换语义 | golden：合成包 roof 映射被真实 SimDriver 解析并驱动 mock/real SWMM 多子步；roof 水量守恒断言复用 M247 闭合框架 |
| M287-C3 D-Flow FM 边界链落地 | 依赖 M273/M276 已证边界契约（governed boundary ID/方向/积分）；`surface_dflowfm_mapping.json` 从 `provider_required` 升级为真实候选；河道中心线仍不得单独推断边界类型/方向/ID（v1.1 §5 红线） | golden：合成单河段案例端到端映射 + SimDriver river link 消费；无 provider 证据时保持 `provider_required` 的负向测试 |
| M287-C4 确认文件契约 | 人工确认结果固化为**独立版本化输入文件**（`coupling/confirmed/*.json`，含确认者、时间、原候选哈希），生成器输出保持只读；重跑流水线时确认与候选做哈希级一致性校验，候选漂移即 review | schema + 漂移检测 golden；E4 编辑器写入的正是该契约 |

### 6.3 QGIS 插件线（E 系列切片，逐页对齐 v1.1 §7 的 8 页）

| 切片 | 内容 | 依赖 |
|---|---|---|
| M287-E2 数据目录树页 | RAS Mapper 式 Terrain / Land Cover / Geometries / 1D Networks 分组树；逐层 CRS/单位/完整性状态灯（Pass/Review/Fatal），点击 finding 定位到地图对象 | 现有 `validation.json` findings（已含对象 ID） |
| M287-E3 网格工作台页 | 加密线/加密区手绘 → 写入 `job_config.json`；剖分后渲染 `mesh_quality.json` 正交性/偏斜度热力图；肇事约束的诊断 GeoJSON 一键加载 | **DONE 2026-09-02**（草稿层 → 契约 GeoJSON → 预检 → 热力图；QGIS 4.0.0 离屏冒烟） |
| M287-E4 耦合关系编辑器 | 地图上高亮 1D 节点 ↔ 2D 单元连线；`confidence: review` 标红；确认/拒绝/改目标写入 M287-C4 确认契约文件；不修改生成器输出 | C4 |
| M287-E5 门禁与导出中心 | 全量报告浏览器（import/topology/field/coupling/reproducibility 分页）；存在 fatal 或未确认 review 时导出按钮禁用（复用 `jobio` 门禁函数）；一键调 B6 导出器 | B6 |
| M287-E6 打包收口 | 图标、翻译、Plugin Reloader 工作流说明、QGIS 3.40/4.x 双轨冒烟清单 | E2-E5 |

UI 纪律（全切片强制）：新增页一律经 `jobio.py` 纯 Python 层（无 qgis
import，可 headless 测试）；插件壳零逻辑；仓库根目录走 QgsSettings 持久化 +
fail-closed 校验（PR #90 既定模式，勿用 `__file__` 推断）。

### 6.4 真实数据线（不变）

M287-D 保持 BLOCKED，入口条件仍为 M281 解锁（授权样本 + 数据所有方格式
契约）。解锁后顺序不变：importer spike → 拓扑/字段验证 → STCF 绑定 →
真实小样 golden。B5 的规则表**格式**先行使得 D 到位时只需灌入批准参数值。

### 6.5 结果制图线（M288，新范围）

| 切片 | 内容 | 出口标准 |
|---|---|---|
| M288-A 运行输出契约 | SimDriver/Surface2D 增加受治理时序输出 writer：CF/UGRID 时序 NetCDF（h、eta、单元流速、干湿状态），写出频率与变量集在 RuntimeConfig 显式声明；默认关闭，开启不得影响任何既有 gate 的数值与性能证据 | schema 校验 + 写读 roundtrip golden；关闭时逐位零影响证据 |
| M288-B `scau_results` CLI | 最大淹没深度/范围包络、洪水到达时间、指定点/断面时序抽取、（可选）GeoTIFF 栅格化；全部确定性并在输出中携带运行 manifest 哈希 | 合成案例端到端 golden：同一运行两次后处理逐位一致 |
| M288-C QGIS 结果页 | 时间轴播放、深度/流速渲染、点击取时序曲线；与 SWMM `.out` / D-Flow FM map 输出的联动查看（只读展示，不换算） | 无状态管道合规（结果文件 → jobio → 渲染）；操作者可视核查清单 |

M288-A 触碰 solver/driver 主干，须单独的 failure-revealing 设计评审，且在
`GoldenSuite` 全绿 + 关闭态零影响证据之前不得默认开启。

### 6.6 收口（M287-F，内容不变、输入更新）

三模型全耦合案例包（B6 导出 + C2/C3 接线 + D 真实数据）+ 全系统质量审计
（复用 M270/G24 whole-system mass audit）+ M288 结果制图冒烟 +
GoldenSuite/CI 发布门禁对齐（稳定性协议 evidence 状态：`ABORT` /
`REVIEW_REQUIRED` / `BLOCK_MERGE` / `BLOCK_RELEASE` 全链路保留）。

## 7. 文件契约（以落地为准的单一契约清单）

现行契约（已定版，新切片只能扩展不能另起）：

```text
job_config.json                 # UI/批处理 → CLI 的唯一入口（schema v1）
validation.json                 # status + findings（fatal/review/pass，含对象 ID）
pipeline_manifest.json          # 源/配置/案例 SHA-256、质量摘要、可复现命令
mesh/case.stcf.nc               # STCF v5 + CF-UGRID（唯一二维规范输入）
mesh/mesh_quality.json
coupling/surface_swmm_mapping.json    # coupling_mapping_schema_version = 1
coupling/roof_drain_mapping.json
coupling/surface_dflowfm_mapping.json
coupling/mapping_report.json
simdriver_links.conf            # SimDriver RuntimeConfig version = 2 片段
```

规划增量契约（随对应切片定版）：

```text
metadata/terrain_condition_policy.json   # B3：DEM 调理授权与参数（与 geometry_clean_policy 同级）
metadata/dpm_rule_table.json             # B5：批准制规则表（格式先行，值随 D-5）
mesh_controls.geojson                    # B4：已定版（schema v1；job_config.mesh_controls.geojson 引用）
mesh_quality_cells.geojson               # B4/E3：已定版（逐单元显示诊断，非认证）
coupling/confirmed/*.json                # C4：人工确认结果（确认者/时间/候选哈希）
case/ 包清单（pipeline_manifest.json 扩展包级哈希）   # B6
results/*.nc + results/*.json            # M288-A/B：时序输出与制图产物 schema
```

裁决记录：参考蓝图中的 `case_manifest.yaml` 与 `*_mapping.csv` 不采纳——
对应能力已由 `pipeline_manifest.json` 与映射 JSON/conf 承载，禁止双契约。

## 8. 门禁与 GoldenSuite 规划

- 已激活：G30 `preproc_gis_synthetic_case`、G31 `coupling_maps_synthetic_case`、
  G32 `preproc_mesh_controls_case`（B4，2026-09-02）（均 `ci_gate:true`）。
- 候选（编号落地时申领，此处按内容列）：重投影确定性（B2）、DEM 调理审计
  （B3）、规则表派生 DPM/soil（B5，
  同时收紧 G30 占位断言）、case 包导出 roundtrip + SimDriver 冷启动（B6）、
  roof SimDriver 消费（C2）、dflowfm 链端到端（C3）、确认契约漂移检测
  （C4）、结果输出 roundtrip 与关闭态零影响（M288-A/B）。
- 提升纪律沿用既有三绿规则与 manifest 三处同步（goldensuite.json +
  check_manifest.py REQUIRED + 未加引号的 `LABELS golden`）。
- B7 双平台实验出证据前，所有确定性 golden 保持同平台口径。

## 9. 明确不做（沿用 v1.1 §9，增补三条）

- 不在前处理与结果制图中实现或配置任何运行时交换/仲裁/账本语义；
- 不自动修复几何或 DEM 后静默继续（一切修改过 policy 授权 + review）；
- 不以空间近邻作为耦合关系最终权威；
- 不在授权样本到位前实现真实格式解析器或声明真实城市数据支持；
- 不让 UI 持有 CLI 之外的私有后端协议；
- **新增**：不在结果层对物理量做再计算/插值"修正"（只读展示与确定性派生制图）；
- **新增**：不启动 WebGIS 第二前端（远期可选；无状态 CLI 管道保证未来加壳
  不需要改动后端契约）；
- **新增**：M288-A 输出 writer 默认关闭，未取得"关闭态零影响"证据前不得
  默认开启或进入发布路径。

## 10. 风险与缓解

| 风险 | 缓解 |
|---|---|
| gmsh 跨平台浮点差异使双平台哈希不一致 | B7 实验先行，门禁形态按证据二选一（v1.1 既定） |
| DPM 规则表参数值依赖数据所有方批准（D-5） | B5 格式与校验先行 + 合成规则表 golden，D 解锁后只灌值 |
| QGIS 4.x API 漂移 / 插件部署为拷贝 | 沿用 PR #89/#90 经验：`qgisMaximumVersion` 显式声明、仓库根 QgsSettings 持久化 + fail-closed、`jobio` 纯层 headless 测试兜底 |
| M288-A 触碰 solver 主干引入回归 | 默认关闭 + 关闭态逐位零影响证据 + 独立 failure-revealing 评审，才允许进入任何 gate |
| roof/dflowfm 接线跨 CouplingLib 里程碑 | preproc 侧只定消费格式；交换语义变更走 CouplingLib 自己的 golden 线（G14/G17 族） |
| 并行会话 M/G 编号冲突 | 编号落地当刻取最小空闲号；本文件编号一律视为占位 |

## 11. 推荐推进顺序（默认串行主线，允许标注的并行）

1. **M287-E3 + B4**（网格工作台：加密控制 + 热力图）——**DONE 2026-09-02**
   （G32；见 `superpowers/specs/2026-09-02-m287-b4-e3-mesh-controls-evidence.md`）；
2. **M287-C4 + E4**（确认契约 + 耦合编辑器）——打通"候选 → 人工确认 →
   固化"闭环，是导出门禁语义完整的前提；
3. **M287-B5**（真实字段派生格式先行）与 **B2/B3**（CRS/DEM 调理）可与
   2 并行；
4. **M287-B6 + E5**（导出器 + 门禁中心）收口前处理全流程；
5. **M287-C2/C3** 按 CouplingLib 侧就绪度插入；
6. **M288-A → B → C** 在前处理主线收口后启动（或在 B6 后并行启动 M288-A
   契约设计评审）;
7. **M287-D** 独立等待 M281 解锁；**M287-F** 最后收口。

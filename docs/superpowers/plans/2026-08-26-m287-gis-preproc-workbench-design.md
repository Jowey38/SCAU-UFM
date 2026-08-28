# M287：GIS 数据前处理工作台（PreProc Workbench）总体设计与分阶段实施规划

日期：2026-08-26（v1.1，2026-08-26 采纳《前处理计划优化建议》四项补强）
状态：PLANNED（设计规划；不改变 M281 D-5 BLOCKED 决策；真实城市数据绑定仍以授权样本 + 格式契约为前提）

## 1. 目标

建设一个"GIS 前处理工作台 + 可复现后端流水线"，使操作者在提供 SWMM 管网数据、
城市建筑物轮廓、DEM、计算边界（可选：土地覆盖、土壤、河网）后，能够：

1. 自动完成格式/坐标系/单位/字段/拓扑检查与清洗裁剪；
2. 自动剖分三角/四边形混合非结构网格（建筑物轮廓与计算边界作为约束）；
3. 自动派生地表/屋面/土壤/糙率/DPM 字段并写出 STCF v5 / CF-UGRID 案例；
4. 自动生成屋面→SWMM、地面→SWMM、2D↔D-Flow FM 的候选耦合关系，
   由人工确认歧义项后固化为版本化映射文件；
5. 一键导出模型全部输入文件与质量/溯源报告。

产品定位：**QGIS 负责空间交互，PreProc 后端负责可复现转换，STCF/UGRID 负责
二维规范输入，CouplingLib 负责运行时耦合语义。** 界面永远不是唯一入口：
CLI 与批处理全程可用，UI 只是同一后端的驱动器。

## 2. 治理约束（先于一切设计）

- M281 D-5 保持 BLOCKED：真实城市数据 importer 的实现与任何"支持真实城市
  数据"的声明，必须等授权样本 + 数据所有方格式契约到位。本规划中所有
  真实数据绑定阶段（M287-D 起）以此为入口条件。
- 在授权样本到位前，工作台的开发、测试与 golden 全部使用项目自著数据：
  `samples/d5_gis_preproc_template/`（合成模板包）与 `scau_preproc` authored
  profiles（G21/G22 既有模式）。
- 边界不变式不放松：
  - 前处理只产出数据与映射文件，禁止承载 `Q_limit`、`V_limit`、
    `mass_deficit_account`、rollback/replay、仲裁语义（CouplingLib 专属）；
  - `Surface2DCore` 不依赖 1D 引擎 ABI；SWMM 与 D-Flow FM 原生对象互不可见；
  - 机器可读字段名一律遵循符号表（`phi_t`/`Phi_c`/`phi_e_n`/`omega_edge`
    等分立字段不得合并）。
- 第三方网格生成器（如 Gmsh）按既有第三方治理进入 `extern/` +
  `third_party/manifest|licenses|patches`，先做 spike 评估再静态嵌入。

## 3. 总体架构

```text
┌─ 前端（QGIS 插件，Python）───────────────────────────────┐
│ 项目向导 / 数据目录 / 字段映射 / 网格设置 / 参数表        │
│ 耦合关系编辑器 / 质量门禁 / 导出与复现                    │
└──────────────┬───────────────────────────────────────────┘
               │ 调用（子进程 CLI + JSON 报告，无私有协议）
┌──────────────▼───────────────────────────────────────────┐
│ 后端编排 PreProcOrchestrator（apps/preproc_cli 扩展）     │
│  SourceCatalog        数据清单与授权/溯源                  │
│  CrsUnitNormalizer    CRS/单位/垂直基准归一                │
│  GeometryValidator    几何有效性（只报告，修复须确认）     │
│  TerrainProcessor     DEM 裁剪/采样 → z_b                  │
│  MeshGenerator        受治理网格剖分（边界+建筑约束+加密） │
│  SemanticClassifier   地表语义分类（roof/road/ground/...） │
│  SurfaceFieldBuilder  manning_n/soil/DPM 字段派生          │
│  SwmmLinkBuilder      屋面/地面 → SWMM 候选映射            │
│  DflowFmLinkBuilder   2D 边界 → D-Flow FM 候选映射         │
│  CaseValidator        复用 libs/mesh + libs/stcf 校验      │
│  ArtifactWriter       STCF/UGRID + 映射 + 报告原子写出     │
│  ProvenanceReport     哈希/参数版本/可复现命令             │
└──────────────┬───────────────────────────────────────────┘
        复用既有权威组件（不重写）：
        libs/mesh（build_mesh 拓扑 fail-closed + quality 报告）
        libs/stcf（schema v5 校验、UGRID 拓扑校验、NetCDF 读写、
                   import_contract 授权/CRS/单位/字段映射前置校验）
        libs/surface2d/stcf_bridge（加载自检）
```

语言分层：GIS 解析、空间叠加、网格编排、UI 用 Python
（`python/scau_preproc/`，目录设计已预留；底层依赖 GDAL/pyproj/shapely 等
MIT/BSD/LGPL 无头库）；拓扑/schema/数值校验保持 C++ 权威实现，Python 经
CLI 或薄绑定调用，**校验只有一份，不在 Python 复制**。

许可证边界：QGIS 为 GPL-2.0+。插件与后端之间只经"子进程 CLI + JSON 文件"
交互，后端（含 C++ 内核）不与 QGIS 源码链接，避免 GPL 传染进入模型内核。

UI↔CLI 交互为**无状态管道**：插件把用户勾选/确认渲染为版本化
`job_config.json` → `subprocess` 调 `scau_preproc --config job_config.json`
→ 读取 CLI 输出的 `validation.json` 等报告刷新地图图层。UI 内部不保留
任何计算状态、不缓存中间结果，消除 UI 与后端状态不一致的隐患；
`job_config.json`/`validation.json` schema 属于后端契约，随 M287-B 定版。

## 4. 输入数据契约

以 `samples/d5_gis_preproc_template/` 为规范输入包结构（清单、CRS/单位、
字段字典、映射规则、质量要求、SWMM/河网映射表、验收清单）。要点：

- 每个数据集必须过 `validate_external_import_contract`：授权标记、
  source_uri、source/target CRS、水平/垂直单位、字段映射（目标限定为
  canonical 14 字段，不重复，单位齐全）；
- 稳定 ID 强制：`building_id`/`inlet_id`/`swmm_node_id`/`river_boundary_id`
  等来自源数据，禁止以最近邻结果充当长期 ID；
- 建筑物→DPM 物理参数（`phi_t`、`Phi_c` 张量、`omega_edge`、`phi_e_n`）
  必须由项目方批准的映射规则表驱动，工具不得自行推断；
- **几何清洗容差显式版本化**：输入包必须携带
  `metadata/geometry_clean_policy.json`（简化容差、snap 捕捉距离、sliver
  面积阈值、自相交/重叠处置等全部显式声明并版本化）。GeometryValidator
  禁止使用任何隐含默认容差；每一次拓扑修复都必须在报告中记录对象 ID、
  修复类型、修复前后的原始坐标，未声明策略的修复一律 fatal。

## 5. 流水线阶段与自动/人工边界

| 阶段 | 自动完成 | 必须人工确认 |
|---|---|---|
| A 数据接入 | 读取、契约校验、范围/CRS/单位/几何检查，fatal/review/pass 分级 | 授权、垂直基准、无效几何修复方案 |
| B 几何处理 | 边界裁剪、DEM 采样、叠加检测 | 建筑阻塞/开口语义、DEM 填洼与水系强制 |
| C 网格剖分 | 约束剖分、局部加密、拓扑构建、质量报告 | 尺寸/加密策略、质量 review 项处置 |
| D 字段派生 | z_b、manning_n、soil_type、按规则表生成 DPM 场 | 参数表本身、未映射对象处置 |
| E 耦合映射 | 候选关系（属性 ID > 空间包含 > 近邻，带距离/高差/置信度） | 所有低置信度关系、一对多/多对一、交换高程 |
| F 导出 | STCF/UGRID 写出（先验证后原子 rename）、映射文件、全部报告 | 存在 fatal 或未确认 review 时禁止导出 |

耦合候选规则优先级（三条链路同构）：
显式属性关联 → 空间包含 → 距离阈值内近邻（仅作候选，必须人工确认）。
河道中心线不得单独推断 D-Flow FM 边界类型/方向/ID。

**网格剖分 fail-closed 隔离**（阶段 C）：Gmsh 以受控子进程运行，强制
超时截断（默认 60 s，policy 可版本化覆盖）与内存上限。真实城市多孔约束
多边形易触发退化几何死循环——剖分失败或超时即抛
`MeshGenerationFailed`，同时导出包含肇事边界/约束 ID 的诊断 GeoJSON
供 QGIS 定位复查；**禁止输出不完整网格**，禁止静默降级为无约束剖分。

**浮点确定性纪律**（阶段 F 写出）：`z_b`、节点坐标与 DPM 张量一律 IEEE 754
双精度、禁扩展精度中间量（MSVC `/fp:precise` 与既有 M284 纪律一致）；
ArtifactWriter 写出后立即重读并逐位复验（写-读 roundtrip bitwise）。
同平台同源数据重跑生成的 `case.stcf.nc` 必须哈希一致（M287-B golden）。
跨平台（windows-msvc vs linux-gcc）哈希一致是**待证目标而非假设**：
GDAL/pyproj 的坐标变换与浮点解析在不同构建下可能有 ULP 级差异，
M287-B 须以双平台 CI 复现实验出证据后，才决定跨平台门禁按
"逐位一致"还是"记录在案的 1e-12 级容差"固化。

## 6. 输出案例包

```text
case/
├── case_manifest.yaml            # 版本、哈希、参数、可复现命令
├── mesh/case.stcf.nc             # STCF v5 + UGRID（唯一二维规范输入）
├── mesh/mesh_quality.json
├── coupling/{surface_swmm,roof_drain,surface_dflowfm}_mapping.json
├── swmm/model.inp                # 原样保留，不篡改
├── dflowfm/{model.mdu,.ext,.bc}  # 原样保留（河网范围内时）
├── forcing/...
└── validation/{import,topology,field,coupling,reproducibility}.json
```

## 7. 界面（QGIS 插件，8 页）

1 项目向导（CRS/基准/数据源/完整性）→ 2 数据目录（逐层状态）→
3 字段映射（目标字段只能从 canonical 集合选择）→ 4 网格设置（预览+质量
热力图）→ 5 语义与物理参数表 → 6 耦合关系编辑器（地图+表格，候选
确认/拒绝/修改）→ 7 质量门禁（fatal/review/pass，点击定位）→
8 导出与复现（有 fatal 或未确认项时导出按钮禁用）。

## 8. 分阶段里程碑（每阶段按"设计→failure-revealing 候选→实现→专属
GoldenTest→manifest/CI"推进）

| 阶段 | 内容 | 入口条件 | 出口标准 |
|---|---|---|---|
| M287-A | 网格剖分 spike：Gmsh（或同等）治理评估，边界+建筑约束剖分 → MeshTopology，合成包驱动 | 无（合成数据） | 剖分结果过 `validate_stcf_case` + quality 报告；第三方治理文件齐备；**超时/退化几何 fail-closed 证据（含故意构造的死锁夹具 + 诊断 GeoJSON 输出）** — **DONE 2026-08-27**，见 `superpowers/specs/2026-08-27-m287a-gmsh-meshgen-spike-evidence.md` |
| M287-B | PreProc 流水线 v1：A-D 阶段 CLI 化，合成包 → 完整 STCF 案例 | M287-A | 新 golden：`preproc_gis_synthetic_case`（同平台确定性重生成逐位比较 + 写读 roundtrip bitwise）；**geometry_clean_policy 驱动的修复审计报告**；双平台哈希对比实验出证据，定跨平台门禁形态 — **DONE 2026-08-27（双平台实验仍 OPEN，待受治理 Linux gmsh 环境）**，见 `superpowers/specs/2026-08-27-m287b-preproc-pipeline-evidence.md` |
| M287-C | 耦合映射生成器：三条链路候选+确认文件格式+映射报告 | M287-B | 新 golden：映射文件 schema 校验 + 合成包端到端；映射文件被 SimDriver 消费通过 — **DONE 2026-08-28**（屋面链路的 SimDriver 接线属后续里程碑），见 `superpowers/specs/2026-08-28-m287c-coupling-maps-evidence.md` |
| M287-D | 真实数据绑定 | **M281 解锁（授权样本+格式契约）** | importer spike → 拓扑/字段验证 → STCF 绑定 → 真实小样 golden（M281 既定顺序） |
| M287-E | QGIS 插件 UI | M287-B 后端契约稳定 | 8 页可用；UI 仅调 CLI；无后端旁路 |
| M287-F | 全耦合真实案例 + 发布门禁 | M287-C/D/E | 三模型全耦合案例包 + 全系统质量审计 + GoldenSuite/CI |

关键排序决策：**UI（M287-E）晚于后端契约稳定**，避免界面绑定未定型格式；
**真实数据（M287-D）与其余阶段解耦并行等待**，授权样本何时到位不阻塞
工作台本体开发。

## 9. 明确不做

- 不在前处理中实现或配置任何运行时交换/仲裁/账本语义；
- 不自动修复几何后静默继续（一切修复过 review）；
- 不以空间近邻作为耦合关系最终权威；
- 不在授权样本到位前实现真实格式解析器或声明真实城市数据支持；
- 不让 UI 持有 CLI 之外的私有后端协议。

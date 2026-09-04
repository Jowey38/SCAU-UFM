# UFM-Mapper 1D 网络作者化专项规划（N 系列：SWMM 管网作者化 + D-Flow FM 河网草绘）

日期：2026-09-03
状态：PLANNED（v3 总规划 `2026-09-02-ufm-mapper-platform-master-plan.md` 的范围扩展；
不改变 M281 D-5 BLOCKED 决策；不修改任何已激活 gate）
决策来源：项目方 2026-09-03 选定"方案 2（SWMM 管网作者化模块）+ 方案 3（河网草绘）"。
本文件是 v3 §8 路线图新增的 **N 系列**，v3 §10 中"不实现 1D 河道几何"一条据此修订
为"不实现河道**水力**建模（断面/糙率/边界水力参数归 D-Flow FM 原生工具），几何草绘纳入"。

---

## 0. 一页摘要

在"数据接入 → 网格 → 字段 → 耦合 → 导出"主线之外，为平台增加 **1D 网络的绘制与编辑**：

- **N1 管网作者化**：在 QGIS 中绘制/编辑检查井、排放口、管道（含断面），或把既有
  `model.inp` 导入为图层编辑，由受治理写出器生成 `swmm/model.inp`；写出后**回读 +
  真实 SWMM 5.2 引擎解析**双重校验；节点可标注"地表雨水口/屋面接入"，自动产生耦合
  候选进入 C4 确认流程。
- **N2 河网草绘**：在 QGIS 中草绘河网节点/河段/地表交界候选线，导出 D-Flow FM
  1D UGRID 网络文件（复用已有干净室生成器）与带 `provider_required` 标记的 MDU
  模板；**边界类型/方向/ID、断面、糙率、初始条件一律不推断**（v1.1 §5 红线），
  作为显式待填项阻断导出门禁，直到 provider/确认文件补齐。

两者都遵守三条铁律（CLI 为本、校验一份、Fail-Closed），并新增第四条：
**作者化产物永远与"外部提供"产物分模式互斥、清单记录模式与哈希，绝不覆盖外部原件。**

---

## 1. 范围与边界

### 1.1 纳入

| 能力 | N1 管网 | N2 河网 |
|---|---|---|
| 图层绘制/编辑（点/线 + 属性） | 检查井 junction、排放口 outfall、管道 conduit（+ 断面 xsection 属性） | 河网节点 river_node、河段 river_branch、地表交界候选 surface_interface_candidate |
| 既有文件导入为图层 | `model.inp` → 图层（round-trip） | 既有 `*_net.nc`（UGRID 1D）→ 图层（只读导入，第二期可编辑） |
| 拓扑辅助 | 管道端点吸附节点自动填 from/to；长度按几何自动或手填；孤立节点/悬空管段检测 | 河段端点吸附节点；分叉/汇流节点度数检测；自相交/重叠检测 |
| 受治理写出 | `swmm/model.inp`（限定节集合）| `dflowfm/<case>_net.nc` + `<case>.mdu` 模板 + `authoring_manifest.json` |
| 写出后校验 | 回读 `.inp` 图等价 + 真实 `SwmmEngine` open/parse 干跑 | UGRID 拓扑回读 + （自托管 lane）真实 DLL `initialize` 干跑 |
| 与耦合链集成 | 作者化节点走 **C5 空间候选模式**（v3 §8.2）：单元包含 → 候选、屋面按质心近邻，全部 `confidence: review`；`coupling_role` 只用于过滤/着色，不改变候选语义 | `surface_interface_candidate` → `surface_dflowfm_mapping.json` 候选（`boundary_type/direction/ID = provider_required`） |

### 1.2 不纳入（红线）

- **不做水力参数推断**：SWMM 的降雨/子汇水（`[SUBCATCHMENTS]`/`[RAINGAGES]`/
  `[INFILTRATION]`）**禁止出现**在作者化 `.inp` 中——城市产流在 Surface2D 内（M247
  决策），屋面/地表进入 SWMM 只经 CouplingLib 节点侧向入流；D-Flow FM 的断面、
  糙率、初始水位、边界水力参数不生成数值，只留显式占位。
- **不推断河道边界类型/方向/ID**（v1.1 §5）；交界线只是几何候选。
- **不做 SWMM 全节集合**：一期只写 `TITLE/OPTIONS/JUNCTIONS/OUTFALLS/CONDUITS/
  XSECTIONS/COORDINATES/REPORT`（与 D-5 样例一致；SWMM 5.2 无 `[END]` 节，样例中的 `[END]` 已于 B6 冷启动证据中被真实引擎拒绝并删除）；`STORAGE/ORIFICES/WEIRS/
  PUMPS/CONTROLS/CURVES/TIMESERIES` 为第二期显式白名单扩展，未白名单的节出现即 fatal。
- **不声明真实城市管网/河网支持**（M281 未解锁前全部为合成/作者化数据）。
- **不做 SWMM 或 D-Flow FM 的运行控制界面**（运行归 SimDriver）。

---

## 2. 契约

### 2.1 N1：`drainage_network.geojson`（`drainage_network_schema_version = 1`）

```jsonc
{
  "type": "FeatureCollection",
  "drainage_network_schema_version": 1,
  "units": {"length": "m", "elevation": "m"},          // 强制米制；OPTIONS 写 FLOW_UNITS CMS, LINK_OFFSETS ELEVATION 或 DEPTH（显式声明）
  "options": {                                          // 只允许白名单键，全部显式
    "flow_units": "CMS", "flow_routing": "DYNWAVE", "link_offsets": "DEPTH",
    "start_datetime": "...", "end_datetime": "...", "routing_step_s": 5, "report_step_s": 60,
    "min_surfarea_m2": 1.167, "allow_ponding": false
  },
  "features": [
    {"type": "Feature", "geometry": {"type": "Point", ...},
     "properties": {"element": "junction", "node_id": "J1", "invert_elevation_m": 8.0,
                    "max_depth_m": 2.0, "init_depth_m": 0.0, "surcharge_depth_m": 0.0, "ponded_area_m2": 0.0,
                    "coupling_role": "surface_inlet" | "roof_drain" | "none",
                    "exchange_elevation_m": 10.0}},                     // 仅 coupling_role != none 时必填
    {"type": "Feature", "geometry": {"type": "Point", ...},
     "properties": {"element": "outfall", "node_id": "O1", "invert_elevation_m": 7.0,
                    "outfall_type": "FREE" | "NORMAL" | "FIXED", "fixed_stage_m": null,
                    "river_interface_candidate": true}},                 // 供 SWMM↔river 引擎接口候选（M240/M278 语义归 CouplingLib）
    {"type": "Feature", "geometry": {"type": "LineString", ...},
     "properties": {"element": "conduit", "link_id": "C1", "from_node": "J1", "to_node": "O1",
                    "length_m": null,                                   // null = 按几何计算；显式值须与几何长度差 < 容差否则 review
                    "roughness_manning": 0.013, "in_offset_m": 0.0, "out_offset_m": 0.0,
                    "xsection": {"shape": "CIRCULAR", "geom1_m": 0.6, "geom2": 0, "geom3": 0, "geom4": 0, "barrels": 1}}}
  ]
}
```

Fail-closed 规则：重复 ID；conduit 端点不在节点吸附容差内；from/to 与几何方向不一致；
孤立节点；无排放口；`max_depth <= 0`；断面形状不在白名单；出现任何非白名单节/键；
坐标不在计算边界内（若提供了边界）；`coupling_role != none` 而缺 `exchange_elevation_m`。

### 2.2 N1 产物

```text
swmm/model.inp                       # 写出器产物（authored 模式）；external 模式下为原样复制
swmm/model.inp.authoring.json        # {mode: authored|external, source_geojson_sha256, inp_sha256,
                                     #  writer_version, sections_written[], engine_parse: {engine: "swmm 5.2.4", ok, node_count, link_count}}
swmm/node_surface_mapping.csv        # 候选（authored 模式自动生成；confidence=review, review_status=needs_confirmation）
swmm/roof_drain_mapping.csv          # 候选（需 buildings 有 roof_drain 意图；同上）
```

模式互斥：`job_config.drainage_network.mode ∈ {external, authored}`；authored 模式下若
输入包已有 `swmm/model.inp`，原件保存为 `swmm/model.source.inp`（不删除、不覆盖），清单
两份哈希都记录。

### 2.3 N2：`river_sketch.geojson`（`river_sketch_schema_version = 1`）

```jsonc
{
  "type": "FeatureCollection",
  "river_sketch_schema_version": 1,
  "features": [
    {"type": "Feature", "geometry": {"type": "Point", ...},
     "properties": {"element": "river_node", "node_id": "RN_1", "long_name": "..."}},
    {"type": "Feature", "geometry": {"type": "LineString", ...},        // 河段中心线几何（geometry 节点）
     "properties": {"element": "river_branch", "branch_id": "RB_1", "from_node": "RN_1", "to_node": "RN_2",
                    "length_m": null}},                                   // null = 按几何
    {"type": "Feature", "geometry": {"type": "LineString", ...},        // 地表↔河道交界几何候选（沿岸线）
     "properties": {"element": "surface_interface_candidate", "interface_id": "SI_1",
                    "branch_id": "RB_1", "side": "left" | "right"}}
  ]
}
```

Fail-closed：重复 ID；河段端点未吸附节点；节点度数为 0；河段自相交/互相交叉（非节点处）；
交界候选未引用存在的河段；坐标不在边界内（若提供）。

### 2.4 N2 产物

```text
dflowfm/<case>_net.nc                 # UGRID 1D 网络（node/branch/geometry；复用 tools/dflowfm 生成器的干净室写法）
dflowfm/<case>.mdu                    # 模板：几何/文件引用已填；水力键写 TODO_PROVIDER 占位
dflowfm/authoring_manifest.json       # {sketch_sha256, net_sha256, mdu_sha256, generator_version,
                                      #  provider_required: [{file, key, reason}], run_smoke: {dll_initialize_ok|skipped}}
coupling/surface_dflowfm_mapping.json # 候选：geometry_ref=SI_*, boundary_type/direction/dflowfm_boundary_id = provider_required
```

导出门禁：`authoring_manifest.provider_required` 非空 → `case/` 导出**禁用**（fatal 级
finding `RiverHydraulicsProviderRequired`），但允许"几何冒烟运行"产物进入 `sandbox/`
路径而非 `case/`。

### 2.5 `job_config.json` 增量块

```jsonc
"drainage_network": {"mode": "authored", "geojson": "swmm/drainage_network.geojson",
                     "generate_coupling_candidates": true, "engine_parse_check": true},
"river_sketch":     {"geojson": "dflowfm/river_sketch.geojson", "case_name": "river",
                     "dll_smoke": false}                      // 真实 DLL 干跑仅自托管 lane
```

---

## 3. 模块与页面

| 模块 | 位置 | 说明 |
|---|---|---|
| `inp_io.py` | `python/scau_preproc/` | `.inp` 白名单节读写（读 → 图层 GeoJSON；写 ← GeoJSON）；语义等价比较器 |
| `swmm_author.py` | `python/scau_preproc/` | 校验 + 写出 + 回读 + 调用引擎干跑（子进程：`scau_preproc swmm-parse --inp`，C++ 侧用既有 `SwmmEngine` open/close） |
| `dflowfm_author.py` | `python/scau_preproc/` | 从 `tools/dflowfm/generate_single_reach_1d.py` 提炼通用 UGRID 1D 网络写出（任意节点/河段/geometry 点） + MDU 模板 |
| `apps/preproc_cli` 新子命令 | `swmm-parse`、`ugrid1d-validate` | C++ 权威侧：真实 SWMM 解析干跑；UGRID 1D 拓扑校验（唯一一份） |
| QGIS **P10 管网工作台** | `workbench_dialog.py` + `jobio.py` | 三个草稿层（junction/outfall/conduit）；吸附；从 `.inp` 导入；保存 → GeoJSON；写出 + 校验结果行；耦合角色着色 |
| QGIS **P11 河网草绘** | 同上 | 三个草稿层（river_node/river_branch/surface_interface_candidate）；吸附；保存 → GeoJSON；导出 net/mdu；`provider_required` 计数灯 |
| E4 耦合编辑器 | 同上 | "新建链接"归 E4 本体（任意 1D 节点，不限作者化）；候选进入 C4 确认；不绕过确认 |

UI 纪律不变：逻辑先落 `jobio.py` 纯层；页面零计算状态；每页离屏冒烟脚本。

---

## 4. 切片与出口标准（编号为占位，落地取最小空闲号）

| 切片 | 内容 | 依赖 | 出口标准 / Gate 候选 |
|---|---|---|---|
| **N1-A `.inp` 白名单 IO + 语义比较** | `inp_io.py` 读写；D-5 样例 `model.inp` 导入→导出语义等价；非白名单节 fatal | — | 单元测试 + round-trip golden 候选 `swmm_inp_roundtrip`（逐位确定性写出；语义等价） |
| **N1-B 作者化写出器 + 引擎解析** | `swmm_author.py`；`scau_preproc swmm-parse`（真实 SwmmEngine）；`authoring.json`；模式互斥与原件保护 | N1-A | golden `swmm_authored_network_parse`：合成作者化网络 → `.inp` → 真实 SWMM 5.2.4 open 成功且节点/管段数、坐标、底高一致；负向：子汇水节出现 → fatal |
| **N1-C 耦合候选生成** | 作者化节点复用 C5 空间候选模式（无独立实现）；`coupling_role` 作过滤/着色 | N1-B、C5 | 候选全部 `review`；C4 确认后 SimDriver 消费（复用 G31 断言） |
| **N1-D P10 管网工作台页** | 草稿层、吸附、导入、保存、写出、结果行 | N1-B | 离屏冒烟：绘 3 井 1 排放 3 管 → 写出 → 引擎解析 ok → 导入回图层等价 |
| **N2-A 河网草绘契约 + UGRID 写出** | `river_sketch.geojson` 校验；`dflowfm_author.py` 通用化写出 `_net.nc`；`ugrid1d-validate` | — | golden 候选 `dflowfm_sketched_network_roundtrip`：草绘 → net.nc → C++ 回读拓扑一致 + 逐位确定性 |
| **N2-B MDU 模板 + provider_required 门禁** | 模板写出；`authoring_manifest.json`；导出禁用逻辑；（自托管）真实 DLL `initialize` 干跑 | N2-A | 负向：占位未填 → 导出禁用；自托管 lane 干跑 ok（`golden_candidate`，非门禁，G11 模式） |
| **N2-C 交界候选 → 映射候选** | `surface_interface_candidate` → `surface_dflowfm_mapping.json`（`provider_required` 字段显式） | N2-A、C3 | 与 C3 合并：provider/确认文件补齐后升级；无补齐保持 `provider_required` 负向测试 |
| **N2-D P11 河网草绘页** | 草稿层、吸附、保存、导出、占位计数灯 | N2-B | 离屏冒烟 |

### 4.1 排期建议（并入 v3 §8.8 关键路径）

```text
主线：C4 ─► E4 ─► B6 ─► E5 ─► (C2, C3) ─► M287-F
N1：  N1-A ─► N1-B ─► N1-C(复用 C5) ─► N1-D             （N1-A/B 可与 C4/C5/E4 并行；新建链接归 E4 本体）
N2：  N2-A ─► N2-B ─► N2-D；N2-C 等 C3 provider 契约     （N2-A 可与 B2/B3 并行）
B6 导出器须消费 N1/N2 的模式与 provider_required 语义 → N1-B、N2-B 应在 B6 定版前完成契约。
```

---

## 5. 治理修订与影响

| 既有约定 | 修订 |
|---|---|
| v2/v3："`swmm/model.inp` 原样复制（不篡改）" | **external 模式**原样复制不变；**authored 模式**产物由写出器生成并经回读 + 真实引擎解析；原件保留为 `model.source.inp`；两模式互斥、清单记录 |
| v3 §10："不实现 1D 河道几何/断面建模" | 改为"不实现河道**水力**建模（断面/糙率/边界水力参数/初始条件归 D-Flow FM 原生工具或 provider）；河网**几何草绘**纳入 N2" |
| v1.1 §5 红线：中心线不得单独推断边界类型/方向/ID | **保持**；N2 只产几何候选，水力字段 `provider_required` 阻断导出 |
| M247 决策：SWMM 子汇水产流禁用 | **强化为写出器 fatal 规则**：作者化 `.inp` 禁止 `SUBCATCHMENTS/RAINGAGES/INFILTRATION/EVAPORATION` |
| 校验只有一份 | 引擎解析干跑与 UGRID 1D 校验放在 `apps/preproc_cli`（C++）；Python 只做属性/拓扑预检与写出 |
| 许可证边界 | SWMM（公有领域）已静态嵌入；D-Flow FM 只经 DLL/文件；QGIS 仍是子进程 + 文件 |

---

## 6. 风险与缓解

| 风险 | 缓解 |
|---|---|
| 作者化 `.inp` 与真实 SWMM 解析行为差异（单位、offset 语义） | 写出后必过真实引擎干跑；`OPTIONS` 全显式白名单；round-trip 语义比较器 |
| 用户用作者化网络"伪造"真实城市管网 | manifest 记录 `mode: authored` + `synthetic/authored` 标志；D-5 未解锁前导出门禁标注 `not_real_city_data` |
| D-Flow FM 网络文件规范漂移（UGRID 1D 属性） | 复用已被真实 DLL 接受的干净室生成器写法（G11 证据）；自托管 lane 干跑 |
| 河网草绘被误当作可运行水力模型 | `provider_required` 非空即导出禁用；MDU 中占位为显式 `TODO_PROVIDER` 字符串使 DLL 拒绝解析而非静默默认 |
| N 系列膨胀为通用 GIS 编辑器 | 只做白名单元素与属性；几何编辑用 QGIS 原生工具 |
| 与 B6 导出器契约耦合 | N1-B/N2-B 先定契约，B6 消费；顺序写入 §4.1 |

---

## 7. 开放决策（默认值）

| 决策 | 选项 | 默认 |
|---|---|---|
| `.inp` 第二期白名单范围 | STORAGE/ORIFICES/WEIRS/PUMPS/CURVES/TIMESERIES 全部 / 仅 STORAGE+WEIRS | 一期不做；二期按真实案例需要申领 |
| 河网导入编辑（既有 `_net.nc` 可编辑） | 一期只读导入 / 一期可编辑 | 只读导入 |
| 交界候选自动生成（沿河段两侧缓冲）| 手绘 / 自动缓冲 + 人工修 | 手绘（一期） |
| 引擎干跑在 hosted lane 是否门禁 | `golden`（SWMM 已嵌入，可门禁）/ `golden_candidate` | SWMM 干跑门禁；D-Flow DLL 干跑仅自托管非门禁 |

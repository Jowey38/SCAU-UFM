# UFM-Mapper GIS 平台开发进展分析与剩余任务清单

日期：2026-09-09
基准：master `5e53d1a`（PR #101 合并后）
上位规划：`docs/superpowers/plans/2026-09-02-ufm-mapper-platform-master-plan.md`（v3）、
`2026-09-03-ufm-mapper-1d-network-authoring-plan.md`（N 系列）
性质：状态快照与任务清单（非规范文档）；切片编号沿用 v3，落地时取最小空闲 M/G 号。

---

## 一、总体进度

| 维度 | 状态 |
|---|---|
| 门禁 | GoldenSuite **G1–G37**，35 项 `ci_gate:true`；本轮新增 G32–G37 共 6 项，全部为同平台确定性 fixture 模式 |
| 后端切片（B 系列 7 项） | 完成 **4**：B2 CRS 治理、B4 网格控制、B5 字段派生、B6 案例包导出；未做 B3 DEM 调理、B7 双平台确定性；A/B/C 主线 v1 已完成 |
| 耦合切片（C 系列 5 项） | 完成 **3**：C 三链路候选、C4 确认契约、C5 空间候选模式；未做 C2 roof 接线、C3 D-Flow FM 边界链 |
| UI 页面（9 页） | 完成 **6**：P1/P2 数据目录树、P4 网格工作台、P6 耦合编辑器、P7/P8 门禁与导出；未做 P3 字段映射、P5 参数表、P9 结果制图 |
| 新增范围 | N 系列（管网作者化 / 河网草绘）0/8；M288 结果制图 0/3 |
| 外部阻塞 | M287-D 真实数据绑定 BLOCKED（M281 未解锁：授权样本 + 格式契约） |

**结论**：前处理主线"接入 → 网格 → 字段 → 耦合候选 → 人工确认 → 导出案例包"已在合成数据上端到端打通并全部门禁化。剩余工作分三块：补齐前处理边角（B3 / B7 / UI 收口）、把导出包变成"可运行"（驱动双模型模式或 C3）、两条新产品线（N 系列、M288）。

---

## 二、本轮已落地（2026-09-02 → 2026-09-08，PR #91–#101）

| 切片 | 内容 | Gate | 证据 |
|---|---|---|---|
| B4 + E3 | 加密线 / 加密区 → gmsh embed + size fields；约束线以网格边链保留；网格工作台 + 质量热力图 | G32 | `superpowers/specs/2026-09-02-m287-b4-e3-mesh-controls-evidence.md` |
| C4 | 人工确认契约 `coupling/confirmed/*.json`（accept / reject / retarget / create）；阶段 E' 有效链接；候选哈希漂移检测；流水线新增 `review` 终态 | G33 | `2026-09-04-m287-c4-confirmation-contract-evidence.md` |
| C5 | 映射模式 explicit_ids / spatial_candidates / mixed；空间候选全 review、必经确认 | G34 | `2026-09-04-m287-c5-spatial-candidates-evidence.md` |
| E4 | 耦合关系编辑器：状态着色连线层、accept / reject / retarget / create / undo 写 C4 文件 | — | `2026-09-04-m287-e4-coupling-editor-evidence.md` |
| B6 | 案例包导出器：门禁 + 原子组装 + `simdriver/run.conf` + 包级 SHA-256 清单；真实 SWMM 冷启动 | G35 | `2026-09-05-m287-b6-case-export-evidence.md` |
| E2 + E5 | 数据目录树 Pass/Review/Fatal 灯；六页报告浏览器 + finding 定位缩放 | — | `2026-09-06-m287-e2-e5-data-tree-report-browser-evidence.md` |
| B5 | `dpm_rule_table.json`（审批门、闭合律 lint）；spec 5.3 规则 2 边投影与内核 1e-12 一致；soil_zones | G36 | `2026-09-07-m287-b5-field-derivation-evidence.md` |
| B2 | `crs_policy.json`；受治理暂存包；PROJ pipeline 版本化审计；地理坐标 fatal | G37 | `2026-09-07-m287-b2-crs-governance-evidence.md` |

本轮顺带修出的真实缺陷（均已记入 `.wolf/buglog.json`）：

1. 样例 `swmm/model.inp` 含非法 `[END]` 节，真实 SWMM 5.2.4 拒绝（`ERROR 205`）；此前 Python 只解析 `[COORDINATES]` 而未暴露。
2. 样例 landcover 多边形重叠（水塘在草地内），v1 首匹配使 23 个水塘单元静默按草地糙率处理。
3. QGIS 宿主导出的 `PYTHONHOME` 污染 `py -3` 子进程（`SRE module mismatch`）。

---

## 三、剩余开发任务清单（按依赖排序）

### A. 前处理主线收口（不依赖外部条件）

| # | 任务 | 内容要点 | 依赖 | 出口证据 |
|---|---|---|---|---|
| A1 | **B3 DEM 调理** | `terrain_condition_policy.json` 显式授权；Priority-Flood 填洼（算法 / 参数版本化）；可选水系强制；接手栅格重投影（B2 遗留的 `dem_policy`）；洼地 / 改动量诊断栅格供 QGIS 复查；策略关闭时流水线逐位不变 | B2 ✔ | golden：调理前后差异审计；关闭态 SHA 不变 |
| A2 | **E5a 参数表页（P5）** | 浏览 `dpm_rule_table.json` / soil 表；编辑只产生新版本文件，不就地改写 | B5 ✔ | 离屏冒烟；版本化写出 |
| A3 | **P3 字段映射页** | 源字段 → canonical 目标下拉（只允许 canonical 集合） | B5 ✔ | 离屏冒烟 |
| A4 | **E6 / E7 打包收口** | 图标、翻译、Plugin Reloader 说明、QGIS 3.40 / 4.x 双轨冒烟清单、`project.ufm.json` 工程索引 | E2–E5 ✔ | 双版本冒烟清单勾完 |
| A5 | **B7 双平台确定性** | 受治理 Linux gmsh 环境进 `third_party`；跨平台哈希实验 | — | 决定门禁口径：逐位 / 记录在案的 1e-12 容差 |
| A6 | **v3 文档同步** | §2.3 模块清单表 4 行状态过期（B5 / C4 / B6 仍写"待建"）；`validate --case-dir` 备注已由暂存目录方案替代；补一份样例包自检清单 | — | 文档 PR |

### B. 让导出包"可运行"（需项目方拍板）

| # | 任务 | 内容要点 | 依赖 |
|---|---|---|---|
| B1 | **决策：驱动双模型运行模式 vs 等 C3** | B6 发现 `SimDriver::run_simulation` 仅支持三模型；surface + SWMM 包可冷启动但不能运行（G35 以 `EXPECT_THROW` 锁定该限制） | 项目方 |
| B2′ | 若选驱动侧：**SimDriver 双模型 run 模式** | 新增 surface + SWMM 循环（不含河网），复用 M247 / M270 守恒框架；golden 用 G35 导出包端到端运行 | B1 |
| B3′ | **C2 roof 链 SimDriver 接线** | run_loop 增 roof 配置键，消费确认后的 `roof_drain_mapping`；守恒复用 M247 闭合框架 | C4 ✔ |
| B4′ | **C3 D-Flow FM 边界链** | 依赖 M273 / M276 边界契约；`surface_dflowfm_mapping.json` 从 `provider_required` 升级；中心线仍不得单独推断边界类型 / 方向 / ID | provider 证据 |

### C. N 系列：1D 网络作者化（已批准，0/8）

| # | 切片 | 内容 | 依赖 |
|---|---|---|---|
| C1 | N1-A `.inp` 白名单 IO | 读写 + 语义比较；非白名单节 fatal；禁止产流节（M247） | — |
| C2 | N1-B 作者化写出器 | `drainage_network.geojson` → `.inp`；回读 + 真实 SwmmEngine 解析干跑（`scau_preproc swmm-parse`）；external / authored 模式互斥、原件保留 | C1 |
| C3 | N1-C 耦合候选 | 作者化节点复用 C5 空间候选模式 | C2 |
| C4 | N1-D P10 管网工作台页 | 草稿层 / 吸附 / 导入 / 写出 | C2 |
| C5 | N2-A 河网草绘契约 + UGRID 写出 | `river_sketch.geojson`；从 `tools/dflowfm/generate_single_reach_1d.py` 提炼 `dflowfm_author`；`ugrid1d-validate` | — |
| C6 | N2-B MDU 模板 + provider_required 门禁 | 水力字段占位非空即导出禁用；自托管 DLL 干跑（非门禁） | C5 |
| C7 | N2-C 交界候选 → 映射候选 | 并入 C3（B4′） | C5、B4′ |
| C8 | N2-D P11 河网草绘页 | 草稿层 / 吸附 / 导出 / 占位灯 | C6 |

### D. M288 结果制图线（0/3，触碰 solver / driver 主干）

| # | 切片 | 内容 | 前置 |
|---|---|---|---|
| D1 | M288-A 输出契约 | SimDriver / Surface2D 受治理时序 NetCDF writer（h、eta、hu/hv、wet_mask）；**默认关闭**；关闭态逐位零影响证据 | 独立 failure-revealing 设计评审 |
| D2 | M288-B `scau_results` CLI | 最大深度 / 范围包络、到达时间、持续时间、点 / 断面时序、GeoTIFF；确定性 + 来源哈希 | D1 |
| D3 | M288-C P9 结果页 | 时间轴、深度 / 流速渲染、点击取时序；SWMM `.out` / D-Flow map 只读联看 | D2 |

### E. 外部阻塞

| # | 任务 | 阻塞条件 |
|---|---|---|
| E1 | M287-D 真实数据绑定：importer spike → 拓扑 / 字段验证 → STCF 绑定 → 真实小样 golden；灌入批准的 `dpm_rule_table` 参数值 | M281 解锁（授权样本 + 数据所有方格式契约） |
| E2 | M287-F 收口：三模型全耦合案例 + 全系统质量审计（M270 / G24）+ M288 冒烟 + GoldenSuite / CI 发布门禁对齐 | 以上全部 |

---

## 四、技术债与开放决策

1. **导出包可运行性**（B1）——阻塞 M287-F 关键路径；建议驱动侧增加 surface + SWMM 双模型运行模式作为独立切片（需 golden）。
2. **内核 `quadratic_form` 舍入接缝**：`libs/surface2d` 的朴素二次型 `xx·nx² + 2xy·nx·ny + yy·ny²` 在斜边可给出 `1+ε`，与 `libs/stcf` 严格的 `phi_e_n ≤ 1` 冲突（B5 发现）；需决定内核改用单位向量形式，还是校验器容忍 ≤ 4 ulp。
3. **C4 确认随 CRS 变化漂移**：候选哈希含节点坐标，重投影会使既有确认全部作废（符合设计）；B3 若接手栅格重投影需再确认这一行为的操作流程。
4. **样例数据质量**：landcover 重叠、`.inp` 非法节说明合成包此前未被真实引擎 / 几何规则充分校验；建议在 A6 补样例包自检清单。
5. **跨平台口径**：所有确定性 golden 目前为同平台口径，B7 出证据前不得声称跨平台一致。

---

## 五、推荐推进顺序

1. **A1 B3 DEM 调理**——后端主线最后一块，同时收掉 B2 遗留的栅格重投影问题。
2. **B1 决策 → B2′ 驱动双模型模式**——打通"导出 → 运行"，为 M288 铺路。
3. **A2 / A3 / A4 UI 收口**——可与 2 并行。
4. **C1–C4 N1 管网作者化**——N2 等 C3 provider 就绪度。
5. **D1 M288-A 设计评审**——B2′ 之后启动。
6. A5 B7、E1 / E2 按外部条件插入。

关键路径：

```text
B3 ─► (B1 决策) ─► B2′ 双模型 run ─► C2 ─► M288-A ─► M288-B ─► M288-C ─► M287-F
UI 收口 (A2/A3/A4) 与 N1 (C1–C4) 并行；C3 / N2 等 provider；M287-D 等 M281；B7 独立
```

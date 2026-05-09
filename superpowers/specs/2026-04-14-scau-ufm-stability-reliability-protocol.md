# SCAU-UFM v5.3.3 稳定性与可靠性增强协议

**日期**: 2026-04-14 (v5.3.3 updated 2026-04-26)
**适用对象**: SCAU-UFM Phase 1 起全部开发人员与自动化流水线
**配套主 Spec**: `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` (v5.3.3)
**性质**: 强制性工程纪律与数值防御协议

---

## 文档边界规则

本协议不是主架构文档的第二份副本。
主 Spec 定义语义与系统不变式；本协议只定义当这些规则被违反时的执行后果、治理动作与证据要求。
当前权威入口见 `superpowers/INDEX.md`；符号、单位、退役别名与 machine-facing 命名约束见 `superpowers/specs/2026-04-22-symbols-and-terms-reference.md`。
复核报告、修文计划、修文状态核销与历史设计书均为非规范性历史记录，不得作为本协议的替代事实源。
两份文档的边界说明见 `superpowers/specs/2026-04-16-document-boundary-rule-note.md`。

## 0. 协议定位与适用范围

本协议是 SCAU-UFM 的执行宪法，用于定义违规则后的执行后果、运行时升级动作、审计证据要求与人工升级路径。
本协议不重复主 Spec 的架构、数据契约、接口契约与耦合语义正文；相关语义定义仍以 `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` 为准。
适用范围覆盖 Phase 1 起的开发、CI、回归验证、运行值守与发布准入流程。

## 1. 规则级别与执行后果

### 1.1 规则分级体系（A/B/C）

本协议中的规则分为三层：

#### Level A：不可豁免硬规则
违反即视为物理完整性被破坏，不得合并、不得发布、不得宣称结果可信。

#### Level B：Phase 1 强制规则
不是数学底线，但属于工程落地的必备能力。Phase 1 未实现则不得称为具备工业级联合模型能力。

#### Level C：实施优化建议
强烈推荐，但不作为一票否决条件。

**架构师注记：Level A 规则的任何背离，将被视为对模型物理完整性的根本性破坏。**

### 1.2 machine-facing 状态表

状态分为两类：**运行时状态**（模拟过程中产生）与 **CI/构建状态**（开发/测试流程中产生）。二者独立但可联动——运行时 `ERROR` 通常导致 CI 侧 `BLOCK_MERGE`。

### 1.3 规则违规到 machine-facing 状态映射

| 规则触发类型 | machine-facing 状态 | 执行后果 |
|---|---|---|
| merge gate violation | `BLOCK_MERGE` | 禁止合并 |
| release gate violation | `BLOCK_RELEASE` | 禁止发布 |
| human review gate | `REVIEW_REQUIRED` | 必须人工复核，且不得作为发布级结果 |
| runtime fatal | `ABORT` | 立即中止并写出黑匣子 |

**运行时状态（优先级递增）**：

| 状态 | 含义 | 自动动作 |
|---|---|---|
| `PASS` | 所有指标正常 | 无 |
| `WARN` | 非阻断异常 | 记录日志并保留审计痕迹 |
| `REVIEW_REQUIRED` | 需要人工复核 | 输出报告并禁止作为发布级结果 |
| `ERROR` | 当前算例结果失效 | 停止可信性认定并标记结果无效 |
| `ABORT` | 运行必须中止 | 触发中止与黑匣子写出 |

**CI/构建状态**：

| 状态 | 含义 | 自动动作 |
|---|---|---|
| `BLOCK_MERGE` | 禁止合并 | CI fail |
| `BLOCK_RELEASE` | 禁止发布 | release gate fail |

**联动规则**：运行时 `ERROR` 或 `ABORT` 自动触发 CI 侧 `BLOCK_MERGE`；`REVIEW_REQUIRED` 不自动阻断但禁止作为发布级结果。

## 2. 正确性硬闸门

本章定义不可绕过的正确性阻断条件；相关数值语义与判据来源见主 Spec 对应章节。

---

### 2.1 GoldenSuite 与 CI 硬闸门 〔Level A〕

- `GoldenSuite` 与主 Spec 定义的 10 项最小 GoldenTest 的测试语义、判据、容差与确定性正确性路径定义，均以主 Spec 对应章节为准。
- 本协议只定义其执行后果：
  - GoldenSuite 必须挂入 CI 主流水线。
  - 任一 merge / pull request 必须附 GoldenSuite 通过证据。
  - 主 Spec 定义的 10 项物理不变式 GoldenTest 必须作为 GoldenSuite 最小集合；缺少任一脚本、确定性参考路径或可复现实证记录，均按 GoldenSuite 不完整处理。
  - 若主 Spec 定义的 GoldenTest 判据在正确性路径上失败：立即 `BLOCK_MERGE`；当前结果不得进入发布链路。
  - 若 PR 仅提供人工截图、大算例通过记录或非确定性 GPU 性能路径输出，而缺少主 Spec 要求的 GoldenTest 证据：直接 `BLOCK_MERGE`。
  - 若实现无法提供主 Spec 要求的确定性正确性路径证据：直接 `BLOCK_MERGE`；必要时默认禁止开启 `USE_CUDA_SOLVER`。
  - 若三角/四边形混合非结构化网格缺少几何量、边法向、邻接关系、退化单元、非流形边与遍历一致性验证证据，或证据显示 `CellType` / `node_count` / `edge_count` 语义被违反：直接 `BLOCK_MERGE`。
  - CI GPU 基础设施采用混合模式：本地 GPU Runner 承担主线阻断测试，云端 GPU 承担性能/大算例补充验证。
  - Phase 2 release 准入必须附 G11 `dflowfm_river_steady` 与 G12 `dual_engine_shared_cell` 通过证据；缺失任一项：`BLOCK_RELEASE`。Phase 1 release gate 仍为 G1–G10，不受影响。
  - 不允许以“大算例能跑”为由绕过主 Spec 与本协议共同要求的 GoldenSuite gate。

---

## 3. 运行时诊断与升级规则

本章定义运行时计数器、阈值触发、自动升级与人工复核的治理规则；具体诊断指标与事件来源由后续条款约束。

### 3.1 运行时在线诊断协议

运行时必须统计以下计数器：

- `count_velocity_clamp`
- `count_negative_depth_fix`
- `count_flux_cutoff`
- `count_drain_split`
- `count_drag_matrix_degenerate`
- `count_ai_prior_conflict`
- `count_mem_pressure_events`
- `count_engine_unreachable[engine_id]`
- `max_cell_cfl`

#### 3.1.1 触发阈值与处置

`max_cell_cfl` 测量量定义：raw 物理 CFL = `(|u| + sqrt(g h)) × dt / Δx_min`；**未**乘 `CFL_safety`。
`C_rollback = 0.8` 阈值与 raw CFL 比较；与 `CFL_safety = 0.8`（时间步预扣安全系数）语义独立。详细参数定义见主 Spec §11.3。
当 `max_cell_cfl > C_rollback` 在连续 3 个 `dt_couple` 窗口成立时，必须触发 `ABORT`，并进入 `snapshot -> rollback -> reduce dt_couple -> replay` 链路。

| 指标 | 触发条件 | 处置 |
|---|---|---|
| `count_velocity_clamp / total_steps > 1%` | 速度安全阀频繁触发 | 标记 `REVIEW_REQUIRED`，禁止将该算例作为发布级结果 |
| `count_negative_depth_fix > 0`（任一次） | 出现负水深纠正 | 标记 `WARN`，记录日志；不阻断 |
| `count_negative_depth_fix / total_steps > 1e-4` | 负水深纠正频次过高 | 立即标记 `ERROR`，停止将该算例作为可信结果 |
| `count_drag_matrix_degenerate >= 1` | 拖曳张量求逆出现退化 | 标记 `REVIEW_REQUIRED`，回查 `A_w` / `Phi_c` |
| `count_ai_prior_conflict >= 3` in any 10 consecutive `dt_couple` windows | AI 连续撞先验围栏 | 暂停该区域 AI 校核，转入人工复核 |
| `count_mem_pressure_events >= 3` in any 10 consecutive `dt_couple` windows | GPU 显存长期逼近上限 | 禁用非核心 AI 特征场并标记 `REVIEW_REQUIRED` |
| `count_engine_unreachable[engine_id] >= max_retry_count`（默认 3） | 1D 引擎 BMI 不响应 | 自动升级该引擎 → `ISOLATED`；输出 `[ENGINE_UNREACHABLE]`；状态可由 `force_clear_isolation` 显式解除 |
| `max_cell_cfl > C_rollback` for 3 consecutive `dt_couple` windows | 局部虚假波速或孔隙率异常 | 立即 `ABORT` 并执行 `snapshot -> rollback -> reduce dt_couple -> replay` |

### 3.2 报告形式

每次算例运行后必须导出：
- `.json` 诊断报告
- 关键计数器空间热力图（后处理生成）

---

## 4. 耦合风险治理

本章治理 1D-2D 交换中的储量透支、deficit 账本、回放一致性与数据类型漂移风险。

### 4.1 1D-2D 耦合防御协议 〔Level A / B〕

本章只定义耦合语义被违反后的执行后果、触发阈值与证据要求。`Q_limit`、20% split、`mass_deficit_account` 的体积账语义，以及 deficit 偿还优先级与 replay 一致性定义，见主 Spec 对应章节。

术语主权对齐要求：本协议与主 Spec 使用同一 machine-facing 名称 `Q_limit`。`Q_max_safe` 已退役为历史别名，不再是有效 machine-facing 名称；审计、CI、日志与 schema 字段中禁止继续使用 `Q_max_safe`。

#### 4.1.1 `Q_limit` / split 违规处置

- 若实现无法证明所有交换、split 子分支、deficit 偿还与 replay 重算均未突破主 Spec 定义的 `Q_limit`：立即标记 `ERROR`，当前算例结果失效。
- 若 PR / merge 请求未附 `Q_limit` 数据类型检查证据，或证据显示 `0.9 * phi_t * h * A / dt_sub` 路径混入 `float` 中间量：直接 `BLOCK_MERGE`。
- 若大交换量工况触发主 Spec 定义的 split 条件而实现未启用 split：至少标记 `REVIEW_REQUIRED`；若已产生负水深纠正、局部储量透支或守恒审计异常，则升级为 `ERROR`。

### 4.2 `mass_deficit_account` 治理后果

- `mass_deficit_account` 的语义定义见主 Spec；本协议只约束其审计与执行后果。
- 任一 deficit 未进入日志、诊断报告或回放审计链：标记 `REVIEW_REQUIRED`，该算例不得作为发布级结果。
- 若残余 deficit 在连续 `N_writeoff_steps = 3` 个 SWMM 大步后仍未清偿并触发 write-off：记录 `WARN` 与节点 ID，并进入操作员核销流程。
- 若 rollback / replay 后 deficit 账与快照前后不一致，或正确性路径下更新顺序不可重复：立即 `ABORT`，并写出黑匣子证据。
- rollback / replay 一致性的容差定义见主 Spec §11.4 `epsilon_deficit`；正确性路径要求严格为 0（见主 Spec §6.5），仅性能路径下使用容差。
- 超出容差或正确性路径下 `ΔV_deficit ≠ 0`：触发 `ABORT`。

### 4.3 Phase / CI 证据要求

- 必须提供 `Q_limit` 静态类型检查证据。
- 必须提供 deficit 更新、偿还、write-off 与 replay 一致性的审计证据。
- 若启用 D-Flow FM 并列 1D 引擎，必须提供 `DFlowFMAdapter` 请求、裁定、写回、BMI 异常转换、RTC / 水工建筑物状态复核与 `RiverExchangePoint.priority_weight` 仲裁证据。
- 双 1D 引擎共享同一 2D 单元时，必须提供 `V_limit_k` 切分、`Q_limit` 裁定、deficit 账本与 replay 一致性证据。
- 证据缺失或不可复现时：PR 阶段 `BLOCK_MERGE`，发布阶段 `BLOCK_RELEASE`。

---

## 5. 张量 / 拖曳 / AI 输入围栏

本章治理张量合法性、拖曳退化与 AI 输入先验围栏，禁止任何绕过物理约束的输入进入可信结果链路。

### 5.1 张量与拖曳防御协议 〔Level A〕

#### 5.1.1 `Phi_c` 基本合法性违规处置

- `phi_t`、`Phi_c`、`epsilon_phi`、`cond_max` 及其合法区间的语义定义与默认参数，均以主 Spec 对应章节为准。
- 任一 PreProc / 运行时实现若未执行主 Spec 要求的张量合法性校验，或允许非法张量进入 GPU / 正确性路径：立即标记 `ERROR`；PR / release 分别触发 `BLOCK_MERGE` / `BLOCK_RELEASE`。
- 任一提交若缺少张量合法性检查证据、边界样例或参数锁定说明：至少标记 `REVIEW_REQUIRED`；用于主线合并时直接 `BLOCK_MERGE`。

### 5.2 拖曳张量比值上限

方向性拖曳张量满足：
\[
\frac{a_{\perp}}{a_{\parallel}} \le r_{drag,max}
\]
默认：
- `r_drag_max = 100`

超限处理：
- 裁剪到上限
- 记录 WARN

### 5.3 求逆退化协议

若：
\[
\det(I + \Delta t C_{total}) < \epsilon_{det}
\]
其中：
- `epsilon_det = 1e-10`

则禁止走标准 2×2 求逆，必须进入降级分支：
- 默认优先退化为标量等效阻力更新
- 对角近似仅允许作为调试/对照分支

并执行：
- `count_drag_matrix_degenerate += 1`

---

### 5.4 AI 冲突仲裁协议 〔Level A / B〕

主 Spec 定义 AI 主权原则、先验围栏语义、物理区间与连续冲突后的系统不变式；本节只定义违规后的执行后果。

#### 5.4.1 AI 主权违规处置

- 任一实现若允许 AI 覆盖 `phi_t`、`Phi_c`、`omega_edge` 等物理约束，或允许 AI 输出绕过主 Spec 规定的物理区间：立即标记 `ERROR`；PR / release 分别触发 `BLOCK_MERGE` / `BLOCK_RELEASE`。
- 若同一区域在任意 10 个连续 `dt_couple` 窗口内触发 `count_ai_prior_conflict >= 3`：必须暂停该区域 AI 校核能力，并标记 `REVIEW_REQUIRED`。
- 若运行记录未保留冲突计数、clamp 证据、Learning Rate Backoff 记录或人工复核结论：该算例不得作为发布级结果。

---

### 5.5 Alpha 功能隔离协议

#### 5.5.1 `vertical_jet_alpha` 默认值与使用条件

`vertical_jet_alpha` 的默认值（`drain_momentum_model = none`）与使用条件由主 Spec §5.2 与 legacy.8.10.2 定义。本协议仅约束其在测试隔离与门禁中的执行后果。

#### 5.5.2 主线隔离

- 大尺度城市主线算例默认使用主 Spec 定义的默认路径。
- Alpha 功能必须拥有独立测试集，不得污染主线稳定性判断。

---

## 6. 故障恢复与审计治理

本章定义 snapshot、rollback、replay、黑匣子写出与异步检查点 IO 的治理要求，确保恢复链路可审计、可重放、可追责。

### 6.1 检查点 IO 隔离协议 〔Level B〕

#### 6.1.1 双引擎故障隔离与回滚（配合主 Spec Section 8）
- `FaultController` 的状态语义、snapshot 强制覆盖范围与 rollback / replay 行为定义见主 Spec 对应章节；本节只定义触发阈值、强制动作与证据要求。
- `count_engine_unreachable[engine_id]`（v5.3.3 新增；`FaultController` 内部捕获 `EngineError` 时递增）进入运行时诊断与 snapshot 审计范围。
- 当耦合裁定导致局部 `max_cell_cfl > C_rollback` 且该条件在连续 3 个 `dt_couple` 窗口成立时，必须触发 `snapshot -> rollback -> reduce dt_couple -> replay`；若实现未执行该链路，立即 `ABORT`（阈值定义见 3.1.1）。
- 若 replay 失败、1D 引擎未从快照起点重新推进，或黑匣子证据缺失：立即 `ABORT`，并禁止该结果进入发布链路。
- 黑匣子 `.crash` 数据至少必须包含 `exchange_state`、地形梯度、RTC 状态、局部水力坡降与熔断前 5 个时间步上下文；任一缺失均标记 `REVIEW_REQUIRED`。

**显存压力预警与动态降级**：
- 每个 `dt_couple` 周期前检查 GPU 显存占用。
- 当占用 > 85%：触发 `count_mem_pressure_events`，优先保留 Level 1/2 核心资源，释放或停更 Level 3 非核心 AI/诊断资源，并输出 `[MEM_PRESSURE_WARN]`。
- 若显存压力已持续触发且仍无法满足主 Spec 要求的 snapshot / replay 正确性前提：必须减小 `dt_couple` 或中止当前算例，不得以不完整快照继续宣称结果可信。

**Snapshot 分片/关键断面重构降级（Level B）**：
- 分片 snapshot 只能作为显存压力下的二级降级方案；其语义前提与最小覆盖定义见主 Spec。
- 若分片模式无法提供可重复 replay、deficit 账一致性与总质量守恒审计证据：禁止启用，并直接 `ABORT` 或降级到更小 `dt_couple`。

**软启动恢复**：
- `FaultController` 从熔断恢复后的软启动语义见主 Spec。
- 若恢复期间再次失稳：必须重新熔断并延长冷却期；若达到长期隔离条件，则进入人工介入路径。

- 热重启/检查点写入必须在异步 IO 线程中完成。
- SimDriver 只负责将数据拷贝到 pinned host memory 后立即返回主循环。
- IO 线程在后台执行 `tmp -> fsync -> rename`。
- 禁止在主计算线程中直接执行大文件 NetCDF 写入。

#### 6.1.2 Snapshot / Checkpoint Schema 治理后果

- snapshot/checkpoint 文件 `schema_version` 缺失、必填字段缺漏或字段类型漂移：直接 `BLOCK_MERGE`；当前算例不得作为 G10 evidence。
- replay 阶段 `content_hash` 与 snapshot 起点不匹配：立即 `ABORT` 并写出黑匣子；CI 侧联动 `BLOCK_MERGE`。
- `*.snapshot.tmp` 残留（atomic rename 失败痕迹）：标记 `REVIEW_REQUIRED`；必须由操作员核销原因并补交完整成功的 snapshot。
- snapshot schema major 版本变化必须补交 GoldenSuite G10 完整回归与 schema 迁移说明；缺失任一项：`BLOCK_RELEASE`。

---

## 7. 显存与资源退化治理

本章定义显存压力下的资源分级、退化模式准入条件与禁止条件。显存压力事件定义与 snapshot 语义见主 Spec 第 8 章。

### 7.1 资源分级定义

| 级别 | 包含资源 | 显存压力下行为 |
|---|---|---|
| **Level 1（必保留）** | `h, hu, hv` 状态数组、地形 `z_b` 数组、snapshot buffer | 任何情况下不得释放或降精度 |
| **Level 2（优先保留）** | 耦合 exchange buffer、CFL 中间归约量、`mass_deficit_account` | 仅在 Level 1 安全后保留 |
| **Level 3（可释放）** | 非核心 AI 特征场、细粒度诊断 heatmap buffer、离线统计缓冲区 | 显存压力时优先释放或停更 |

### 7.2 显存压力触发与退化流程 〔Level B〕

显存压力的权威触发阈值、动态降级主流程与 snapshot / replay 正确性前提，以 `6.1.1` 为准；本节只保留资源分级视角下的摘要：

1. 每个 `dt_couple` 周期前检查 GPU 显存占用率
2. **占用 > 85%**：触发 `count_mem_pressure_events += 1`，释放或停更 Level 3 资源，输出 `[MEM_PRESSURE_WARN]`
3. 若释放 Level 3 后仍无法满足 `6.1.1` 规定的 snapshot / replay 正确性前提：允许进入分片 snapshot 审核路径，或减小 `dt_couple`
4. 若分片 snapshot 仍不满足 `6.1.1` 的正确性条件：不得继续宣称结果可信，必须中止当前算例或退回更小 `dt_couple`

### 7.3 退化模式禁止条件

- 不得以不完整快照继续宣称结果可信
- 不得在退化模式下跳过 deficit 账本一致性校验
- 不得将分片模式作为常态路径——它只是显存压力下的应急降级
- 退化模式下的算例结果自动携带 `[DEGRADED_MODE]` 标记，不得作为 GoldenTest 基准

## 8. Phase 准入与证据要求

实现优先级补充：
- Kernel 3 性能优化顺序应先尝试模板元编程与 `shfl` 优化，
  再考虑拆分 `K3a/K3b`。这是性能建议，不是物理硬规则。

以下条件全部满足，方可进入 Phase 1 主线开发：

- 必须提供 GoldenSuite 通过证据
  - 证据至少包括：CI 通过记录、10 项物理不变式 GoldenTest 的脚本映射、确定性参考路径说明、失败即阻断的门禁结果
- 必须提供 Q_limit 数据类型检查证据
  - 证据至少包括：`0.9 * phi_t * h * A / dt_sub` 静态类型检查结果，以及边界工况下 CPU/GPU 正确性路径误差不超过 `1e-12` 的运行记录
- 必须提供 snapshot / rollback / replay 正确性证据
  - 证据至少包括：rollback 后 `mass_deficit_account` 与 exchange buffer 同步恢复，以及 replay 路径在相同输入下结果确定性可重复
- 必须提供 MockSwmmEngine 最低能力验证证据
  - 最低能力要求：支持 `get_node_head()`、`set_node_lateral_inflow()`、`step()` 三个基本行为
- 主 Spec 已升级到 v5.3.3
- STCF v5 字段集已锁定
- `validate_dpm_consistency()` 已在 PreProc 每一级接入
- CI GPU 采用混合模式并完成验证：
  - 本地 GPU Runner 可执行 GoldenTest / CUDA 回归
  - 云端 GPU 实例可执行性能基准与大算例验证
  - “完成验证”定义为：本地 GoldenTest 全通过，云端性能基准完成且无 FATAL/ERROR

## 9. 第三方版本与兼容性治理

- 必须显式锁定 `SWMM` 版本来源与版本号
- 必须显式锁定 `D-Flow FM` / `BMI bridge` 版本来源与版本号
- 第三方升级时必须补交回归与一致性证据：
  - GoldenSuite 回归证据
  - 接口兼容性验证证据
  - snapshot / rollback / replay 一致性验证证据

---

## 10. 操作员职责与升级路径

本章定义系统动作触发后的人工接管、责任归属、升级时限与复核闭环要求。

### 10.1 角色定义

| 角色 | 职责范围 |
|---|---|
| **开发负责人** | 负责所提交代码的正确性；修复 CI 失败与 GoldenTest 回归 |
| **架构负责人** | 负责数值/物理不变式的完整性；审核 deficit 核销、参数变更与张量合法性 |
| **值班负责人** | 负责运行时事件的即时响应；判定是否 ABORT、是否启动人工介入 |
| **发布负责人** | 负责 release gate 证据完整性；审核是否满足 BLOCK_RELEASE 解除条件 |

### 10.2 完整职责矩阵

| 情形 | 系统状态 | 系统自动动作 | 人工动作 | 责任角色 | 升级时限 |
|---|---|---|---|---|---|
| GoldenTest 失败 | BLOCK_MERGE | CI fail，阻断合并 | 复核并修复测试或代码 | 开发负责人 | 24h 内修复或回滚 |
| GoldenSuite 全套失败 | BLOCK_RELEASE | release gate fail | 调查根因并修复 | 架构负责人 | 48h 内完成根因分析 |
| `count_negative_depth_fix > 0`（任一次） | WARN | 记录日志；不阻断 | 回查网格/参数/耦合配置 | 开发负责人 | 下次提交前完成 |
| `count_negative_depth_fix / total_steps > 1e-4` | ERROR | 标记结果失效 | 回查网格/参数/耦合配置 | 开发负责人 | 24h 内修复 |
| `count_velocity_clamp / total_steps > 1%` | REVIEW_REQUIRED | 输出报告，禁止发布级使用 | 检查耦合配置与入流边界 | 架构负责人 | 下次提交前完成 |
| `count_drag_matrix_degenerate >= 1` | REVIEW_REQUIRED | 输出报告 | 回查 PreProc 的 `A_w` / `Phi_c` | 架构负责人 | 下次提交前完成 |
| `count_ai_prior_conflict >= 3`（滑动窗口） | REVIEW_REQUIRED | 暂停该区域 AI 校核 | 检查 AI 先验围栏与训练数据 | 架构负责人 | 48h 内完成 |
| `count_mem_pressure_events >= 3`（滑动窗口） | REVIEW_REQUIRED | 禁用 Level 3 资源 | 检查显存预算与网格规模 | 值班负责人 | 当次运行内处理 |
| `Q_limit` 违规 | ERROR → BLOCK_MERGE | CI fail + 标记结果失效 | 修复交换通量路径 | 开发负责人 | 24h 内修复 |
| deficit write-off | WARN | 记录日志与节点 ID | 审核核销合理性 | 架构负责人 | 48h 内完成 |
| replay 失败或证据缺失 | ABORT | 中止运行 + 黑匣子写出 + 禁止发布链路 | 人工介入并补齐 replay/黑匣子证据 | 值班负责人 | 即时响应 |
| rollback 后 deficit 账不一致 | ABORT | 中止运行 + 黑匣子写出 | 调查 snapshot 覆盖范围 | 值班负责人 | 即时响应 |
| FaultController 进入 COOLDOWN | REVIEW_REQUIRED | 保持冷却并限制交换恢复速率 | 核验恢复门槛与冷却计时策略 | 值班负责人 | 当次运行内判定 |
| FaultController 进入 RECOVERING | REVIEW_REQUIRED | 启用软启动并持续监控异常计数 | 复核恢复窗口是否满足退出条件 | 值班负责人 | 当次运行内判定 |
| `count_engine_unreachable[engine_id] >= max_retry_count` | WARN + REVIEW_REQUIRED | 自动升级该引擎 → `ISOLATED` 并输出 `[ENGINE_UNREACHABLE]` | 判断是否需要重启、替换引擎或执行 `force_clear_isolation` | 值班负责人 | 当次运行内判定 |
| 引擎进入 ISOLATED 状态 | WARN + REVIEW_REQUIRED | 冻结该引擎交换口 | 判断是否需要重启、替换引擎或人工解除隔离 | 值班负责人 | 当次运行内判定 |
| D-Flow FM BMI 调用失败或不响应 | WARN + REVIEW_REQUIRED | 通过既有 `EngineReport.healthy=false` 路径进入 `FaultController`；达到阈值后进入 `ISOLATED` | 检查 D-Flow FM 进程、BMI bridge、输入文件与资源占用 | 值班负责人 | 当次运行内判定 |
| D-Flow FM RTC / 水工建筑物返回非法水位、流量或开度 | ERROR 或 REVIEW_REQUIRED | 冻结对应交换口并保留审计证据；若已破坏守恒或导致 replay 失败则升级为 `ABORT` | 复核 RTC 规则、建筑物参数与 `RiverExchangePoint` 映射 | 架构负责人 | 24h 内完成 |
| 双 1D 引擎共享单元仲裁证据缺失 | BLOCK_MERGE / BLOCK_RELEASE | 阻断合并或发布 | 补交 `priority_weight`、`V_limit_k`、`Q_limit`、deficit 与 replay 证据 | 发布负责人 | 发布前完成 |
| AI authority violation（AI 越权覆盖物理约束） | ERROR → BLOCK_MERGE / BLOCK_RELEASE | 标记结果失效并触发门禁阻断 | 回退越权逻辑并补充围栏证据 | 架构负责人 | 24h 内修复 |
| tensor 非法写入 GPU / illegal-to-GPU | ERROR → BLOCK_MERGE / BLOCK_RELEASE | 立即阻断正确性路径并标记失效 | 修复张量合法性校验链路 | 开发负责人 | 24h 内修复 |
| snapshot 时序违规（缺失、越序、覆盖不全） | ABORT | 中止运行 + 黑匣子写出 | 排查 checkpoint 时序与覆盖集合 | 值班负责人 | 即时响应 |
| 退化模式禁令被违反 | BLOCK_RELEASE | release gate fail | 修复退化模式治理并补交证据 | 发布负责人 | 发布前完成 |
| 张量合法性校验缺失 | BLOCK_MERGE | CI fail | 补充校验代码与证据 | 开发负责人 | 24h 内修复 |
| 第三方版本升级 | BLOCK_RELEASE | release gate 暂停 | 补交回归/兼容性/replay 证据 | 发布负责人 | 发布前完成 |
| `max_cell_cfl > C_rollback` 持续 3 窗口（见 3.1.1） | ABORT | 触发 `snapshot -> rollback -> reduce dt_couple -> replay` 并写出黑匣子 | 检查孔隙率配置与网格质量、回放链路与快照覆盖 | 架构负责人 | 即时响应 |
| 黑匣子数据缺失 | REVIEW_REQUIRED | 标记结果不完整 | 修复 crash dump 写出逻辑 | 开发负责人 | 24h 内修复 |

### 10.3 升级规则

- 若责任角色在升级时限内未完成处理，自动升级至架构负责人
- 若架构负责人 48h 内仍未关闭，升级至项目负责人
- 所有 ABORT 事件无时限缓冲，必须即时响应

## 11. 附录：状态码、报告 Schema 与证据清单

### 11.1 运行时状态码定义

| 代码 | 状态 | 类型 | 说明 |
|---|---|---|---|
| 0 | PASS | 运行时 | 所有计数器正常，无异常触发 |
| 1 | WARN | 运行时 | 非阻断异常，记录日志 |
| 2 | REVIEW_REQUIRED | 运行时 | 需人工复核，禁止作为发布级结果 |
| 3 | ERROR | 运行时 | 结果失效，停止可信性认定 |
| 4 | ABORT | 运行时 | 运行中止，写出黑匣子 |
| 10 | BLOCK_MERGE | CI/构建 | 禁止合并，CI 失败 |
| 11 | BLOCK_RELEASE | CI/构建 | 禁止发布，release gate 失败 |

**状态可叠加**：一次运行可能同时触发多个状态码，最终取最严格状态作为总结论。优先级：`ABORT > ERROR > REVIEW_REQUIRED > WARN > PASS`。

### 11.2 运行诊断报告最小 Schema

```json
{
  "version": "1.0",
  "run_id": "string",
  "timestamp": "ISO-8601",
  "status": "PASS",
  "counters": {
    "count_velocity_clamp": 0,
    "count_negative_depth_fix": 0,
    "count_flux_cutoff": 0,
    "count_drain_split": 0,
    "count_drag_matrix_degenerate": 0,
    "count_ai_prior_conflict": 0,
    "count_mem_pressure_events": 0,
    "count_engine_unreachable": {
      "swmm-01": 0,
      "dflowfm-01": 0
    },
    "max_cell_cfl": 0.0,
    "total_steps": 0
  },
  "deficit_audit": {
    "total_deficit_volume_m3": 0.0,
    "writeoff_count": 0,
    "writeoff_node_ids": []
  },
  "degraded_mode": false,
  "golden_test_passed": true,
  "warnings": [],
  "errors": []
}
```

### 11.3 Phase / Release 证据清单模板

| 证据项 | Phase 1 准入 | Release 准入 | 说明 |
|---|---|---|---|
| GoldenSuite CI 通过记录 | 必须 | 必须 | 含确定性参考路径说明 |
| `Q_limit` 数据类型检查 | 必须 | 必须 | 静态类型检查 + CPU/GPU 误差 ≤ 1e-12 |
| snapshot / rollback / replay 正确性 | 必须 | 必须 | deficit 账 + exchange buffer 同步恢复 |
| MockSwmmEngine 最低能力验证 | 必须 | — | `get_node_head` / `set_node_lateral_inflow` / `step` |
| 张量合法性校验证据 | 必须 | 必须 | PreProc 每级接入 `validate_dpm_consistency()` |
| 第三方版本兼容性验证 | — | 必须 | GoldenSuite 回归 + 接口兼容 + replay 一致 |
| 显存预算验证 | — | 必须 | 退化模式下仍满足 Level 1/2 资源保持 |
| 黑匣子完整性验证 | — | 必须 | `.crash` 文件包含全部必需字段 |

---

**协议通则**：
- 本协议优先级高于局部优化偏好。
- 任何违反本协议的实现，不得进入主干。
- 本协议只定义 gate、执行后果、审计证据与 operator 责任；若与主 Spec 的物理语义、接口契约或数值不变式定义发生冲突，以主 Spec 定义为准，并在本协议中同步修正治理条款。

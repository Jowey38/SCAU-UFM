# SCAU-UFM superpowers 权威入口

本索引用于防止修文、复核、状态核销与实施计划被误读为当前规范。除本页列出的权威入口外，`superpowers/specs/` 文档均视为非规范性历史记录。

## 规范性入口

| 入口 | 权限边界 |
|---|---|
| `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` | 主 Spec；定义物理语义、接口契约、系统不变式、耦合核心语义与默认参数。 |
| `superpowers/specs/2026-04-14-scau-ufm-stability-reliability-protocol.md` | 稳定性协议；定义 gate、触发阈值、执行后果、证据要求与 operator 责任。 |
| `superpowers/specs/2026-04-22-symbols-and-terms-reference.md` | 符号与术语表；定义 machine-facing 名称、单位、退役别名与命名约束。 |
| `superpowers/specs/project-layout-design.md` | 目标目录结构设计；定义二维地表内核、CouplingLib、SWMM/D-Flow FM 适配、第三方治理、验证体系与 Phase 演进的目标工程落位。 |

## 执行类规范（条件性权威）

下列 spec 在对应工程动作未完成时为条件性权威，约束当时的 spike / sandbox 退出标准与证据要求；动作完成且证据归档后转为历史记录。在归档前主仓库不得越过其退出标准进入对应阶段实施。

| 入口 | 权限边界 |
|---|---|
| `superpowers/specs/2026-05-08-third-party-spike-design.md` | SWMM 5.2.x 与 D-Flow FM / BMI 两路 1D 引擎可行性 spike；定义假设清单、退出标准、ABI 落差矩阵与失败回退路径。Phase 1 实施 `libs/coupling/drainage/`、Phase 2 实施 `libs/coupling/river/` 的入口前提。 |
| `superpowers/specs/2026-05-08-numerical-sandbox-design.md` | 三角/四边形混合非结构化网格上的 HLLC + DPM well-balanced 数值 sandbox；定义 sympy 派生证明、G1/G2/G3 在最小混合网格上的退出标准与失败回退路径。Phase 1 实施 `libs/surface2d/dpm/` / `riemann/` / `source_terms/` 的入口前提。 |

## 阶段性证据入口（非规范性）

下列文档用于定位近期实施阶段的边界分析、证据汇总与复核结论。它们不提升为规范性入口，不覆盖主 Spec、稳定性协议、符号表、目录设计或条件性权威 spike / sandbox 规范；若与上表冲突，以上表为准。

| 入口 | 证据边界 |
|---|---|
| `superpowers/specs/2026-05-29-m76-drainage-river-adapter-entry-boundary-analysis.md` | Drainage/River 适配层进入边界分析；确认真实 SWMM / D-Flow FM 集成仍受第三方 spike gate 约束，并限定下一步为 mock-only、third-party-free seam。 |
| `superpowers/specs/2026-05-29-m88-mock-adapter-boundary-hardening-rollup.md` | M77-M87 mock-only 适配层边界加固 rollup；汇总 third-party-free、DTO 使用、输入域校验与适配层不得持有核心运行时语义的证据。 |
| `superpowers/specs/2026-05-29-m93-mock-fixture-api-rollup.md` | M89-M92 mock fixture API rollup；汇总 SWMM / D-Flow FM mock fixture 与双 mock 场景证据，仍不构成真实第三方引擎集成证据。 |
| `superpowers/specs/2026-05-29-m94-engine-interface-core-runtime-member-guard-evidence.md` | M94 post-rollup evidence；记录 engine interface 禁止承载核心运行时成员名的直接编译期 guard，延续 m93 后续边界加固。 |
| `superpowers/specs/2026-05-29-m95-dflowfm-accept-lateral-discharge-target-evidence.md` | M95 post-rollup evidence；记录 D-Flow FM mock accept 边界只能写入 `lateral_discharge` 的目标校验，仍不构成真实 D-Flow FM 集成证据。 |
| `superpowers/specs/2026-05-30-m101-shared-cell-mock-coupling-rollup.md` | M96-M101 shared-cell / mock-coupling rollup；定位双 mock 共享单元、仲裁与元数据证据，不构成真实 1D 引擎集成证据。 |
| `superpowers/specs/2026-05-30-m106-health-boundary-rollup.md` | Mock engine health boundary rollup；汇总 mock health 状态边界与第三方无关的健康检查证据。 |
| `superpowers/specs/2026-05-30-m108-health-to-diagnostic-rollup.md` | Health-to-diagnostic rollup；汇总 mock health 到 fault diagnostic 的非执行性诊断证据。 |
| `superpowers/specs/2026-05-30-m110-fault-controller-passive-diagnostic-to-policy-rollup.md` | Fault-controller passive diagnostic-to-policy rollup；汇总被动诊断到 policy 映射证据，仍不执行运行时控制。 |
| `superpowers/specs/2026-05-30-m113-mock-scheduler-passive-observation-rollup.md` | Mock scheduler passive observation rollup；汇总调度器被动观测证据，不构成真实 scheduler 执行证据。 |
| `superpowers/specs/2026-05-30-m116-passive-action-consumption-rollup.md` | Passive action consumption rollup；汇总 operator action 被动消费链路证据。 |
| `superpowers/specs/2026-05-30-m120-passive-state-classification-rollup.md` | Passive state classification rollup；汇总 fault-controller passive state 分类证据。 |
| `superpowers/specs/2026-05-30-m124-passive-transition-stage-rollup.md` | Passive transition stage rollup；汇总 passive transition 阶段证据。 |
| `superpowers/specs/2026-05-30-m128-passive-action-audit-stage-rollup.md` | Passive action audit stage rollup；汇总 action audit 阶段证据。 |
| `superpowers/specs/2026-05-30-m132-passive-action-outcome-stage-rollup.md` | Passive action outcome stage rollup；汇总 action outcome 阶段证据。 |
| `superpowers/specs/2026-05-30-m136-blocked-action-stage-rollup.md` | Blocked action stage rollup；汇总 blocked action 阶段证据。 |
| `superpowers/specs/2026-06-01-m141-scheduler-control-passive-chain-rollup.md` | Scheduler control passive chain rollup；汇总 scheduler control 被动链路证据。 |
| `superpowers/specs/2026-06-01-m145-scheduler-phase-evidence-stage-rollup.md` | Scheduler phase evidence stage rollup；汇总 scheduler phase evidence 阶段证据。 |
| `superpowers/specs/2026-06-01-m149-passive-scheduler-control-validation-stage-rollup.md` | Passive scheduler control validation stage rollup；汇总 scheduler control validation 阶段证据。 |
| `superpowers/specs/2026-06-01-m156-passive-scheduler-control-policy-validation-stage-rollup.md` | Passive scheduler control policy validation stage rollup；汇总 scheduler control policy validation 阶段证据。 |
| `superpowers/specs/2026-06-01-m159-phase-control-executable-boundary-stage-rollup.md` | Phase-control executable boundary stage rollup；汇总 executable boundary 阶段证据。 |
| `superpowers/specs/2026-06-01-m161-executable-boundary-negative-evidence-stage-rollup.md` | Executable boundary negative evidence stage rollup；汇总 executable boundary 负向证据。 |
| `superpowers/specs/2026-06-01-m163-mutating-control-evidence-stage-rollup.md` | Mutating control evidence stage rollup；汇总 mutation control 证据边界。 |
| `superpowers/specs/2026-06-01-m165-behavior-specific-evidence-dto-rollup.md` | Behavior-specific evidence DTO rollup；汇总 behavior-specific evidence DTO 阶段证据。 |
| `superpowers/specs/2026-06-02-m166-passive-behavior-specific-evidence-dto-completion-rollup.md` | Passive behavior-specific evidence DTO completion rollup；汇总 passive behavior-specific evidence DTO 完成证据。 |
| `superpowers/specs/2026-06-02-m173-evidence-consumption-to-runtime-evidence-no-mutation-rollup.md` | Evidence consumption to runtime evidence no-mutation rollup；汇总 evidence consumption 到 runtime evidence 的无 mutation 阶段证据。 |
| `superpowers/specs/2026-06-02-m177-dry-run-stage-rollup.md` | Dry-run stage rollup；汇总 dry-run 阶段证据，仍不等同于真实外部引擎或发布 gate 证据。 |

## 历史记录规则

- 根目录历史 Markdown、会话导出、早期方案评审稿与对话记录只作为非规范性背景材料，不得覆盖上表规范性入口。
- 若历史文档与规范性入口冲突，以本索引列出的规范性入口为准，并优先同步主 Spec、稳定性协议与符号表。
- 涉及 `Q_limit`、`V_limit`、`dt_sub_seconds`、`priority_weight`、`mass_deficit_account`、`epsilon_deficit`、`max_cell_cfl`、`C_rollback`、`CFL_safety` 的 machine-facing 定义，统一以符号表和主 Spec §11 为准。

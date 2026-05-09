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

## 历史记录规则

- 根目录历史 Markdown、会话导出、早期方案评审稿与对话记录只作为非规范性背景材料，不得覆盖上表规范性入口。
- 若历史文档与规范性入口冲突，以本索引列出的规范性入口为准，并优先同步主 Spec、稳定性协议与符号表。
- 涉及 `Q_limit`、`V_limit`、`dt_sub_seconds`、`priority_weight`、`mass_deficit_account`、`epsilon_deficit`、`max_cell_cfl`、`C_rollback`、`CFL_safety` 的 machine-facing 定义，统一以符号表和主 Spec §11 为准。

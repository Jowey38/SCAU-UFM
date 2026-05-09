# SCAU-UFM 符号与术语参考表

**日期**: 2026-04-22  
**适用范围**: SCAU-UFM 主 Spec、稳定性协议、架构收口文档中的 machine-facing 与审计术语对齐  
**统一入口声明（规范性）**: 自本版本起，符号、术语与单位的统一入口为本文件；相关文档出现冲突时，以本表命名与单位约束为准。当前文档体系权威入口见 `superpowers/INDEX.md`，review/status/plan/design 类历史文件不得覆盖本表定义。

| Term | Canonical Name | Meaning | Unit/Dimension | Governing Doc |
|---|---|---|---|---|
| `phi_t` | `phi_t` | 单元总体可通水孔隙率（总体孔隙率），用于将几何体积映射为可用物理储量。 | 无量纲 | `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` |
| `Phi_c` | `Phi_c` | 界面方向导流张量（连通性/方向性），用于界面过流与方向性传导；v5.3.3 / Phase 1 中为 PreProc 生成的静态 STCF 场，主推进默认 `∂Phi_c/∂t = 0`。 | 无量纲张量（2×2） | `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` |
| `phi_{e,n}` / `phi_e_n` | `phi_e_n` | 边法向有效导流标量；由静态 `Phi_c` 与边法向投影得到，用于边通量、well-balanced 配对与 soft-block 判据。 | 无量纲 | `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` |
| `omega_edge` | `omega_edge` | 边连通性/开闭权重；用于 hard-block / soft-block 分类与拓扑连通治理，不得与 `phi_e_n` 混同。 | 无量纲 | `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` |
| `Q_limit` | `Q_limit` | 耦合子步流量硬门上界；由 `V_limit / dt_sub` 定义，是唯一有效 machine-facing 名称。 | `m^3/s` | `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` |
| `V_limit` | `V_limit` | 耦合子步体积硬门上界，定义为 `0.9 * phi_t * h * A`。 | `m^3` | `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` |
| `mass_deficit_account` | `mass_deficit_account` | 未完成交换量的体积账本（deficit 账），用于滚动、清偿、核销与回放一致性审计。 | `m^3` | `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` |
| `M_ref` | `M_ref` | 审计参考总质量；定义为参考状态下所有 `h >= h_wet` 单元的 `sum(phi_t * h * A)`。 | `m^3` | `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` |
| `epsilon_deficit` | `epsilon_deficit` | rollback / replay 性能路径下的 deficit 账容差；正确性路径要求严格为 0。 | `m^3` | `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` |
| `dt_sub` | `dt_sub` | 2D 子步时间步长，用于体积-流量换算与子步更新。 | `s` | `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` |
| `dt_couple` | `dt_couple` | 外层耦合时钟周期（耦合大步），用于 1D-2D 同步调度。 | `s` | `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` |
| `max_cell_cfl` | `max_cell_cfl` | 全域单元 raw 物理 CFL 最大值；按 `(|u| + sqrt(g h)) × dt / Δx_min` 度量，未乘 `CFL_safety`。 | 无量纲 | `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` |
| `C_rollback` | `C_rollback` | rollback 触发阈值；与 raw `max_cell_cfl` 比较，语义独立于 `CFL_safety`。 | 无量纲 | `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` |
| `CFL_safety` | `CFL_safety` | 时间步预扣安全系数；用于时间步选择，不用于缩放 `max_cell_cfl` 诊断量。 | 无量纲 | `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` |
| `rollback` | `rollback` | 从 snapshot 恢复到已知一致状态的回退动作。 | 过程语义 | `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` |
| `snapshot` | `snapshot` | 在耦合周期边界保存的可回放一致状态快照。 | 状态集合 | `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` |
| `replay` | `replay` | 从快照起点按确定性顺序重算的恢复/复核路径。 | 过程语义 | `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` |
| `GoldenTest` | `GoldenTest` | 单项正确性基准测试；用于判定核心数值路径是否保持基线一致。 | 测试工件 | `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` |
| `GoldenSuite` | `GoldenSuite` | 发布/合并门禁级回归测试集合；每个 `GoldenTest ∈ GoldenSuite`。 | 测试集合 | `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` |
| `BLOCK_MERGE` | `BLOCK_MERGE` | CI 阻断状态：禁止合并。 | 状态码 | `superpowers/specs/2026-04-14-scau-ufm-stability-reliability-protocol.md` |
| `BLOCK_RELEASE` | `BLOCK_RELEASE` | 发布阻断状态：禁止发布。 | 状态码 | `superpowers/specs/2026-04-14-scau-ufm-stability-reliability-protocol.md` |
| `REVIEW_REQUIRED` | `REVIEW_REQUIRED` | 需人工复核状态：不自动阻断合并，但不得作为发布级结果。 | 状态码 | `superpowers/specs/2026-04-14-scau-ufm-stability-reliability-protocol.md` |
| `ABORT` | `ABORT` | 运行中止状态：必须停止并写出故障证据。 | 状态码 | `superpowers/specs/2026-04-14-scau-ufm-stability-reliability-protocol.md` |
| `u_hydro_tol` | `u_hydro_tol` | 静水平衡速度容差阈值，用于 hydrostatic 类验证与正确性判据。 | `m/s` | `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` |
| `eta_tol` | `eta_tol` | 水位/自由面容差阈值，用于稳态与守恒相关验证判据。 | `m` | `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` |

## 命名约束（强制）

- `Q_limit` 是耦合硬门流量上界的唯一 machine-facing 名称。
- `Q_max_safe` 为 historical/deprecated alias，仅允许作为迁移历史注记保留，不得出现在 audit/CI/log/schema 字段或机器可读接口中。
- 涉及体积-流量换算时必须显式保留两步表达：`V_limit = 0.9 * phi_t * h * A`，`Q_limit = V_limit / dt_sub`。
- `phi_e_n` 与 `omega_edge` 必须分字段记录：前者是边法向有效导流标量，后者是边连通性/开闭权重；日志、schema 与审计字段不得二者混名。
- `max_cell_cfl` 是 raw 物理 CFL 诊断量，不得乘 `CFL_safety`；`C_rollback` 与 `CFL_safety` 是独立参数。

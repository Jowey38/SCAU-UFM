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

## 城市产流符号（M247）

下表登记 M247 城市产流模块的 machine-facing 名称、单位与代码拼写（代码拼写与 canonical name 不同处在括号注明）。Green-Ampt 参数沿用主 Spec §4.2/§4.3 既有命名，不得另造别名。权威设计入口：`superpowers/specs/2026-06-13-m247-urban-runoff-generation-design.md`。

| Term | Canonical Name | Meaning | Unit/Dimension | Governing Doc |
|---|---|---|---|---|
| `F_inf` | `F_inf` (`cumulative_infiltration`) | Green-Ampt 累计下渗量；驱动下渗能力随累计量衰减。 | `m` | `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` |
| `t_ponding` | `t_ponding` (`ponding_time`) | 积水开始时刻；`-1` 表示未积水 sentinel。 | `s` | `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` |
| `K_s` | `K_s` (`k_s`) | 饱和导水率。 | `m/s` | `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` |
| `psi_f` | `psi_f` | 湿润锋吸力水头；必须严格为正（`psi_f = 0` 退化 Green-Ampt）。 | `m` | `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` |
| `theta_s` | `theta_s` | 饱和含水量；要求 `theta_s > theta_i`、`theta_s <= 1`。 | 无量纲 | `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` |
| `theta_i` | `theta_i` | 初始含水量；要求 `theta_i >= 0`。 | 无量纲 | `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` |
| `pervious_fraction` | `pervious_fraction` | 单元透水地面面积分数。 | 无量纲 | `superpowers/specs/2026-06-13-m247-urban-runoff-generation-design.md` |
| `impervious_fraction` | `impervious_fraction` | 单元不透水地面面积分数。 | 无量纲 | `superpowers/specs/2026-06-13-m247-urban-runoff-generation-design.md` |
| `roof_fraction` | `roof_fraction` | 单元屋面面积分数；三者之和 `<= 1 + epsilon_area`。 | 无量纲 | `superpowers/specs/2026-06-13-m247-urban-runoff-generation-design.md` |
| `initial_abstraction_capacity` | `initial_abstraction_capacity` | 初损/截留容量（深度）。 | `m` | `superpowers/specs/2026-06-13-m247-urban-runoff-generation-design.md` |
| `depression_storage_capacity` | `depression_storage_capacity` | 地面洼蓄容量（深度）。 | `m` | `superpowers/specs/2026-06-13-m247-urban-runoff-generation-design.md` |
| `roof_abstraction_capacity` | `roof_abstraction_capacity` | 屋面初损容量（深度）。 | `m` | `superpowers/specs/2026-06-13-m247-urban-runoff-generation-design.md` |
| `roof_storage_capacity` | `roof_storage_capacity` | 屋面暂存容量（深度）。 | `m` | `superpowers/specs/2026-06-13-m247-urban-runoff-generation-design.md` |
| `roof_drain_capacity` | `roof_drain_capacity` | 屋面雨水斗/立管物理排水能力；非 `Q_limit`，先于耦合门限施加。 | `m^3/s` | `superpowers/specs/2026-06-13-m247-urban-runoff-generation-design.md` |
| `swmm_node_index` | `swmm_node_index` | 屋面直排目标 SWMM 节点的稳定整数索引；`-1` 表示无目标，热路径不得字符串查找。 | 整数索引 | `superpowers/specs/2026-06-13-m247-urban-runoff-generation-design.md` |
| `roof_pending_volume` | `roof_pending_volume` | 屋面已生成但尚未入管网或溢流的暂存体积。 | `m^3` | `superpowers/specs/2026-06-13-m247-urban-runoff-generation-design.md` |
| `roof_to_swmm_requested_volume` | `roof_to_swmm_requested_volume` | 本子步请求注入 SWMM 节点的屋面体积。 | `m^3` | `superpowers/specs/2026-06-13-m247-urban-runoff-generation-design.md` |
| `roof_to_swmm_accepted_volume` | `roof_to_swmm_accepted_volume` | CouplingLib 实际接受注入 SWMM 的屋面体积。 | `m^3` | `superpowers/specs/2026-06-13-m247-urban-runoff-generation-design.md` |
| `roof_to_swmm_rejected_volume` | `roof_to_swmm_rejected_volume` | CouplingLib 拒收的屋面体积；必须回 pending 或溢流，不得作为损失。 | `m^3` | `superpowers/specs/2026-06-13-m247-urban-runoff-generation-design.md` |
| `roof_overflow_to_surface_volume` | `roof_overflow_to_surface_volume` | 屋面链溢流入 2D 地表体积；与 `surface_added_volume` 不相交。 | `m^3` | `superpowers/specs/2026-06-13-m247-urban-runoff-generation-design.md` |
| `surface_added_volume` | `surface_added_volume` | 地面链（透水+不透水）净入地表体积；不含屋面溢流。 | `m^3` | `superpowers/specs/2026-06-13-m247-urban-runoff-generation-design.md` |
| `rejected_fail_closed_volume` | `rejected_fail_closed_volume` | fail-closed 拒绝的非法输入体积；不得作为正常质量损失桶。 | `m^3` | `superpowers/specs/2026-06-13-m247-urban-runoff-generation-design.md` |
| `epsilon_area` | `epsilon_area` | 面积分数和约束容差。 | 无量纲 | `superpowers/specs/2026-06-13-m247-urban-runoff-generation-design.md` |
| `epsilon_mass_abs` | `epsilon_mass_abs` | 产流质量闭合绝对容差。 | `m^3` | `superpowers/specs/2026-06-13-m247-urban-runoff-generation-design.md` |
| `epsilon_mass_rel` | `epsilon_mass_rel` | 产流质量闭合相对容差。 | 无量纲 | `superpowers/specs/2026-06-13-m247-urban-runoff-generation-design.md` |

命名约束补充：`roof_drain_capacity` 是产流侧物理排水能力，**不是** `Q_limit`；`Q_limit` 仍是耦合硬门限流量上界的唯一 machine-facing 名称，产流侧不得引入 `Q_max_safe` 或其他流量门限别名。

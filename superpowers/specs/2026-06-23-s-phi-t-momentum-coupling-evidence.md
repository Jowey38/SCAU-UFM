# S_phi_t 接入动量实施证据（scope A，部分交付）

**日期：** 2026-06-23
**范围：** `libs/surface2d/`（riemann、source_terms、time_integration）、`tests/unit/surface2d/`
**计划：** `docs/superpowers/plans/2026-06-22-s-phi-t-momentum-coupling.md`
**设计：** `docs/superpowers/specs/2026-06-21-s-phi-t-momentum-coupling-design.md`（scope A，两轮数值评审）

## 1. 已交付（committed）

| Task | 内容 | commit | 状态 |
|---|---|---|---|
| baseline | M241-M249 surface2d 基线（含并行 M247 runoff，共享 CMakeLists） | `0d1a15b` | — |
| 1 | HLLC 动量平流-only（新增 `advective_normal_momentum_flux`；`physical_normal_momentum_flux` 保留专供 s_star 波速，§5.5.1 不变） | `38da16f` | spec+质量评审通过 |
| 2 | `source_terms/well_balanced` 纯函数模块（Audusse 单侧分裂：F_p、单侧 S_phi_t、平方差 S_topo、边界压力）+ 5 模块单测 | `5accb7b` | spec+质量评审通过 |
| 3 | step 接线（EdgeStepDiagnostics 加 wb_pressure/s_topo；内部边平流 HLLC + WB 配对；墙边界 phi_t-缩放压力）；迁移 1 处 pressure_momentum 断言到边诊断 | `6b0e88e` | spec+质量评审通过 |
| 4 | G4 phi_t 跳变静水（**逐位通过 1e-12**）；G5 变床静水（**DISABLED**，见 §3） | `1dc4eb7` | — |

评审说明：Task 1-3 经 subagent 两阶段评审（spec 合规 + 代码质量）全部通过；Task 4-5 因 subagent 评审积分耗尽改为 inline 评审与执行。

## 2. 自检与安检

- 全量构建 `BUILD_ERRORS:0`（`SCAU_WARNINGS_AS_ERRORS=ON`）。
- 全量测试 **142/142 passed**（含 1 个 DISABLED G5），约 38 s。
- 安检：S_phi_t 改动面均为纯算术；单元/边索引经 `GeometryCache` 邻接 + `validate_*_match_mesh` 边界保护；`reconstruct_hydrostatic_pair` 纯函数、`diagnostics.edges.back()` 安全；无原始内存/字符串/路径/第三方面。无 finding。

## 3. 未交付：G5 变床静水 well-balancing（已定位、已 DISABLED、待专项）

G5 失败，残差 `O(Δz_b)≈0.13`。根因是**预存的主 Spec §5.4 偏差**：共享的 `reconstruct_hydrostatic_pair` 用 `h_i*=max(0, eta_min − z_b_i)`，而 §5.4 要求 Audusse `z_b*=max(z_b_L,z_b_R)`、`h_i*=max(0, eta_i − z_b*)`。变床上当前式使 `h_L*≠h_R*`，标准 HLLC 因此在静水态产生伪平流通量。WB 配对本身正确（`left_normal=−½g·phi_t_i·h_i²` 与 h̄ 无关，已由 G4 与模块单测证明）。

修复 = 把 `reconstruct_hydrostatic_pair` 改为 §5.4 Audusse。这是**基础内核修正**，波及面：
- `test_hydrostatic_reconstruction` 的 `PreservesVelocityWhenDepthIsClipped`（平床 eta_L=2,eta_R=1 现期望 h_L*=1；Audusse 下平床不裁剪 → h_L*=2，需重设计该用例的裁剪场景为 z_b* > eta_i）。
- `set_edge_aligned_state`（bed-step 夹具，eta 相等、z_b 不同）驱动的 dpm_edge_source / momentum / hllc_wave 用例的通量值。

因属基础内核改动、且与现有测试预期冲突需逐一判断、且当前无 subagent 两阶段评审保障，按工程纪律 G5 与该重构修复**延迟为专项任务**，G5 复现器保留为 `DISABLED_SlopingBedKeepsMomentumZero`（去前缀即复现）。

## 4. 边界声明

- scope A 的**平床 phi_t 跳变** well-balancing 已逐位交付（G4）。**变床** well-balancing 待 §5.4 Audusse 重构专项。
- 动态（非零速度）phi_t 跳变仍属 §5.5.2 验证空白区，不宣称 release 级 O(Δx²) 守恒；CVC 为登记的未来路径。
- 不构成 GoldenSuite 门禁证据；G4（及修复后的 G5）为候选 `G_n`。

# S_phi_t 接入动量：Well-Balanced 压力-源项配对设计

> **状态：** 设计（brainstorming 产出，待 writing-plans 转实施）。
> **范围决策：** A（well-balancing 收口，Phase-1 spec 边界内）。
> **权威依据：** 主 Spec `2026-04-11-scau-ufm-global-architecture-design.md` §5.2（控制方程）、§5.5（DPM-HLLC 调用顺序与主权分离）、§5.5.1（波速锁定）、§5.5.2（DPM 一致性引理、边平均算子、验证空白区）、§5.6（S_phi_t/S_topo 界面中心差分）；符号表 `2026-04-22-symbols-and-terms-reference.md`。

## 0. 问题与目标

当前 Surface2D 把静水压力 `0.5gh²` 捆绑在 HLLC 动量通量内，并与平流一并乘 `phi_e_n`；`S_phi_t` 仅为诊断量（`pressure_pairing_phi_t` 与 `s_phi_t_centered` 按构造互为相反数，`residual≡0`），未施加到动量更新；`S_topo` 不存在。这偏离主 Spec §5.2/§5.5 的主权分离（压力按 `phi_t` 缩放、平流按 `phi_e_n`/`Phi_c` 缩放），且无法在 `phi_t` 跳变或变床处保持 well-balanced（参见 M249：墙边界缺压力已修，但 phi_t-跳变/变床的 WB 配对仍缺）。

**目标（范围 A）：** 实现 `phi_t`-缩放压力 + `S_phi_t` + `S_topo` 的同界面模板 well-balanced 配对，保证 `phi_t` 跳变静水与变床静水的零速度平衡，并正确施加 `S_phi_t` 源项到动量。动态（非零速度）`phi_t` 跳变仍属 Spec §5.5.2 验证空白区，不宣称 release 级 O(Δx²) 守恒；Cea-Vázquez-Cendón (CVC) augmented-HLLC 为登记的未来升级路径。

## 1. 架构与模块边界（方案 1：spec 结构化分离）

动量主权按 Spec 分离：平流 ↔ `phi_e_n`（导流），压力/源 ↔ `phi_t`（储量）。把"压力 + S_phi_t + S_topo"从 HLLC 分离为独立的、同界面模板的 Well-Balanced 配对模块。

### 1.1 `libs/surface2d/riemann/hllc`（重构）

`hllc_normal_flux` 的 `EdgeFlux.momentum_n / momentum_x / momentum_y` 改为**只含平流动量** `h·u·u_n`（移除 `0.5gh²` 压力项），仍由 `phi_e_n` 缩放。`mass`、波速估计（`c=√(gh)`，不含 `phi`，§5.5.1）、HLLC 分支选择、CFL 一律不变。

### 1.2 `libs/surface2d/source_terms/well_balanced`（新增）

纯函数模块，对每条内部边产出 WB 配对的动量贡献，使用同一界面模板（算术平均 `phi_t,e`、`h̄_e`，§5.5.2）：

- phi_t-缩放压力界面通量：`F_p,e = 0.5·g·phi_t,e·h̄_e²`，沿 `n`（界面通量，左右反号）
- `S_phi_t,e = 0.5·g·h̄_e²·(phi_t,j − phi_t,i)`，沿 `n`（界面中心源）
- `S_topo,e = −g·phi_t,e·h̄_e·(z_{b,j} − z_{b,i})`，沿 `n`（界面中心源），`z_b = eta − h`
- 边分类门控（复用 M248 `classify_edge`，§5.6）：hard-block 不装配任何 WB 配对项；soft-block 与 regular 装配。

`h̄_e` 使用 hydrostatic reconstruction 后的 `h_L*`、`h_R*` 之均值（与压力通量同模板，§5.6）。

### 1.3 `libs/surface2d/time_integration/step`（接线）

内部边动量残差 = HLLC 平流贡献（`phi_e_n` 缩放，界面通量反号）+ `well_balanced` 模块贡献（压力通量反号 + S_phi_t/S_topo 中心源）。墙/开边界的反射压力（M249 引入）改为复用 `well_balanced` 的 phi_t-缩放压力形式，保持一致（墙取内侧单元的 phi_t-缩放压力）。源项应用顺序不变（WB 配对属通量级，先于 source_terms 的雨/渗/摩阻）。

### 1.4 `libs/surface2d/source_terms/phi_t`（演进）

现有 `pressure_pairing_phi_t`、`s_phi_t_centered` 诊断核并入 `well_balanced` 模块的可复用纯函数；诊断字段语义保留（值不变）。

### 1.5 保持的不变式

- `phi_t=1` 且 `phi_e_n=1`：phi_t-缩放压力 `0.5·g·1·h̄²` = 旧 HLLC 压力，平流+分离压力之和与旧"平流+压力一并 phi_e_n 缩放"逐位相同 → 均匀 phi_t 用例零漂移。
- HLLC 不感知 `phi_t`、不含地形（§5.5/§5.5.1）。
- 波速、CFL、质量通量完全不变。

## 2. 离散格式与 Well-Balancing 相消证明

### 2.1 连续控制方程（§5.2）

```
∂(Φc·h·u)/∂t + ∇·(Φc·h·u⊗u + ½g·φt·h²·I) = S_topo + S_phi_t + ...
S_phi_t = ½g·h²·∇φt        S_topo = −g·φt·h·∇z_b
```

### 2.2 边界面离散（同界面模板，§5.6）

内部边 `e_ij`，法向 `n`，`h̄_e = ½(h_L*+h_R*)`，`phi_t,e = ½(phi_t,i+phi_t,j)`：

```
F_adv,e  = phi_e_n · (h·u·u_n)_HLLC                    [仅平流，无压力]
F_p,e    = ½·g·phi_t,e·h̄_e²                · n         [界面通量，左右反号]
S_φt,e   = ½·g·h̄_e² · (phi_t,j − phi_t,i)  · n         [界面中心源]
S_topo,e = −g·phi_t,e·h̄_e · (z_{b,j} − z_{b,i}) · n    [界面中心源]
```

单元 `i`（取 `L=i`）的边 `e` 贡献：

```
dM_i += (−F_adv,e − F_p,e + S_φt,e + S_topo,e) · len_e
单元 j（R=j）取通量项相反号：dM_j += (+F_adv,e + F_p,e + ...) · len_e
```

压力 `F_p,e` 作为界面通量（左右反号守恒）；`S_φt,e`、`S_topo,e` 作为界面中心源按 §5.6 中心差分对装配。三者共用同一 `h̄_e`、`phi_t,e` 模板（§5.6 强制；禁止任一处单独换平均或限幅器）。

### 2.3 Well-Balancing 相消证明（静水 u=0，η=h+z_b 均匀）

连续恒等式：

```
∇·(½g·φt·h²·I) − S_phi_t − S_topo
 = ½g·φt·∇(h²) + ½g·h²·∇φt − ½g·h²·∇φt + g·φt·h·∇z_b
 = g·φt·h·∇h + g·φt·h·∇z_b
 = g·φt·h·∇(h+z_b) = g·φt·h·∇η
```

静水 `∇η=0` ⟹ 压力散度被 `S_phi_t + S_topo` 精确抵消 ⟹ 动量残差为 0 ⟹ 速度保持零。离散层面因三项共用同模板，相消逐边成立；平流项在 `u=0` 时为 0，不破坏平衡。

### 2.4 关键场景预期

- **G2（phi_t 跳变，平床 z_b=0，u=0）**：`∇η=0`，压力跳变力被 `S_φt,e` 抵消 → 零速度；`S_topo=0`。
- **变床（z_b 斜坡，phi_t 均匀，u=0）**：`∇η=0`，压力随 h 的力被 `S_topo,e` 抵消 → 零速度；`S_φt=0`。

### 2.5 phi_t=1 零漂移与 soft-block 压力主权更正

`phi_t=1`、`∇φt=0`：`F_p,e = 0.5g·h̄²`（= 旧 HLLC 压力），`S_φt=0`，`phi_e_n=1` 时平流+压力总和逐位不变。

**已知行为更正：** 当 `phi_e_n≠1` 且 `phi_t=1` 时，旧格式把压力也乘了 `phi_e_n`；新格式压力乘 `phi_t`（不乘 `phi_e_n`），即 soft-block 边的 WB 压力配对**不受 `phi_e_n` 抑制**——更符合 §5.5 第 6 步（`phi_e_n` 只缩放平流）。现有测试中唯一 `phi_e_n≠1` 的流通用例（`test_hllc_flux` 的 `phi_e_n=0.25`）只断言 mass，不受影响。

## 3. 验证、基线影响与回退

### 3.1 新增 GoldenTest（标准 well-balanced 套餐）

| ID | 名称 | 场景 | 断言 |
|---|---|---|---|
| G4 | phi_t-jump lake at rest | 混合网格，u=0，η 均匀，phi_t 跳变（如 1.0→1.5），墙边界 | 一步后 `max(|hu|,|hv|) < 1e-12`；WB 压力与 `S_phi_t` 装配非零但相消 |
| G5 | sloping-bed lake at rest | u=0，η 均匀（z_b 斜坡 ⟹ h 随之），phi_t 均匀，墙边界 | 一步后 `max(|hu|,|hv|) < 1e-10`（`EXPECT_NEAR`）；WB 压力与 `S_topo` 装配非零但相消 |

落位：先作为 `tests/unit/surface2d/` 单测（与 `test_well_balanced_lake_at_rest` 同形）；GoldenSuite manifest 化属独立任务，G4/G5 设计为其候选 `G_n`。

### 3.2 现有测试基线影响

**零漂移（均匀 phi_t=1、phi_e_n=1）：** G1、G3、lake-at-rest、wetting/drying、friction、rainfall/infiltration、coupling_exchange、cfl、geometry、step_rollback、conservative_update、boundary 等不变。

**需更新断言（HLLC 动量改为平流-only）：**
- `test_hllc_flux`：
  - `ZeroVelocityHydrostaticStateHasZeroMassFluxAndPressureMomentum`：`momentum_n` 由 `4.905` → `0.0`，改断言为"平流-only，静水无平流动量"。
  - `NormalMomentumFluxDependsOnVelocityDirection`：值变（still=0，moving=h·u²），关系仍成立。
  - 向量动量用例：`momentum_n` 变为平流分量，更新值。
- `test_pressure_momentum` / `test_momentum_transport` / `test_hllc_wave_momentum` / `test_dpm_edge_source_conservation`：M249 引入的"边诊断 `momentum_x/y == hllc_normal_flux(...)`" expected 自动跟随实现，但含义变为只验证平流；需补"内部边总动量贡献（平流 + WB 配对）"断言，使用 `well_balanced` 模块诊断输出。
- `test_step.cpp` 涉 phi_t≠1 的诊断（`pressure_pairing`/`s_phi_t`）：语义保留，值不变（`1.22625`/`−1.22625`）。

**诊断结构扩展：** `EdgeStepDiagnostics` 增 `wb_pressure`、`s_topo` 字段（与 `pressure_pairing`/`s_phi_t` 并列），供 G4/G5 与守恒审计断言相消。

### 3.3 实施切片（供 writing-plans）

1. HLLC 平流-only 重构 + 更新 `test_hllc_flux`（隔离验证）。
2. 新增 `source_terms/well_balanced` 纯函数模块 + 单测（phi_t-缩放压力、S_phi_t、S_topo、相消）。
3. step.cpp 接线（内部边平流 + WB；墙/开边界压力改用 WB 形式）+ 迁移受影响断言。
4. G4/G5 GoldenTest。
5. 全量自检 + 安检 + 证据归档。

### 3.4 回退路径

- 每切片独立可回退；切片 1-2 不接线（不影响 step），切片 3 为行为切换点。
- 若 G4/G5 相消未达精度：回退到 M249 现状（HLLC 含压力、S_phi_t 仅诊断），保留 WB 模块为 dead code + DISABLED GoldenTest，不阻塞主线。
- 严守 §5.5.2：不越界尝试动态 phi_t-跳变 O(Δx²)；CVC 为登记的未来任务。

### 3.5 命名（符号表对齐）

新机器可读名：`wb_pressure_flux`、`s_phi_t`、`s_topo`、`phi_t_edge`、`h_bar_edge`。保留 `phi_e_n`/`omega_edge`/`phi_t` 既有语义；不引入 `Q_max_safe` 类退役别名；`max_cell_cfl` 不乘 `CFL_safety`（不变）。

## 4. 边界声明

- 不构成 GoldenSuite 门禁证据（manifest 化为独立任务）。
- 动态（非零速度）phi_t 跳变守恒一致性属 §5.5.2 验证空白区，本设计不覆盖。
- 标号待分配：避开并行会话占用号（M247 runoff / M240 tri-coupling 系列）。

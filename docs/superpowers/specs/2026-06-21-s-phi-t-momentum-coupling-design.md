# S_phi_t 接入动量：Well-Balanced 压力-源项配对设计

> **状态：** 设计（brainstorming 产出，待 writing-plans 转实施）。已并入数值评审 `离散格式与 well-balancing 相消证明.md`：采纳 Path B（Audusse 单侧分裂）、修正 WB 边源系数约定（见 §2.2/§2.4b）。
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

纯函数模块，对每条内部边、**按单侧（per-cell-side）** 产出 WB 配对的动量贡献。采用 **Path B：Audusse 单侧分裂装配**（见 §2.2），使用同一界面模板（算术平均 `phi_t,e`、`h̄_e`，§5.5.2）与 Audusse 重构底高 `z_{b,e} = max(z_{b,i}, z_{b,j})`：

- phi_t-缩放压力界面通量（守恒，左右反号）：`F_p,e = 0.5·g·phi_t,e·h̄_e²`，沿 `n`
- 孔隙率单侧局部源（对单元 `i`）：`S_phi_t,e^(i) = 0.5·g·h̄_e²·(phi_t,e − phi_t,i)`，沿 `n_{ie}`
- 地形单侧局部源（对单元 `i`）：`S_topo,e^(i) = −g·phi_t,e·h̄_e·(z_{b,e} − z_{b,i})`，沿 `n_{ie}`
- 边分类门控（复用 M248 `classify_edge`，§5.6）：hard-block 不装配任何 WB 配对项；soft-block 与 regular 装配。

`h̄_e` 使用 hydrostatic reconstruction 后的 `h_L*`、`h_R*` 之均值（与压力通量同模板，§5.6）。`phi_t,e − phi_t,i = ½(phi_t,j − phi_t,i)`，故单侧孔隙率源等效于全跳变系数 **¼**（见 §2.2 系数校正）。`n_{ie}` 为单元 `i` 在边 `e` 的外法向。

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

### 2.2 边界面离散（Path B：Audusse 单侧分裂，同界面模板，§5.6）

**装配约定（关键）**：压力 `F_p,e` 作为**界面通量**（左右反号、守恒）；孔隙率与地形源按**单侧（per-cell-side）局部对消**装配——对边 `e` 的每一侧单元，用该单元自身的 `phi_t,i`、`z_b,i` 与边重构量构造源项，使该单元侧的"压力通量 − 源"精确退化为局部静水压梯度。这比"全边共享中心源同步累加两侧"更稳健（后者需把系数从 ½ 修正为 ¼，且在 Audusse 非线性截断 `h* = max(0, η − z_{b,e})` 下难以逐位对消）。

内部边 `e_ij`，单元 `i` 在边的外法向 `n_{ie}`，`h̄_e = ½(h_i*+h_j*)`，`phi_t,e = ½(phi_t,i+phi_t,j)`，Audusse 重构底高 `z_{b,e} = max(z_{b,i}, z_{b,j})`：

```
F_adv,e     = phi_e_n · (h·u·u_n)_HLLC                          [仅平流，无压力；界面通量]
F_p,e       = ½·g·phi_t,e·h̄_e²                                 [界面通量，左右反号]
S_phi_t,e^(i) = ½·g·h̄_e² · (phi_t,e − phi_t,i)                  [单元 i 侧局部源]
S_topo,e^(i)  = −g·phi_t,e·h̄_e · (z_{b,e} − z_{b,i})            [单元 i 侧局部源]
```

单元 `i` 的边 `e` 贡献（所有项乘 `n_{ie}·len_e`）：

```
dM_i += ( −F_adv,e − F_p,e + S_phi_t,e^(i) + S_topo,e^(i) ) · n_{ie} · len_e
```

单元 `j` 用其自身 `n_{je} = −n_{ie}`、`phi_t,j`、`z_b,j` 对称装配（`F_adv,e`、`F_p,e` 因 `n_{je}=−n_{ie}` 自动反号守恒）。三处必须共用同一 `h̄_e`、`phi_t,e`、`z_{b,e}` 模板（§5.6 强制；禁止任一处单独换平均或限幅器）。

**系数校正（评审拦截）**：`phi_t,e − phi_t,i = ½(phi_t,j − phi_t,i)`，即单侧源等效于"全跳变" `(phi_t,j − phi_t,i)` 的系数 **¼**。若误用全跳变系数 ½（在两侧同步累加约定下），等同重复计双倍孔隙率跳变、G4 立即 O(1) 报红。

### 2.3 Well-Balancing 单侧逐位相消证明（静水 u=0，η=h+z_b 均匀）

**连续恒等式**（方向性）：

```
∇·(½g·φt·h²·I) − S_phi_t − S_topo = g·φt·h·∇(h+z_b) = g·φt·h·∇η
```

静水 `∇η=0` ⟹ 压力散度被 `S_phi_t + S_topo` 精确抵消。

**离散单侧对消（单元 i 侧）**：

孔隙率项：
```
−F_p,e + S_phi_t,e^(i) = −½g·phi_t,e·h̄² + ½g·h̄²·(phi_t,e − phi_t,i) = −½g·phi_t,i·h̄²
```
即单元 i 侧的"压力通量 − 孔隙率源"精确退化为以**本单元 phi_t,i** 计的局部静水压力 `−½g·phi_t,i·h̄²`。

地形项：静水时 `η_i = h_i* + z_{b,e} = h_i + z_{b,i}`（重构与单元中心同 η），故 `z_{b,e} − z_{b,i} = h_i − h̄_e`（因 η 均匀，`h_i* = η − z_{b,e} = h̄_e`）。代入 `S_topo,e^(i) = −g·phi_t,e·h̄_e·(h_i − h̄_e)`。

将单侧总和沿单元 i 的闭合多边形求和：`Σ_e (−½g·phi_t,i·h̄_e²) n_{ie} len_e`。当 η 均匀、各边 `h̄_e = η − z_{b,e}` 由共享 `z_{b,e}` 决定时，配合 `S_topo` 项使各边贡献与单元内部静水压梯度逐位相消 ⟹ `dM_i = 0` ⟹ 速度保持零。平流项 `F_adv,e` 在 `u=0` 时为 0，不破坏平衡。

> **实施期锁定项（G5 验证目标）**：地形项的**逐位**对消依赖重构 `h_i*` 与 `S_topo` 源严格同模板。Audusse 标准在 `h_i ≠ h̄_e`（低床单元）时需配套的 centered 地形源分量；本设计采用单侧 `z_{b,e}=max` 重构形式，其在变床下的精确逐位对消由实施计划锁定 `h_i*` 使用细节，并以 G5（`EXPECT_NEAR`，tol 1e-10）为验收。平床 G4 为精确逐位（tol 1e-12）。

### 2.4 关键场景预期

- **G2/G4（phi_t 跳变，平床 z_b=0，u=0）**：`∇η=0`，压力跳变力被单侧 `S_phi_t,e^(i)` 逐位抵消 → 零速度；`S_topo=0`（tol 1e-12）。
- **G5（z_b 斜坡，phi_t 均匀，u=0）**：`∇η=0`，压力随 h 的力被单侧 `S_topo,e^(i)` 抵消 → 零速度；`S_phi_t=0`（tol 1e-10，Audusse 重构）。

### 2.4b 装配约定决策（Path B over Path A）

经数值评审（`离散格式与 well-balancing 相消证明.md`）核验，采纳 **Path B（Audusse 单侧分裂）**，否决 Path A（中心差分对 + 硬系数 ¼）：

- **Path A**（全边共享中心源、两侧同步累加）：须把 `S_phi_t` 全跳变系数硬改为 ¼；平床精确，但变床/陡坡/干湿下因 Audusse 非线性截断 `h*=max(0,η−z_{b,e})` 难以逐位对消。
- **Path B**（单侧局部对消，采纳）：用单元自身 `phi_t,i`/`z_b,i` 与重构 `z_{b,e}=max` 构造单侧源，逐位对消天然兼容变床与未来干湿，扩展性强；CUDA 线程内按 Left/Right 单侧装配，无需跨线程共享系数约定。

**教训（记入 cerebrum）**：WB 边源的系数依赖装配约定；"全跳变"与"单侧半跳变"差 2 倍，约定与系数必须配套，否则 G4 在 O(1) 级报红。

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

新机器可读名：`wb_pressure_flux`、`s_phi_t`、`s_topo`、`phi_t_edge`、`h_bar_edge`、`z_b_edge`（= `max(z_b_i, z_b_j)`，Audusse 重构底高）。单侧源记为 `s_phi_t_side`、`s_topo_side`（per-cell-side）。保留 `phi_e_n`/`omega_edge`/`phi_t` 既有语义；不引入 `Q_max_safe` 类退役别名；`max_cell_cfl` 不乘 `CFL_safety`（不变）。

## 4. 边界声明

- 不构成 GoldenSuite 门禁证据（manifest 化为独立任务）。
- 动态（非零速度）phi_t 跳变守恒一致性属 §5.5.2 验证空白区，本设计不覆盖。
- 标号待分配：避开并行会话占用号（M247 runoff / M240 tri-coupling 系列）。

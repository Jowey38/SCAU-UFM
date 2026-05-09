# SCAU-UFM 混合网格 HLLC + DPM well-balanced 数值 sandbox 设计

**日期**: 2026-05-08
**性质**: 工程类规范（条件性权威）
**适用范围**: 主仓库二维地表数值内核（`libs/surface2d/`）正式实施之前的数值方法可行性 sandbox
**配套主 Spec**: `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md`（§5 DPM 主方程与界面离散、§10.2 GoldenSuite 矩阵）
**配套稳定性协议**: `superpowers/specs/2026-04-14-scau-ufm-stability-reliability-protocol.md`

---

## 0. 文档定位

本 spec 是 SCAU-UFM Phase 1 工程实施的入口前提之一。在数值 sandbox 退出标准未达成前，主仓库 `libs/surface2d/dpm/`、`libs/surface2d/riemann/` 与 `libs/surface2d/source_terms/` 不得进入正式实现；主 Spec §5 在 sandbox 结论支持下可能需要回写补强。

本 spec 在退出标准达成且证据归档后转为历史记录，不再作为权威。

---

## 1. 目的与非目标

### 1.1 目的

- 在 CUDA 与正式工程实施之前，证明主 Spec §5 锁定的 well-balanced 模板与 HLLC 通量在三角/四边形混合非结构化网格上数值可达。
- 让 G1 `hydrostatic_step`、G2 `phi_t_jump_hydrostatic`、G3 `phi_c_edge_zero_velocity` 三项 GoldenTest 在原型代码上各自先通过一遍，避免主仓库 CUDA 实施过程中"理论问题与实现问题混在一起"。
- 用 sympy 派生 well-balanced 公式，独立于代码实现，使数值方法成为可审计的数学事实而非口头约定。

### 1.2 非目标

- 不实施完整 DPM 主方程的所有项（`drag` / `infiltration` / `rainfall` 等源项不进 sandbox）。
- 不验证 SWMM / D-Flow FM 耦合（属于 spike A 与 GoldenTest G8 / G11 / G12）。
- 不做性能优化、CUDA、向量化；sandbox 用最直白的 Python 实现。
- 不替代 GoldenSuite。sandbox 通过仅证明"数值方法可达"，主仓库正式实施仍必须独立通过 GoldenSuite gate。

---

## 2. 仓库落位

```text
sandbox/
└── numerical/
    ├── pyproject.toml
    ├── README.md
    ├── proofs/
    │   ├── well_balanced_hydrostatic.ipynb
    │   ├── phi_t_jump_pressure_pairing.ipynb
    │   └── phi_c_edge_zero_velocity.ipynb
    ├── src/
    │   ├── mesh.py
    │   ├── dpm_state.py
    │   ├── hllc.py
    │   ├── source_phi_t.py
    │   ├── reconstruction.py
    │   └── time_integration.py
    ├── tests/
    │   ├── test_g1_hydrostatic_step.py
    │   ├── test_g2_phi_t_jump_hydrostatic.py
    │   └── test_g3_phi_c_edge_zero_velocity.py
    ├── cases/
    │   ├── mixed_mesh_minimal.json
    │   └── reference_outputs/
    └── evidence/
        ├── well_balanced_proof.md
        ├── g1_run.md
        ├── g2_run.md
        ├── g3_run.md
        └── sandbox_report.md
```

约束：

- `sandbox/` 不与主仓库 `libs/` 共享任何代码或构建目标，独立 Python 环境。
- 实现优先正确性而非性能；不允许使用任何 CUDA / Numba / Cython 优化。
- `cases/reference_outputs/` 中的参考输出由 sympy 派生公式给出，而不是从 sandbox 自身回归生成（否则参考与被测同源失效）。

---

## 3. 测试网格的硬性要求

sandbox 必须在以下最小混合网格上跑通 G1 / G2 / G3。该网格虽小，但已暴露混合网格上 HLLC 边法向投影的所有典型情形：

```text
节点：
  N0 (0, 0)
  N1 (1, 0)
  N2 (2, 0)
  N3 (0, 1)
  N4 (1, 1)
  N5 (2, 1)

单元：
  C0 = quad   (N0, N1, N4, N3)
  C1 = tri    (N1, N2, N4)
  C2 = tri    (N2, N5, N4)

边：
  E0  N0–N1  内边的左/右单元 = (boundary, C0)
  E1  N1–N4  内边的左/右单元 = (C0, C1)      ← 关键：四边邻三角
  E2  N1–N2  内边的左/右单元 = (boundary, C1)
  E3  N2–N4  内边的左/右单元 = (C1, C2)      ← 三角邻三角
  E4  N4–N3  内边的左/右单元 = (C0, boundary)
  E5  N0–N3  内边的左/右单元 = (boundary, C0)
  E6  N2–N5  内边的左/右单元 = (boundary, C2)
  E7  N5–N4  内边的左/右单元 = (C2, boundary)
```

E1 是关键边：左侧四边形 C0、右侧三角 C1。任何只在四边形上推过 well-balanced 的实现，过 E1 时几乎一定崩。所有 G1 / G2 / G3 测试在该网格上必须通过。

此外，sandbox 必须额外构造两个对照网格：

- **quad-only 对照网格**：由 N0–N5 同组节点连成 2 个相邻四边形（C0' = N0,N1,N4,N3；C1' = N1,N2,N5,N4），覆盖 quad–quad 邻接。
- **tri-only 对照网格**：将上述区域三角化为 4 个三角，覆盖 tri–tri 邻接。

三个网格在节点几何上重合（N0–N5 坐标相同），唯一差异是单元拓扑，便于做单一变量对照。

约束：

- mesh.py 必须分别支持 `CellType.Triangle` 与 `CellType.Quadrilateral`，并对每个 Cell 用 `node_count` 与 `edge_count` 控制几何遍历，禁止假设所有单元为四边。
- 边法向、单元中心、面积、邻接关系必须由 mesh.py 计算，并写入 `cases/mixed_mesh_minimal.json` 作为 reference。

---

## 4. 必交付物

### 4.1 sympy 派生证明（`proofs/`）

三本 Jupyter notebook，每本至少包含：

- **`well_balanced_hydrostatic.ipynb`**：派生 hydrostatic reconstruction + HLLC 通量在 `eta = const, u = 0` 下的 well-balanced 性质。覆盖：
  - 三角邻三角（E3 类）。
  - 四边邻四边（不在最小网格内，但 sandbox 应额外构造一个 quad–quad 算例）。
  - 四边邻三角（E1 类）。
  - 边界条件下边法向投影的 well-balanced 性质（E0 / E5 / E2 类）。
- **`phi_t_jump_pressure_pairing.ipynb`**：派生 `phi_t` 跨界跳变下，主 Spec §5.5.2 中 Edge-Local 压力配对项与 `S_phi_t` 中心差分项的同模板等价性。证明二者相消使 `eta = const, u = 0` 下静水平衡仍成立。
- **`phi_c_edge_zero_velocity.ipynb`**：派生 `Phi_c / phi_{e,n}` 跨界跳变在零速度初值下不会产生虚假对流通量；覆盖 hard-block (`omega_edge = 0`) 与 soft-block (`phi_e_n < phi_edge_min`) 两种判据下的退化通量。

### 4.2 sandbox 实现（`src/`）

- `mesh.py`：实现混合三角/四边形拓扑、几何量、边邻接。
- `dpm_state.py`：状态变量 (h, hu, hv, eta) + DPM 字段 (`phi_t`, `Phi_c`, `phi_e_n`, `omega_edge`)。
- `hllc.py`：HLLC 边法向通量、波速估计、正性保持，**只支持双精度，固定顺序规约**。
- `source_phi_t.py`：`S_phi_t` 中心差分实现。
- `reconstruction.py`：hydrostatic reconstruction，覆盖 `phi_t` 跨界。
- `time_integration.py`：显式 Runge-Kutta（最低 RK2），固定步长，不做自适应 CFL，便于派生与回归对照。

实现禁忌：

- 禁止隐式 atomic add 或随机顺序求和——任何 reduction 必须是固定顺序。
- 禁止任何 CUDA、向量化、JIT 加速；纯 NumPy + Python loops 即可。
- 禁止隐式假设单元为四边或三角；类型分支必须显式。

### 4.3 GoldenTest 实现（`tests/`）

每个 GoldenTest 必须包含：

- 输入设置（DPM 字段、初始状态、边界条件）。
- 期望参考解（来自 sympy 派生而非 sandbox 自身回归）。
- 通过判据：
  - G1：`max |eta - eta_0| < eta_tol = 1e-12`，`max |u| < u_hydro_tol = 1e-12`，跑 1000 step。
  - G2：在 `phi_t` 阶跃工况下同 G1 判据。
  - G3：`max |u| < u_hydro_tol` 在所有 cell 上，跑 1000 step。
- 失败时打印每个 cell 与每条 edge 的诊断量（`max_cell_cfl`、边法向通量、`S_phi_t` 项、reconstruction 输出）。

---

## 5. 退出标准

下列条件全部满足才视为退出：

1. 三本 sympy 派生 notebook 全部跑通且结论清晰；`evidence/well_balanced_proof.md` 中给出 well-balanced 派生的 closed-form 与混合网格情形覆盖证明。
2. G1 / G2 / G3 在最小混合网格上各自通过 1000 step；`evidence/g1_run.md` / `g2_run.md` / `g3_run.md` 给出每个 cell 与每条边在最后一步的诊断。
3. G1 / G2 / G3 在 quad-only 与 tri-only 的对照网格上也分别通过；混合网格结果的 cell-wise 误差不大于单一类型网格的 2 倍。
4. E1（四边邻三角边）在 1000 step 后仍保持边法向通量为 0 且压力配对项与 `S_phi_t` 抵消量数值一致到 `1e-12`。
5. `evidence/sandbox_report.md` 总结主 Spec §5 是否需要任何回写补强；若需要，给出具体段落级补强建议。

---

## 6. 失败回退路径

- 若 G1 失败：主 Spec §5.4 hydrostatic reconstruction 不能直接用于实现，必须返工；暂停 `libs/surface2d/` 实施。
- 若 G2 失败：主 Spec §5.5.2 Edge-Local 压力配对在混合网格上不成立，必须按 sympy 结论补强或退回到 §5.5.2 已登记的 "Cea-Vázquez-Cendón 升级路径"；暂停 `libs/surface2d/dpm/` 与 `libs/surface2d/riemann/` 实施。
- 若 G3 失败：主 Spec §5 中 hard-block / soft-block 判据需要重新设计；同样暂停。
- 任一失败均触发 brainstorming 二轮，主 Spec §5 在结论支持下封板修订；不允许带着失败的 sandbox 进入主仓库实施。

---

## 7. 与 spike A 的关系

sandbox B 与 spike A 严格独立，可并行执行：

- spike A 验证第三方 1D 引擎接口假设；不涉及 2D 数值方法。
- sandbox B 验证 2D 数值方法假设；不涉及 SWMM / D-Flow FM。
- 两者退出后均成为 Phase 1 实施的入口前提。Phase 1 release gate 在 spike A 与 sandbox B 双双退出后才允许进入 GoldenSuite G1–G10 主仓库实施验证。

---

## 8. 证据归档

退出标准达成后：

- 三本 sympy notebook 与 `evidence/sandbox_report.md` 合并为 `superpowers/specs/2026-05-08-numerical-sandbox-design-evidence.md`，作为主 Spec §5 的 sympy 级数学事实参照。
- `cases/reference_outputs/` 提升为主仓库 GoldenTest 参考输入/输出的 baseline 之一（落位 `tests/golden/reference/` 时按 §10.2 的 G1 / G2 / G3 命名）。
- 本 spec 文件在归档后保留为历史记录，不作为权威。

---

## 9. 与 GoldenSuite 的关系

sandbox 通过的 G1 / G2 / G3 是"参考实现级"通过，不能替代主仓库 GoldenSuite gate。主仓库 `libs/surface2d/` 实施完成后，必须独立通过 §10.2 G1–G10 全部 GoldenTest，包括 sandbox 已通过的三项；GoldenSuite evidence 与 sandbox evidence 相互独立、互为参照。

未在主仓库通过 G1–G10 完整 GoldenSuite 之前，不允许以 "sandbox 已通过" 作为 release Phase 1 准入证据。

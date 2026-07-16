# GoldenSuite Manifest 化设计

> **状态：** 设计（brainstorming 产出，待 writing-plans 转实施）。
> **范围：** 全 G1-G12 矩阵登记 + surface2d 已实现子集（G1-G6）在 `tests/golden/` 下建专用 golden 测试激活为 CI gate。
> **权威依据：** 主 Spec `2026-04-11-scau-ufm-global-architecture-design.md` §10.1/§10.2/§10.3/§11；project-layout `project-layout-design.md` §4.20/§8.4；稳定性协议 `2026-04-14-scau-ufm-stability-reliability-protocol.md`。

## 1. 目录布局与 manifest

### 1.1 目录（project-layout §4.20）

```
tests/golden/
├── suite_manifest/
│   └── goldensuite.json        # 机器可读，全 G1-G12 单一事实源
├── reference/
│   └── tolerances.md           # u_hydro_tol/eta_tol=1e-12 等容差声明 + 基线版本
├── evidence/
│   └── <gate-run records>      # gate 执行结果/守恒证据落位
├── golden_tolerances.hpp       # C++ 常量头（§11 导出）
├── hydrostatic_step/           # G1
│   └── test_hydrostatic_step.cpp
├── phi_t_jump_hydrostatic/     # G2
│   └── test_phi_t_jump_hydrostatic.cpp
├── phi_c_edge_zero_velocity/   # G3
│   └── test_phi_c_edge_zero_velocity.cpp
├── narrow_gap_blockage/        # G4
│   └── test_narrow_gap_blockage.cpp
├── dpm_drag_decay/             # G5
│   └── test_dpm_drag_decay.cpp
├── phi_c_spd_reject/           # G6
│   └── test_phi_c_spd_reject.cpp
└── CMakeLists.txt              # 注册 6 个 golden 测试，打 ctest LABEL "golden"
```

目录名 = §10.2 test_name，满足 project-layout §4.20 同名约束 `test_path = tests/golden/<test_name>/`。

### 1.2 manifest schema（§4.20 最小字段集 + status）

`goldensuite.json` 登记**全 G1-G12**：

```json
[
  {
    "test_id": "G1",
    "name": "hydrostatic_step",
    "test_path": "tests/golden/hydrostatic_step/",
    "applicable_phase": "Phase 1+",
    "reference_path": "tests/golden/reference/tolerances.md",
    "ci_gate": true,
    "status": "implemented"
  },
  {
    "test_id": "G2",
    "name": "phi_t_jump_hydrostatic",
    "test_path": "tests/golden/phi_t_jump_hydrostatic/",
    "applicable_phase": "Phase 1+",
    "reference_path": "tests/golden/reference/tolerances.md",
    "ci_gate": true,
    "status": "implemented"
  },
  {
    "test_id": "G7",
    "name": "stcf_v4_to_v5_migration",
    "test_path": "tests/golden/stcf_v4_to_v5_migration/",
    "applicable_phase": "Phase 1+",
    "reference_path": "tests/golden/reference/tolerances.md",
    "ci_gate": false,
    "status": "pending"
  }
]
```
（示例；完整 manifest 包含 G1-G12 全部 12 项。）

- `test_id` 严格对齐主 Spec §10.2 的 G_n。
- `status`：`implemented`（G1-G6）/ `pending`（G7-G12）。
- `ci_gate`：G1-G6 为 `true`（active CI 门禁）；G7-G12 为 `false`（待实现后激活）。
- manifest 作为 GoldenSuite 完整性的单一事实源。稳定性协议"缺任一项=不完整"由此可机检。

## 2. 六个专用 golden 测试（§10.2 判据）

每个 `tests/golden/<name>/test_<name>.cpp`，复用已验证物理与最小混合网格，**判据严格照 §10.2**。这些是**新建的专用 golden 测试**——按门禁语义独立编写，不是把 unit 测试搬过来。Unit 测试保留原位，职责不同（单元级/组件级 vs 门禁级 release gate）。

| G_n | 名称 | §10.2 判据 → 断言 | 断言形态 |
|---|---|---|---|
| G1 | `hydrostatic_step` | eta 不漂移、速度 ≤ `u_hydro_tol` | Well-Balanced |
| G2 | `phi_t_jump_hydrostatic` | phi_t 跳变下仍保持静水平衡 | Well-Balanced |
| G3 | `phi_c_edge_zero_velocity` | Phi_c/phi_e_n 跳变不生成虚假速度 | Well-Balanced |
| G4 | `narrow_gap_blockage` | phi_e_n→0 无非法穿透通量 | Exact-Zero |
| G5 | `dpm_drag_decay` | 拖曳只耗散动量、不制造质量 | Conservation-Near + 衰减 |
| G6 | `phi_c_spd_reject` | 非正定 Phi_c 不得进入正确性路径 | fail-closed |

**命名纪律**：不复用 S_phi_t 工作中的临时内部"G4/G5"命名。Spec 权威：G4=`narrow_gap_blockage`，G5=`dpm_drag_decay`。我的"变床静水"在 §10.2 无对应 G_n，**不进 GoldenSuite**（留作 surface2d unit 回归）。

容差用 §11 命名常量，统一经 `golden_tolerances.hpp` 引用（见 §3.1）。每个测试打 ctest `LABEL "golden"`。

## 3. 门禁判据策略 + CI gate 绑定 + 证据落位

### 3.1 门禁判据策略（Criterion Strategy）

核心叙述："语义驱动断言，常量源自 Spec"。

**常量一致性**：所有静水漂移断言（G1-G3）引用 `tests/golden/golden_tolerances.hpp` 中由主 Spec §11 导出的 `u_hydro_tol`/`eta_tol`，禁止测试代码出现字面 magic number。"一改全改"集中于该头。

`golden_tolerances.hpp` 内容：

```cpp
#pragma once

namespace scau::golden {

// Main-spec §11 tolerance constants. Change here propagates to all golden tests.
// These match the authoritative values in the spec default-parameter table.
inline constexpr double u_hydro_tol = 1.0e-12;        // m/s
inline constexpr double eta_tol     = 1.0e-12;        // m
inline constexpr double conservation_near_tol = 1.0e-9; // recomposed-float anti-flaky

}  // namespace scau::golden
```

**断言形态差异化语义**：

- **物理平衡 Well-Balanced（G1-G3）**：`EXPECT_NEAR(max_abs_momentum, 0.0, scau::golden::u_hydro_tol)`；eta 漂移用 `eta_tol`。
- **精确零 Exact-Zero（G4）**：结构性零点 `EXPECT_EQ(val, 0.0)`，强制逻辑硬对齐（hard-block `return EdgeFlux{}` 字面零，不加容差）。
- **容差守恒 Conservation-Near（G5）**：动量衰减 `EXPECT_LT(|hu_friction|, |hu_control|)`（对无摩擦控制运行比较）；质量守恒 `EXPECT_NEAR(delta_mass, 0.0, scau::golden::conservation_near_tol)`，**显式注释"防 Flaky 浮点聚合噪声"**。
- **fail-closed（G6）**：`EXPECT_THROW(..., std::invalid_argument)`。

### 3.2 CI gate 绑定

在 `.github/workflows/ci.yml` 新增两个独立 job：

**1. `golden-suite-gate`**（构建 + 门禁测试）
```yaml
golden-suite-gate:
  name: golden-suite (${{ matrix.preset }})
  runs-on: ${{ matrix.os }}
  strategy:
    fail-fast: false
    matrix:
      include:
        - os: ubuntu-22.04
          preset: linux-gcc
        - os: windows-2022
          preset: windows-msvc
  steps:
    - uses: actions/checkout@v4
    # ... vcpkg bootstrap + cache (same as build-and-test) ...
    - run: cmake --preset ${{ matrix.preset }}
    - run: cmake --build --preset ${{ matrix.preset }}
    - run: ctest --preset ${{ matrix.preset }} -L golden --output-on-failure
```
只跑 `LABEL golden` 的 6 个 active golden 测试，作为独立命名门禁。失败即阻断 merge。满足稳定性协议"GoldenSuite 必须在 CI 中表征"。

**2. `goldensuite-manifest-check`**（机检完整性，纯脚本，无需构建）
```yaml
goldensuite-manifest-check:
  name: manifest-completeness
  runs-on: ubuntu-22.04
  steps:
    - uses: actions/checkout@v4
    - name: Check manifest completeness
      shell: bash
      run: |
        # Every G_n in goldensuite.json must exist
        # Every implemented+ci_gate entry must have test_path dir + LABEL golden in CMake
        # test_id same-name constraint: dir name == name field
        python3 tests/golden/suite_manifest/check_manifest.py
```
一个轻量 python 脚本校验 manifest 的内部一致性与完整性——把"缺任一项=不完整"变成强制 CI 不变式。

### 3.3 reference / evidence 落位

- `tests/golden/reference/tolerances.md`：容差声明（引 §11 常量值与来源）、参考基线版本、确定性路径说明（§10.3：CPU double、固定顺序规约）。
- `tests/golden/evidence/`：首次 gate 执行证据（6/6 通过 + 各判据类型记录）；后续每次 gate 追加。
- **变床静水**：不入 GoldenSuite（§10.2 无对应 G_n），保留为 surface2d unit 回归；evidence README 注明为"补充 well-balancing 检查，非门禁项"。

### 3.4 边界声明

- G7-G12 在 manifest 标 `pending`/`ci_gate:false`，不阻断；待对应能力实现后激活。
- 本设计不修改主 Spec §10.2（权威 G_n 不变）；manifest 是其工程落位，冲突时以 §10.2 为准。
- 不修改稳定性协议。
- Unit 测试保留原位（`tests/unit/surface2d/`），职责不同（组件级 vs 门禁级），不删除或迁移。

## 4. 实施切片概览（供 writing-plans）

1. 目录骨架 + `golden_tolerances.hpp` + `goldensuite.json`（含全 G1-G12）+ reference/tolerances.md。
2. G1-G6 六个专用 golden 测试（各自 test_*.cpp）+ `tests/golden/CMakeLists.txt`（`LABEL golden`）+ 根 CMakeLists 接入。
3. `check_manifest.py` 完整性检查脚本。
4. `.github/workflows/ci.yml` 新增两个 job（`golden-suite-gate` + `goldensuite-manifest-check`）。
5. 首次 gate 执行证据 + evidence/归档 + INDEX/OpenWolf。

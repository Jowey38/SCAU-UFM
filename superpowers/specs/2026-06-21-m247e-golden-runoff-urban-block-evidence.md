# M247-E 城市产流 Golden 算例证据

**日期：** 2026-06-21
**范围：** `tests/unit/surface2d/test_runoff_golden_urban_block.cpp`
**性质：** 城市产流（M247-A..D）端到端守恒 golden 算例。是 GoldenSuite 候选，但当前仍属单测层；尚未接入正式 `tests/golden/` manifest 门禁。

## 1. 算例

闭盒"城市街区"——`build_mixed_minimal_mesh` 的 3 个单元，分别赋 3 种下垫面：

- `C0`（四边形）：透水地面（Green-Ampt 入渗）。
- `C1`（三角）：不透水地面（全径流，无入渗）。
- `C2`（三角）：屋面，直排到 SWMM node 0；溢流目标为 `C0`。

驱动：roof-aware 10 参 `advance_one_step_cpu`（flux → ground runoff → roof runoff → exchange → friction），跨三段降雨过程：

| 段 | 雨强 (m/s) | 步数 |
|---|---|---|
| 常雨 | 1e-4 | 5 |
| 峰值 | 5e-4 | 3 |
| 雨停 | 0 | 5 |

屋面采用 mock acceptance（accept-all，但 `roof_drain_capacity` 极小），`roof_storage_capacity = 0`，故每步屋面入水大部溢流到 C0、极小量入管网——同时覆盖"入管"与"溢流"两条屋面出路。降雨量级取极小，使闭盒接近 lake-at-rest（CFL 平凡、全程不 rollback）；本 golden 验证**产流守恒**而非 CFL 稳定性。

## 2. 守恒判据（golden 断言）

1. **逐子步质量闭合**：`rainfall_volume == surface_added + infiltration + abstraction + depression_storage_delta + roof_to_swmm_accepted + roof_pending_delta + roof_overflow_to_surface`（1e-9）。
2. **逐子步毛雨量**：`rainfall_volume == rate * dt * total_area`（每个单元都受雨）。
3. **累计系统质量平衡**：闭盒内地表储量变化 `Δ(Σ phi_t*h*A) == Σ surface_added + Σ roof_overflow - Σ ponded_infiltration`（地面径流与屋面溢流向 h 注水；入渗/初损/洼蓄/入管/暂存均不进 h；M247-F 起，单元已有的地表积水 h 也参与单次 Green-Ampt 地面入渗，作为纯液相量 `ponded_infiltration` 从 h 中扣除），1e-8。
4. **行为覆盖**：`Σ roof_to_swmm_accepted > 0`（屋面入管发生）且 `Σ roof_overflow > 0`（屋面溢流发生）。
5. **Green-Ampt 单调**：透水单元 `cumulative_infiltration (F_inf)` 全程非减。
6. **雨停段**：雨停 5 步毛雨量为 0；累计毛雨量等于前两段手算值 `(1e-4*5 + 5e-4*3)*dt*total_area`。

### 2.1 M247-F: ponded h infiltration coupling (ponded_infiltration)

M247-F coupled the cell existing ponded surface water `h` into the single-call Green-Ampt
ground runoff chain. This changed the cumulative storage invariant from
`Δ(Σ phi_t*h*A) == Σ surface_added + Σ roof_overflow` to
`Δ(Σ phi_t*h*A) == Σ surface_added + Σ roof_overflow - Σ ponded_infiltration`, where
`ponded_infiltration` is a new pure-liquid audited quantity (no phi_t factor) reported as
`ponded_infiltration_volume` in `GroundRunoffResult` and `StepDiagnostics`. The pure-liquid
infiltration volume is removed from a phi_t-weighted storage column via
`dh = ponded_infiltration_volume/(phi_t*A)`. Two new low-porosity mini-goldens
(`LowPorosityInterfaceConservation`, `MixedRainAndPondedConservation`) specifically guard the
`O(1-phi_t)` liquid-volume leak across the surface-soil interface.

As a consequence of M247-F, the reused M247-E golden `ConservesAcrossRainPeakAndPostRainPhases`
is no longer a pure rain-only scenario: its initially-ponded pervious C0 cell now slowly drains
via Green-Ampt infiltration across the run. This is the intended M247-F semantic and conservation
is preserved (the surface-storage invariant accounts for it via the ponded_infiltration term); it
is not a regression.

## 3. 自检证据

- 构建：`cmake --build --preset windows-msvc` 0 编译/链接错误，0 LNK1168。
- 测试：`ctest --preset windows-msvc` → **53/53 通过**（含本 golden；Total ≈ 11.3 s）。
  - Note: this 53 counts ctest entries (one per test executable / gtest suite binary), not
    individual gtest TEST() cases. M247-F added 6 new gtest cases inside existing executables
    (2 mini-goldens in test_runoff_golden_urban_block + 2 in test_runoff_ground + 2 in
    test_runoff_step), so the ctest entry count is unchanged at 53/53 post-M247-F.
- 单算例：`test_runoff_golden_urban_block` 1/1 通过。

## 4. 边界声明

- mock acceptance：屋面入管未接真实 SWMM；真实 CouplingLib/SWMM adapter（写节点 lateral inflow `q = accepted/dt_sub`）待 M240 三方驱动 + SWMM 引擎落到本基线后接入端口。
- 本算例用最小混合网格 + 逐单元下垫面赋值；大规模真实城区网格 golden 与正式 `tests/golden/` GoldenSuite manifest 门禁仍为后续项。
- 守恒判据以守恒律本身为参考（而非脆弱的逐项手算魔数），更稳健地证明集成热路径的产流质量闭合。

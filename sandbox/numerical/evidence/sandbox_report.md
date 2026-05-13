# sandbox B exit report

**日期**: 2026-05-12
**对应规范**: `superpowers/specs/2026-05-08-numerical-sandbox-design.md`

## 结论

sandbox B 达成 DPM / HLLC / source terms 的参考实现级退出条件，可作为 `libs/surface2d/dpm/`、`libs/surface2d/riemann/`、`libs/surface2d/source_terms/` 正式实施前的数值方法证据。

该结论不替代主仓库 GoldenSuite gate；正式 `Surface2DCore` 实施后仍必须独立通过 G1-G10。

## 已归档证据

- `sandbox/numerical/proofs/well_balanced_hydrostatic.ipynb`: hydrostatic reconstruction + HLLC 在 `eta = const, u = 0` 下的零质量通量证明。
- `sandbox/numerical/proofs/phi_t_jump_pressure_pairing.ipynb`: `phi_t` 跳变 pressure pairing 与 `S_phi_t` 精确相消证明。
- `sandbox/numerical/proofs/phi_c_edge_zero_velocity.ipynb`: `Phi_c / phi_e_n` 跳变在零速度、hard-block、soft-block 下的零通量退化证明。
- `sandbox/numerical/evidence/well_balanced_proof.md`: closed-form 证明摘要与混合网格覆盖说明。
- `sandbox/numerical/evidence/g1_run.md`: G1 mixed / quad-only / tri-only 1000 step 诊断。
- `sandbox/numerical/evidence/g2_run.md`: G2 mixed / quad-only / tri-only 1000 step 诊断。
- `sandbox/numerical/evidence/g3_run.md`: G3 mixed / quad-only / tri-only 1000 step 诊断。

## 本地验证

- `python sandbox/numerical/src/run_golden_sandbox.py`: PASS，并重写 `g1_run.md` / `g2_run.md` / `g3_run.md`。
- `python -m unittest discover -s sandbox/numerical/tests -p "test_*.py"`: 3 tests OK。
- `python -c "import sympy"`: 当前机器未安装 `sympy`；notebook 已以执行输出归档，复跑依赖记录在 `sandbox/numerical/pyproject.toml`。

## 退出标准核对

| 条件 | 状态 | 证据 |
|---|---|---|
| 三本 sympy 派生 notebook 跑通且结论清晰 | 已归档 | `proofs/*.ipynb` 均包含执行输出 `0` 或 `(0, 0)`；本机复跑需先安装 `pyproject.toml` 依赖 |
| G1/G2/G3 mixed mesh 跑 1000 step | 已满足 | `g1_run.md`、`g2_run.md`、`g3_run.md` 的 `mixed` 均为 `PASS` |
| G1/G2/G3 quad-only 和 tri-only 对照跑 1000 step | 已满足 | 三个 run evidence 中 `quad_only`、`tri_only` 均为 `PASS` |
| mixed cell-wise 误差不大于单一类型网格 2 倍 | 已满足 | 三组网格 `max_eta_error = 0.000e+00`、`max_velocity = 0.000e+00` |
| E1 四边邻三角零通量且 pressure/source 抵消到 `1e-12` | 已满足 | G2 mixed E1: `mass_flux = 0.000e+00`、`residual = 0.000e+00` |
| 主 Spec §5 回写判断 | 已完成 | 见下节 |

## 主 Spec §5 回写建议

建议对主 Spec §5 做段落级补强，但不需要改变核心数值模板：

1. 在 hydrostatic reconstruction / HLLC 小节明确：well-balanced 证明只依赖边法向投影，不依赖 cell 类型；实现必须覆盖 quad-tri、tri-tri、quad-quad 和边界边。
2. 在 `phi_t` pressure pairing / `S_phi_t` 小节明确二者必须共享同一个 edge-local 差分模板并以相反号进入残差，避免未来实现中使用不一致 stencil。
3. 在 `Phi_c / phi_e_n` 小节明确 hard-block (`omega_edge = 0`) 与 soft-block (`phi_e_n < phi_edge_min`) 必须在 HLLC 通量入口处显式退化为零通量。

这些是文字补强，不要求推翻主 Spec §5 的既有公式。

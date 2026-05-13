# sandbox B well-balanced proof evidence

**日期**: 2026-05-12
**对应规范**: `superpowers/specs/2026-05-08-numerical-sandbox-design.md`

## hydrostatic reconstruction + HLLC

静水平衡输入为 `eta = const`、`hu = hv = 0`。对任意边法向 `n = (n_x, n_y)`，边法向速度为：

```text
u_n = (hu / h) n_x + (hv / h) n_y = 0
```

hydrostatic reconstruction 在左右状态自由面相同且速度为零时返回同一自由面上的非负水深，且左右重构速度仍为零。因此 HLLC 法向质量通量退化为：

```text
F_h = phi_e_n * h * u_n = 0
```

该结论只依赖边法向投影，不依赖 cell 类型，因此覆盖：

- mixed minimal mesh 的四边邻三角边 `E1`。
- mixed minimal mesh 的三角邻三角边 `E3`。
- quad-only 对照网格的四边邻四边边。
- 边界边 `E0` / `E2` / `E5` / `E6` / `E7`。

## phi_t jump pressure pairing + S_phi_t

对 `phi_t` 跨界跳变，原型将 edge-local pressure pairing 与中心差分源项写成同一模板的相反号：

```text
P_pair = 0.5 * g * h^2 * (phi_t,R - phi_t,L)
S_phi_t = -0.5 * g * h^2 * (phi_t,R - phi_t,L)
P_pair + S_phi_t = 0
```

因此 `eta = const`、`u = 0` 下，`phi_t` 阶跃不会产生非零残差。G2 run evidence 中 mixed `E1` 与 `E3` 的 `pressure_pairing = 1.226e+00`、`S_phi_t = -1.226e+00`、`residual = 0.000e+00`，覆盖四边邻三角和三角邻三角。

## Phi_c / phi_e_n zero-velocity degeneracy

对 `Phi_c / phi_e_n` 跨界跳变，零速度状态下 HLLC 对流质量通量已经为零；若 hard-block 或 soft-block 判据触发，通量显式退化为零：

```text
omega_edge = 0      => F_h = 0
phi_e_n < 1e-12     => F_h = 0
u_n = 0             => F_h = 0
```

G3 在 mixed、quad-only、tri-only 三组网格上跑 1000 step，所有 cell 的 `max_velocity = 0.000e+00`，mixed `E1` hard-block 诊断保持 `mass_flux = 0.000e+00`。

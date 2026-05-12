# Plan-M2 最小 2D Surface2DCore 骨架退出证据

**日期**: 2026-05-12
**对应实施计划**: `superpowers/plans/2026-05-11-plan-m2-surface2d-core-skeleton.md`

## 1. 退出标准核对

| 标准 | 状态 | 证据 |
|---|---|---|
| `libs/mesh/` 支持 Triangle / Quadrilateral 混合拓扑 | 已完成 | `tests/unit/mesh/test_mesh_geometry.cpp` 覆盖 mixed minimal、quad-only、tri-only。 |
| mesh 计算面积、中心、边长、边中点、法向和邻接 | 已完成 | `test_mesh_geometry` 覆盖 C0/C1/C2 面积中心与 E0/E1/E3/E6 几何。 |
| mesh 拒绝非法 cell / edge specs | 已完成 | `tests/unit/mesh/test_mesh_validation.cpp` 覆盖错误拓扑、重复 ID、缺失/额外边、反向 left-cell。 |
| `libs/surface2d/` 只提供状态与 CPU 空推进骨架 | 已完成 | `tests/unit/surface2d/test_surface_state.cpp` 与 `tests/unit/surface2d/test_step.cpp` 通过；未实现正式 DPM/HLLC/source terms。 |
| 本地 Windows preset 构建和测试通过 | 已通过 | `PATH="/d/Program Files/CMake/bin:$PATH" cmake --build --preset windows-msvc` 与 `PATH="/d/Program Files/CMake/bin:$PATH" ctest --preset windows-msvc` 通过，6/6 tests passed，总测试耗时 2.52 sec。 |
| GitHub Actions matrix 通过 | 已通过 | CI run `25725318038` 通过，run URL: https://github.com/Jowey38/SCAU-UFM/actions/runs/25725318038；`windows-msvc` success，142 sec；`linux-gcc` success，54 sec；总耗时 146 sec。 |

## 2. 本地验证记录

```text
PATH="/d/Program Files/CMake/bin:$PATH" cmake --build --preset windows-msvc
```

结果：通过。生成目标包括 `scau_core`、`scau_mesh`、`scau_surface2d`、`test_core_types`、`test_core_error`、`test_mesh_geometry`、`test_mesh_validation`、`test_surface_state`、`test_surface_step`。

```text
PATH="/d/Program Files/CMake/bin:$PATH" ctest --preset windows-msvc
```

结果：通过。

```text
100% tests passed, 0 tests failed out of 6
Total Test time (real) = 2.52 sec
```

测试清单：

```text
test_core_types        Passed
test_core_error        Passed
test_mesh_geometry     Passed
test_mesh_validation   Passed
test_surface_state     Passed
test_surface_step      Passed
```

## 3. GitHub Actions 验证记录

- Commit: `2fe6f8d1cce49c7df42a838c721e358113ee3d51`
- Workflow: `CI`
- Run URL: https://github.com/Jowey38/SCAU-UFM/actions/runs/25725318038
- Run status: `completed`, conclusion: `success`
- Run started: `2026-05-12T09:19:15Z`
- Run updated: `2026-05-12T09:21:41Z`
- Total duration: 146 sec
- `windows-msvc`: `success`, `2026-05-12T09:19:18Z` → `2026-05-12T09:21:40Z`, 142 sec
- `linux-gcc`: `success`, `2026-05-12T09:19:25Z` → `2026-05-12T09:20:19Z`, 54 sec

## 4. 范围约束

- 本 M2 不引入 SWMM、D-Flow FM 或任何 1D engine ABI。
- 本 M2 不正式实现 `libs/surface2d/dpm/`、`libs/surface2d/riemann/`、`libs/surface2d/source_terms/`，等待 sandbox B 完整退出。
- `max_cell_cfl` 保持原始物理 CFL 诊断字段；当前 CPU skeleton 返回 `0.0`，后续正式数值推进再填充真实值。

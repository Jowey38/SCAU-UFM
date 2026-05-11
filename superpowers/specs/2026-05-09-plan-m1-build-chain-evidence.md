# Plan-M1 退出证据

**日期**: 2026-05-11
**对应实施计划**: `superpowers/plans/2026-05-09-plan-m1-build-chain.md`

## 1. 退出标准核对

| 标准 | 状态 | 证据 |
|---|---|---|
| 干净 Linux 环境 (Ubuntu 22.04 + GCC 12) 全流程通过 | 已通过 | GitHub Actions `linux-gcc` job 通过：`https://github.com/Jowey38/SCAU-UFM/actions/runs/25634269933/job/75243277918`，耗时约 1 分 4 秒。 |
| 干净 Windows 环境 (VS 2022) 全流程通过 | 已通过 | 本地执行 `cmake --build --preset windows-msvc && ctest --preset windows-msvc`，输出 `100% tests passed, 0 tests failed out of 2`。 |
| GitHub Actions matrix 通过且冷启动 ≤ 12 分钟 / 缓存后 ≤ 4 分钟 | 已通过 | Run `https://github.com/Jowey38/SCAU-UFM/actions/runs/25634269933` 通过；`linux-gcc` 约 1 分 4 秒，`windows-msvc` 约 2 分 16 秒，总耗时约 2 分 20 秒。 |
| `core::Index` / `core::Real` / `core::Vector2` / `core::ScauError` / `SCAU_ASSERT` 全部有单元测试 | 已完成 | `tests/unit/core/test_types.cpp` / `tests/unit/core/test_error.cpp`，共 9 个 GoogleTest 测试用例。 |
| README 含可复制构建测试指令 | 已完成 | `README.md` 的 `Build & Test` 节包含 Linux 与 Windows preset 命令。 |

## 2. 已知风险与下一步

- M1 退出证据已补齐：本地 Windows 验证与 GitHub Actions Linux/Windows matrix 均通过。
- M1 不引入任何 1D 引擎依赖；spike A 与 sandbox B 仍在并行进行，结论尚未回写。
- M2 入口前提：本 Plan-M1 全部退出标准达成 + sandbox B mesh.py 原型可用。
- M3 入口前提：M2 退出 + sandbox B 全部退出标准达成。

## 3. 归档

本文件作为 M1 完成的可审计证据归入 `superpowers/specs/`；M2 plan 撰写时引用本文件作为入口前提。

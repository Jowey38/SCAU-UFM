# Plan-M1 退出证据

**日期**: 2026-05-11
**对应实施计划**: `superpowers/plans/2026-05-09-plan-m1-build-chain.md`

## 1. 退出标准核对

| 标准 | 状态 | 证据 |
|---|---|---|
| 干净 Linux 环境 (Ubuntu 22.04 + GCC 12) 全流程通过 | 待验证 | 尚未在 Linux 环境或 GitHub Actions Linux job 中取得通过证据。 |
| 干净 Windows 环境 (VS 2022) 全流程通过 | 已通过 | 本地执行 `cmake --build --preset windows-msvc && ctest --preset windows-msvc`，输出 `100% tests passed, 0 tests failed out of 2`。 |
| GitHub Actions matrix 通过且冷启动 ≤ 12 分钟 / 缓存后 ≤ 4 分钟 | 待验证 | `.github/workflows/ci.yml` 已创建；尚未推送并取得 GitHub Actions run URL 与 duration。 |
| `core::Index` / `core::Real` / `core::Vector2` / `core::ScauError` / `SCAU_ASSERT` 全部有单元测试 | 已完成 | `tests/unit/core/test_types.cpp` / `tests/unit/core/test_error.cpp`，共 9 个 GoogleTest 测试用例。 |
| README 含可复制构建测试指令 | 已完成 | `README.md` 的 `Build & Test` 节包含 Linux 与 Windows preset 命令。 |

## 2. 已知风险与下一步

- M1 尚未完全退出：Linux 冷启动验证与 GitHub Actions matrix 证据仍缺失。
- M1 不引入任何 1D 引擎依赖；spike A 与 sandbox B 仍在并行进行，结论尚未回写。
- M2 入口前提：本 Plan-M1 全部退出标准达成 + sandbox B mesh.py 原型可用。
- M3 入口前提：M2 退出 + sandbox B 全部退出标准达成。

## 3. 归档

本文件作为 M1 完成度的可审计证据归入 `superpowers/specs/`；M2 plan 撰写时引用本文件作为入口前提。当前文件记录的是阶段性证据，不代表 M1 已满足全部退出标准。

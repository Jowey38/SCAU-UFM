# 第三方版本兼容性矩阵

| 组件 | 锁定版本 | 嵌入模式 | 构建/接入 | 兼容性结论 | 证据 |
|---|---|---|---|---|---|
| SWMM | 5.2.4 (VERSION 52004, develop) | 源码静态嵌入 | `cmake/third_party/swmm.cmake` → `scau::extern_swmm5` | MSVC 19.x / C99 编译通过；公共 API 集成测试 5/5 PASS | `tests/unit/coupling/test_coupling_swmm_engine.cpp`、M240 evidence |
| D-Flow FM kernel | Delft3D-main 快照（接口契约） | 运行时 DLL 动态接入 | `cmake/third_party/dflowfm.cmake`（运行时配置，无构建） | BMI 1.0 契约锁定；kernel 步进证据待外部构建 DLL | `extern/dflowfm/dflowfm_lib/include/`、M240 evidence §3 |
| D-Flow FM BMI bridge | BMI 1.0 (dimr_lib bmi.h) | 头文件快照 | `cmake/third_party/bmi_bridge.cmake` → INTERFACE | fail-closed 边界测试 4/4 PASS | `tests/unit/coupling/test_coupling_dflowfm_engine.cpp` |

升级流程见 `upgrade-policy.md`；ABI 边界与调用约定见 `abi-boundary-policy.md`。

## gmsh preprocessing determinism (B7)

| Platform | python | gmsh | numpy | netCDF4 | G30 verdict | Evidence |
|---|---|---|---|---|---|---|
| Windows 11 x86_64 (fixture author host) | 3.14.4 | 4.15.2 | 2.5.2 | 1.7.4 | bitwise | `b7_evidence/windows-x86_64.json` (2026-09-09) |
| ubuntu-22.04 x86_64 (CI `preproc-cross-platform`) | 3.12 | 4.15.2 (lock) | 2.5.2 | 1.7.4 | pending first artifact | CI artifact `b7-evidence-linux-x86_64`; copy the row here once collected |

Gate wording decision (plan v3 §12 "跨平台门禁口径") is taken in the B7 evidence
document once the Linux row exists: `bitwise` → same wording as today;
`tolerance_1e-12` → promote the lane with the recorded tolerance; `topology_differs`
→ platform governance (fixtures stay same-platform; the lane stays non-blocking).

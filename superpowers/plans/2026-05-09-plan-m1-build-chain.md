# Plan-M1: 构建链与脚手架 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在主仓库根目录搭起可在 Windows 与 Linux 上配置、构建、测试并通过 GitHub Actions CI 的最小 C++20 工程骨架，含 `libs/core/` 基础类型与错误处理、GoogleTest 单元测试贯通，作为后续 M2–M5 全部里程碑的工程母床。

**Architecture:** 顶层 CMake + vcpkg manifest 模式管理依赖；`libs/core/` 作为唯一无内部依赖的公共基础库，承载共享类型 `Index` / `Real` / `Vector2` 与异常类 `ScauError` 及断言宏 `SCAU_ASSERT`；测试落位 `tests/unit/<lib_name>/`，通过 CTest 集成；GitHub Actions 用 matrix 同时构建 Windows + Linux 两路并跑测试。

**Tech Stack:** CMake 3.27+ / vcpkg manifest mode / C++20 / GoogleTest 1.14+ / GitHub Actions / GCC 12+ (Linux) / MSVC 19.32+ (Windows)

**入口前提:**
- 无（M1 与 spike A、sandbox B 可并行启动）

**退出标准:**
- 在两个干净环境（Windows 11 + Visual Studio 2022 / Ubuntu 22.04 + GCC 12）下，`cmake --preset` + `cmake --build` + `ctest` 全部通过
- GitHub Actions CI 在两个 OS matrix 上从冷启动到全部测试通过 ≤ 12 分钟（vcpkg 冷拉），加缓存后 ≤ 4 分钟
- `libs/core/` 暴露 `core::Index` / `core::Real` / `core::Vector2` / `core::ScauError` / `SCAU_ASSERT` 五项 API，全部有单元测试覆盖
- 仓库根 README.md 含可复制的"克隆 → 构建 → 测试"三步指令

---

## File Structure

| 文件 | 责任 |
|---|---|
| `CMakeLists.txt`（仓库根） | 顶层 project()、C++20 全局、warnings、enable_testing()、add_subdirectory(libs/core, tests) |
| `vcpkg.json` | manifest 模式声明 GoogleTest 依赖 |
| `CMakePresets.json` | windows-msvc / linux-gcc 两个 configure preset + 对应 build/test preset |
| `cmake/modules/ScauWarnings.cmake` | 跨编译器统一的警告等级 + interface target `scau::warnings` |
| `libs/core/CMakeLists.txt` | 定义 `scau_core` static library target + include 暴露 |
| `libs/core/include/core/types.hpp` | `Index` / `Real` / `Vector2` 公共类型 |
| `libs/core/include/core/error.hpp` | `ScauError` 异常类、`SCAU_ASSERT` 宏 |
| `libs/core/src/error.cpp` | `ScauError` 实现 |
| `tests/CMakeLists.txt` | GoogleTest 发现 + add_subdirectory(unit/core) |
| `tests/unit/core/CMakeLists.txt` | 注册 `test_core_types`、`test_core_error` 两个测试 |
| `tests/unit/core/test_types.cpp` | 验证 Index 容量、Real 精度、Vector2 算术 |
| `tests/unit/core/test_error.cpp` | 验证 ScauError 抛出、SCAU_ASSERT 触发 |
| `.github/workflows/ci.yml` | matrix(windows-latest, ubuntu-latest) 配置-构建-测试-缓存 |
| `.gitignore` | 忽略 build/ / vcpkg_installed/ / .vs/ / *.user |
| `README.md` | 项目简介 + 构建测试指令（覆盖既有最小 README） |

---

## Task 1: 仓库根 .gitignore

**Files:**
- Create: `.gitignore`

- [ ] **Step 1: 写 .gitignore**

```gitignore
# Build directories
build/
build-*/
out/
cmake-build-*/

# vcpkg
vcpkg_installed/
.vcpkg-root

# IDE
.vs/
.vscode/
.idea/
*.user
*.suo

# OS
.DS_Store
Thumbs.db

# Editors
*.swp
*~

# Compiler artefacts
*.o
*.obj
*.lib
*.a
*.exp
*.pdb
*.ilk
```

- [ ] **Step 2: 提交**

```bash
git add .gitignore
git commit -m "chore: add gitignore for cmake / vcpkg / IDE artefacts"
```

---

## Task 2: vcpkg.json manifest

**Files:**
- Create: `vcpkg.json`

- [ ] **Step 1: 写 manifest**

```json
{
  "name": "scau-ufm",
  "version-string": "0.1.0",
  "description": "SCAU urban flood model: Anisotropic DPM HLLC + SWMM + D-Flow FM coupled platform",
  "license": null,
  "dependencies": [
    {
      "name": "gtest",
      "version>=": "1.14.0"
    }
  ],
  "builtin-baseline": "0fb1a37835ddc09ef7df09b6c4131773c8ad3a2c"
}
```

> Note: `builtin-baseline` 是 vcpkg 仓库中一次已知 commit 的 SHA，用来锁定依赖图。M1 实施时，请先运行 `git -C $VCPKG_ROOT log -1 --format=%H` 取当前 vcpkg HEAD，然后回填到上面字段；仓库内固定 baseline 让 CI 可重复。

- [ ] **Step 2: 提交**

```bash
git add vcpkg.json
git commit -m "build: declare vcpkg manifest with gtest 1.14"
```

---

## Task 3: 顶层 CMakeLists.txt 骨架

**Files:**
- Create: `CMakeLists.txt`

- [ ] **Step 1: 写顶层 CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.27)

project(SCAU_UFM
    VERSION 0.1.0
    DESCRIPTION "SCAU urban flood model"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

include(ScauWarnings)

enable_testing()

add_subdirectory(libs/core)
add_subdirectory(tests)
```

- [ ] **Step 2: 暂不构建（依赖 Task 4 的 ScauWarnings.cmake 与 Task 6 的 libs/core/）；本任务只定义骨架，先提交骨架**

```bash
git add CMakeLists.txt
git commit -m "build: add top-level CMakeLists with C++20 and module path"
```

---

## Task 4: cmake/modules/ScauWarnings.cmake

**Files:**
- Create: `cmake/modules/ScauWarnings.cmake`

- [ ] **Step 1: 写 ScauWarnings.cmake**

```cmake
# Defines INTERFACE target scau::warnings carrying project-wide warning flags.
# Linking against scau::warnings opts into the warning set without polluting
# global flags.

if(TARGET scau::warnings)
    return()
endif()

add_library(scau_warnings INTERFACE)
add_library(scau::warnings ALIAS scau_warnings)

if(MSVC)
    target_compile_options(scau_warnings INTERFACE
        /W4
        /permissive-
        /w14242 /w14254 /w14263 /w14265 /w14287
        /we4289 /w14296 /w14311 /w14545 /w14546
        /w14547 /w14549 /w14555 /w14619 /w14640
        /w14826 /w14905 /w14906 /w14928
    )
else()
    target_compile_options(scau_warnings INTERFACE
        -Wall -Wextra -Wpedantic
        -Wshadow -Wnon-virtual-dtor -Wold-style-cast
        -Wcast-align -Wunused -Woverloaded-virtual
        -Wconversion -Wsign-conversion
        -Wdouble-promotion -Wformat=2
    )
endif()

option(SCAU_WARNINGS_AS_ERRORS "Promote warnings to errors" OFF)
if(SCAU_WARNINGS_AS_ERRORS)
    if(MSVC)
        target_compile_options(scau_warnings INTERFACE /WX)
    else()
        target_compile_options(scau_warnings INTERFACE -Werror)
    endif()
endif()
```

- [ ] **Step 2: 提交**

```bash
git add cmake/modules/ScauWarnings.cmake
git commit -m "build: add scau::warnings interface target for warning policy"
```

---

## Task 5: CMakePresets.json

**Files:**
- Create: `CMakePresets.json`

- [ ] **Step 1: 写 presets**

```json
{
  "version": 6,
  "cmakeMinimumRequired": {
    "major": 3,
    "minor": 27,
    "patch": 0
  },
  "configurePresets": [
    {
      "name": "linux-gcc",
      "displayName": "Linux GCC Debug",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/linux-gcc",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "CMAKE_TOOLCHAIN_FILE": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake",
        "VCPKG_TARGET_TRIPLET": "x64-linux",
        "SCAU_WARNINGS_AS_ERRORS": "ON"
      },
      "condition": {
        "type": "equals",
        "lhs": "${hostSystemName}",
        "rhs": "Linux"
      }
    },
    {
      "name": "windows-msvc",
      "displayName": "Windows MSVC Debug",
      "generator": "Visual Studio 17 2022",
      "binaryDir": "${sourceDir}/build/windows-msvc",
      "cacheVariables": {
        "CMAKE_TOOLCHAIN_FILE": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake",
        "VCPKG_TARGET_TRIPLET": "x64-windows",
        "SCAU_WARNINGS_AS_ERRORS": "ON"
      },
      "condition": {
        "type": "equals",
        "lhs": "${hostSystemName}",
        "rhs": "Windows"
      }
    }
  ],
  "buildPresets": [
    {
      "name": "linux-gcc",
      "configurePreset": "linux-gcc",
      "configuration": "Debug"
    },
    {
      "name": "windows-msvc",
      "configurePreset": "windows-msvc",
      "configuration": "Debug"
    }
  ],
  "testPresets": [
    {
      "name": "linux-gcc",
      "configurePreset": "linux-gcc",
      "configuration": "Debug",
      "output": {
        "outputOnFailure": true
      },
      "execution": {
        "noTestsAction": "error",
        "stopOnFailure": false
      }
    },
    {
      "name": "windows-msvc",
      "configurePreset": "windows-msvc",
      "configuration": "Debug",
      "output": {
        "outputOnFailure": true
      },
      "execution": {
        "noTestsAction": "error",
        "stopOnFailure": false
      }
    }
  ]
}
```

- [ ] **Step 2: 提交**

```bash
git add CMakePresets.json
git commit -m "build: add presets for windows-msvc and linux-gcc"
```

---

## Task 6: libs/core/ CMake target

**Files:**
- Create: `libs/core/CMakeLists.txt`

- [ ] **Step 1: 写 libs/core/CMakeLists.txt**

```cmake
add_library(scau_core
    src/error.cpp
)
add_library(scau::core ALIAS scau_core)

target_include_directories(scau_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(scau_core
    PRIVATE
        scau::warnings
)

target_compile_features(scau_core PUBLIC cxx_std_20)
```

- [ ] **Step 2: 暂不构建（依赖 Task 7、Task 8 的源文件存在）；先提交目标定义**

```bash
git add libs/core/CMakeLists.txt
git commit -m "build(core): declare scau::core static library target"
```

---

## Task 7: TDD `core::Index` / `core::Real` / `core::Vector2`

**Files:**
- Create: `tests/unit/core/test_types.cpp`
- Create: `tests/unit/core/CMakeLists.txt`
- Create: `tests/CMakeLists.txt`
- Create: `libs/core/include/core/types.hpp`

- [ ] **Step 1: 写失败测试 `test_types.cpp`**

```cpp
#include <gtest/gtest.h>
#include <core/types.hpp>

#include <cstdint>
#include <limits>
#include <type_traits>

namespace {

TEST(CoreTypes, IndexIsAtLeast32BitSigned) {
    static_assert(std::is_signed_v<scau::core::Index>,
                  "Index must be signed to allow -1 sentinel");
    EXPECT_GE(sizeof(scau::core::Index), sizeof(std::int32_t));
}

TEST(CoreTypes, RealIsDoublePrecisionByDefault) {
    static_assert(std::is_same_v<scau::core::Real, double>,
                  "Real must be double on the CPU reference path");
    EXPECT_EQ(std::numeric_limits<scau::core::Real>::digits, 53);
}

TEST(CoreTypes, Vector2DefaultsToZero) {
    scau::core::Vector2 v{};
    EXPECT_DOUBLE_EQ(v.x, 0.0);
    EXPECT_DOUBLE_EQ(v.y, 0.0);
}

TEST(CoreTypes, Vector2BraceInit) {
    scau::core::Vector2 v{1.5, -2.25};
    EXPECT_DOUBLE_EQ(v.x, 1.5);
    EXPECT_DOUBLE_EQ(v.y, -2.25);
}

}  // namespace
```

- [ ] **Step 2: 写 `tests/unit/core/CMakeLists.txt`**

```cmake
add_executable(test_core_types test_types.cpp)
target_link_libraries(test_core_types
    PRIVATE
        scau::core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_core_types COMMAND test_core_types)
```

- [ ] **Step 3: 写 `tests/CMakeLists.txt`**

```cmake
find_package(GTest CONFIG REQUIRED)

add_subdirectory(unit/core)
```

- [ ] **Step 4: 配置（仍预期 fail，因为 types.hpp 未创建）**

Linux:
```bash
cmake --preset linux-gcc
```
Windows:
```powershell
cmake --preset windows-msvc
```
Expected: configure 成功，build 时 `test_types.cpp` 找不到 `core/types.hpp`。

- [ ] **Step 5: 写最小实现 `libs/core/include/core/types.hpp`**

```cpp
#pragma once

#include <cstdint>

namespace scau::core {

using Index = std::int32_t;
using Real = double;

struct Vector2 {
    Real x{0.0};
    Real y{0.0};
};

}  // namespace scau::core
```

- [ ] **Step 6: 重新构建并测试**

Linux:
```bash
cmake --build --preset linux-gcc --target test_core_types
ctest --preset linux-gcc -R test_core_types
```
Windows:
```powershell
cmake --build --preset windows-msvc --target test_core_types
ctest --preset windows-msvc -R test_core_types
```
Expected: 4/4 测试通过。

- [ ] **Step 7: 提交**

```bash
git add libs/core/include/core/types.hpp \
        tests/unit/core/test_types.cpp \
        tests/unit/core/CMakeLists.txt \
        tests/CMakeLists.txt
git commit -m "feat(core): add Index/Real/Vector2 with unit tests"
```

---

## Task 8: TDD `core::ScauError` 与 `SCAU_ASSERT`

**Files:**
- Create: `tests/unit/core/test_error.cpp`
- Modify: `tests/unit/core/CMakeLists.txt`
- Create: `libs/core/include/core/error.hpp`
- Create: `libs/core/src/error.cpp`

- [ ] **Step 1: 写失败测试 `test_error.cpp`**

```cpp
#include <gtest/gtest.h>
#include <core/error.hpp>

#include <string>

namespace {

TEST(CoreError, ScauErrorCarriesMessage) {
    try {
        throw scau::core::ScauError("expected message");
    } catch (const scau::core::ScauError& e) {
        EXPECT_STREQ(e.what(), "expected message");
    } catch (...) {
        FAIL() << "Wrong exception type";
    }
}

TEST(CoreError, ScauErrorIsStdException) {
    scau::core::ScauError err("x");
    const std::exception* base = &err;
    EXPECT_NE(base, nullptr);
}

TEST(CoreError, AssertPassesWhenTrue) {
    EXPECT_NO_THROW(SCAU_ASSERT(1 + 1 == 2, "math is broken"));
}

TEST(CoreError, AssertThrowsWhenFalse) {
    EXPECT_THROW(SCAU_ASSERT(false, "intentional failure"),
                 scau::core::ScauError);
}

TEST(CoreError, AssertMessageContainsHint) {
    try {
        SCAU_ASSERT(false, "trigger reason");
        FAIL() << "should have thrown";
    } catch (const scau::core::ScauError& e) {
        std::string msg(e.what());
        EXPECT_NE(msg.find("trigger reason"), std::string::npos);
    }
}

}  // namespace
```

- [ ] **Step 2: 注册测试目标，修改 `tests/unit/core/CMakeLists.txt`**

新内容（替换 Task 7 留下的版本）：

```cmake
add_executable(test_core_types test_types.cpp)
target_link_libraries(test_core_types
    PRIVATE
        scau::core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_core_types COMMAND test_core_types)

add_executable(test_core_error test_error.cpp)
target_link_libraries(test_core_error
    PRIVATE
        scau::core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_core_error COMMAND test_core_error)
```

- [ ] **Step 3: 配置 + 构建（预期 build fail，因为 error.hpp 不存在）**

Linux:
```bash
cmake --build --preset linux-gcc --target test_core_error 2>&1 | tail -20
```

Expected: build error: `core/error.hpp: No such file or directory`

- [ ] **Step 4: 写 `libs/core/include/core/error.hpp`**

```cpp
#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

namespace scau::core {

class ScauError : public std::runtime_error {
public:
    explicit ScauError(const std::string& msg) : std::runtime_error(msg) {}
    explicit ScauError(const char* msg) : std::runtime_error(msg) {}
};

[[noreturn]] void throw_assert_failure(std::string_view condition,
                                       std::string_view message,
                                       const char* file,
                                       int line);

}  // namespace scau::core

#define SCAU_ASSERT(cond, msg)                                                \
    do {                                                                       \
        if (!(cond)) {                                                         \
            ::scau::core::throw_assert_failure(#cond, (msg), __FILE__,         \
                                                __LINE__);                     \
        }                                                                      \
    } while (false)
```

- [ ] **Step 5: 写 `libs/core/src/error.cpp`**

```cpp
#include <core/error.hpp>

#include <sstream>

namespace scau::core {

void throw_assert_failure(std::string_view condition,
                          std::string_view message,
                          const char* file,
                          int line) {
    std::ostringstream os;
    os << "SCAU_ASSERT failed: " << condition
       << " | " << message
       << " | at " << file << ':' << line;
    throw ScauError(os.str());
}

}  // namespace scau::core
```

- [ ] **Step 6: 构建并测试**

Linux:
```bash
cmake --build --preset linux-gcc --target test_core_types test_core_error
ctest --preset linux-gcc
```
Windows:
```powershell
cmake --build --preset windows-msvc --target test_core_types test_core_error
ctest --preset windows-msvc
```
Expected: 9/9 测试通过（4 来自 types，5 来自 error）。

- [ ] **Step 7: 提交**

```bash
git add libs/core/include/core/error.hpp \
        libs/core/src/error.cpp \
        tests/unit/core/test_error.cpp \
        tests/unit/core/CMakeLists.txt
git commit -m "feat(core): add ScauError and SCAU_ASSERT with unit tests"
```

---

## Task 9: 端到端冷启动构建验证

**Files:**
- 无新文件；只验证从干净仓库到全测试通过的端到端路径

- [ ] **Step 1: Linux 冷启动验证**

```bash
rm -rf build vcpkg_installed
cmake --preset linux-gcc
cmake --build --preset linux-gcc
ctest --preset linux-gcc
```
Expected:
- configure 成功；vcpkg 拉取并构建 GoogleTest（首次约 2–4 分钟）
- 全部目标 build 成功
- ctest 输出 `100% tests passed, 0 tests failed out of 2`（test_core_types + test_core_error）

- [ ] **Step 2: Windows 冷启动验证**

```powershell
Remove-Item -Recurse -Force build, vcpkg_installed -ErrorAction SilentlyContinue
cmake --preset windows-msvc
cmake --build --preset windows-msvc
ctest --preset windows-msvc
```
Expected: 同 Linux，全 2 项测试通过。

- [ ] **Step 3: 若两个平台均通过，进入下一任务；若任一失败，回到失败的 Task 修复后再重跑本任务**

---

## Task 10: GitHub Actions CI matrix

**Files:**
- Create: `.github/workflows/ci.yml`

- [ ] **Step 1: 写 CI workflow**

```yaml
name: CI

on:
  push:
    branches: [main, master]
  pull_request:
    branches: [main, master]

env:
  VCPKG_ROOT: ${{ github.workspace }}/vcpkg
  VCPKG_DEFAULT_BINARY_CACHE: ${{ github.workspace }}/vcpkg-cache

jobs:
  build-and-test:
    name: ${{ matrix.preset }}
    runs-on: ${{ matrix.os }}
    strategy:
      fail-fast: false
      matrix:
        include:
          - os: ubuntu-22.04
            preset: linux-gcc
            triplet: x64-linux
          - os: windows-2022
            preset: windows-msvc
            triplet: x64-windows

    steps:
      - name: Checkout
        uses: actions/checkout@v4

      - name: Install Linux deps
        if: runner.os == 'Linux'
        run: |
          sudo apt-get update
          sudo apt-get install -y ninja-build cmake build-essential pkg-config

      - name: Install Ninja (Windows)
        if: runner.os == 'Windows'
        uses: seanmiddleditch/gha-setup-ninja@v4

      - name: Bootstrap vcpkg
        shell: bash
        run: |
          mkdir -p "$VCPKG_DEFAULT_BINARY_CACHE"
          if [ ! -d "$VCPKG_ROOT" ]; then
            git clone https://github.com/microsoft/vcpkg.git "$VCPKG_ROOT"
          fi
          if [ "${{ runner.os }}" = "Windows" ]; then
            "$VCPKG_ROOT/bootstrap-vcpkg.bat" -disableMetrics
          else
            "$VCPKG_ROOT/bootstrap-vcpkg.sh" -disableMetrics
          fi

      - name: Cache vcpkg binary cache
        uses: actions/cache@v4
        with:
          path: ${{ env.VCPKG_DEFAULT_BINARY_CACHE }}
          key: vcpkg-${{ runner.os }}-${{ hashFiles('vcpkg.json') }}
          restore-keys: |
            vcpkg-${{ runner.os }}-

      - name: Configure
        run: cmake --preset ${{ matrix.preset }}

      - name: Build
        run: cmake --build --preset ${{ matrix.preset }}

      - name: Test
        run: ctest --preset ${{ matrix.preset }}
```

- [ ] **Step 2: 本地 yaml lint（可选但建议）**

如果安装了 `actionlint`：
```bash
actionlint .github/workflows/ci.yml
```
Expected: 无输出（无 issue）。

- [ ] **Step 3: 提交**

```bash
git add .github/workflows/ci.yml
git commit -m "ci: add GitHub Actions matrix for windows-msvc and linux-gcc"
```

- [ ] **Step 4: 推送并验证**

```bash
git push
```

到 GitHub repo Actions 标签页观察 workflow run。Expected:
- 两个 matrix job 都跑完
- 首次冷启动 ≤ 12 分钟
- 全 2 个测试通过

若失败，查看日志、回到失败任务修复。

---

## Task 11: README.md 构建说明

**Files:**
- Create or replace: `README.md`

- [ ] **Step 1: 写 README.md**

````markdown
# SCAU-UFM

SCAU urban flood model. C++20 numerical core (Anisotropic DPM HLLC) with embedded SWMM and D-Flow FM 1D engines, fully coupled by `CouplingLib`.

This repository is currently at **Milestone M1 (build chain + scaffolding)**. Subsequent milestones (M2 mesh+stcf, M3 surface2d, M4 coupling, M5 release gate) deliver actual numerics. See `superpowers/INDEX.md` for the authoritative documentation tree.

## Prerequisites

- CMake ≥ 3.27
- A C++20 compiler:
  - Linux: GCC ≥ 12 or Clang ≥ 14
  - Windows: Visual Studio 2022 (MSVC ≥ 19.32)
- Ninja (Linux) or the Visual Studio 17 2022 generator (Windows)
- A local clone of [vcpkg](https://github.com/microsoft/vcpkg); the path must be exported as `VCPKG_ROOT` before running CMake.

## Build & Test

Linux:
```bash
export VCPKG_ROOT=/path/to/vcpkg
cmake --preset linux-gcc
cmake --build --preset linux-gcc
ctest --preset linux-gcc
```

Windows (PowerShell):
```powershell
$env:VCPKG_ROOT = "C:\path\to\vcpkg"
cmake --preset windows-msvc
cmake --build --preset windows-msvc
ctest --preset windows-msvc
```

Both presets pass `SCAU_WARNINGS_AS_ERRORS=ON`. Override locally with `-DSCAU_WARNINGS_AS_ERRORS=OFF` while iterating.

## Documentation

- Authoritative entries: `superpowers/INDEX.md`
- Main spec: `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md`
- Stability protocol: `superpowers/specs/2026-04-14-scau-ufm-stability-reliability-protocol.md`
- Symbols & terms: `superpowers/specs/2026-04-22-symbols-and-terms-reference.md`
- Project layout design: `superpowers/specs/project-layout-design.md`

## License

TBD.
````

- [ ] **Step 2: 提交**

```bash
git add README.md
git commit -m "docs: add README with build & test instructions"
```

---

## Task 12: M1 退出验收清单

**Files:**
- Create: `superpowers/specs/2026-05-09-plan-m1-build-chain-evidence.md`

- [ ] **Step 1: 写 evidence 文档**

````markdown
# Plan-M1 退出证据

**日期**: <填实际完成日期>
**对应实施计划**: `superpowers/plans/2026-05-09-plan-m1-build-chain.md`

## 1. 退出标准核对

| 标准 | 证据 |
|---|---|
| 干净 Linux 环境 (Ubuntu 22.04 + GCC 12) 全流程通过 | `<填本地构建日志路径或 GitHub Actions run URL>` |
| 干净 Windows 环境 (VS 2022) 全流程通过 | `<同上>` |
| GitHub Actions matrix 通过且冷启动 ≤ 12 分钟 / 缓存后 ≤ 4 分钟 | `<run URL + duration>` |
| `core::Index` / `core::Real` / `core::Vector2` / `core::ScauError` / `SCAU_ASSERT` 全部有单元测试 | `tests/unit/core/test_types.cpp` / `tests/unit/core/test_error.cpp`，共 9 个测试 |
| README 含可复制构建测试指令 | `README.md`（Build & Test 节） |

## 2. 已知风险与下一步

- M1 不引入任何 1D 引擎依赖；spike A 与 sandbox B 仍在并行进行，结论尚未回写。
- M2 入口前提：本 Plan-M1 全部退出标准达成 + sandbox B mesh.py 原型可用。
- M3 入口前提：M2 退出 + sandbox B 全部退出标准达成。

## 3. 归档

本文件作为 M1 完成的可审计证据归入 `superpowers/specs/`；M2 plan 撰写时引用本文件作为入口前提。
````

- [ ] **Step 2: 提交**

```bash
git add superpowers/specs/2026-05-09-plan-m1-build-chain-evidence.md
git commit -m "docs(m1): add exit evidence template"
```

---

## Task 13: M1 完成 tag

**Files:**
- 无新文件；打 git tag

- [ ] **Step 1: 打 tag**

```bash
git tag -a m1-build-chain -m "Milestone M1: build chain + scaffolding complete"
git push origin m1-build-chain
```

- [ ] **Step 2: 在 GitHub Releases 创建对应 release（可选但建议），描述 M1 退出证据文件链接**

---

## 自审 (Self-Review) 结论

- **Spec coverage:** Plan-M1 的目标对应 `project-layout-design.md §6.1` 中 `libs/core/`、`apps/sim_driver/` 之外其他 Phase 1 必有最小实现目录的子集。M1 只覆盖最底层 `libs/core/` + 工程脚手架；其余目录（`libs/mesh/`、`libs/stcf/`、`libs/surface2d/*`、`libs/coupling/*`、`libs/io/`、`apps/sim_driver/`、`configs/`、`tests/numerical/`、`tests/golden/*`）按里程碑顺序在 M2–M5 实施。
- **Placeholder scan:** evidence 模板 Task 12 中保留了"<填实际完成日期>"等占位，是设计上让 evidence 在 M1 真正完成时填写，不属于计划本体的 placeholder。计划本体的所有任务步骤均含完整代码与命令。
- **Type consistency:** `Index` (`std::int32_t`)、`Real` (`double`)、`Vector2` (struct {x, y})、`ScauError` (extends std::runtime_error)、`SCAU_ASSERT` (宏) 在 Task 7、Task 8 中名称、签名、命名空间一致。
- **退出标准 vs 任务覆盖:**
  - 双平台冷启动通过 ← Task 9 + Task 10
  - CI 通过 ← Task 10
  - 5 项 API 全测试覆盖 ← Task 7 (3 项) + Task 8 (2 项)
  - README ← Task 11

---

## 任务清单总览

- [ ] Task 1: 仓库根 `.gitignore`
- [ ] Task 2: `vcpkg.json` manifest
- [ ] Task 3: 顶层 `CMakeLists.txt` 骨架
- [ ] Task 4: `cmake/modules/ScauWarnings.cmake`
- [ ] Task 5: `CMakePresets.json`
- [ ] Task 6: `libs/core/CMakeLists.txt` target
- [ ] Task 7: TDD `core::Index` / `Real` / `Vector2`
- [ ] Task 8: TDD `core::ScauError` 与 `SCAU_ASSERT`
- [ ] Task 9: 端到端冷启动验证
- [ ] Task 10: GitHub Actions CI matrix
- [ ] Task 11: `README.md` 构建说明
- [ ] Task 12: M1 退出证据
- [ ] Task 13: M1 完成 tag

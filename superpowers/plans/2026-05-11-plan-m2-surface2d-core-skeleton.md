# Plan-M2: 最小 2D Surface2DCore 骨架 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 M1 构建链上加入可编译、可测试的最小 `libs/mesh/` 与 `libs/surface2d/` C++20 骨架，为后续 DPM-HLLC 数值推进提供不依赖 1D 引擎的 2D surface core 母床。

**Architecture:** `libs/mesh/` 承载混合三角形/四边形拓扑、几何量和邻接验证，直接复刻 sandbox B `mesh.py` 已验证的最小能力；`libs/surface2d/` 只承载状态、配置、诊断和 CPU backend 的空推进骨架，消费 `scau::mesh` 但不实现正式 DPM、HLLC 或 source terms；根 CMake 继续按 `libs/<module>` + `tests/unit/<module>` 方式串接。

**Tech Stack:** CMake 3.27+ / C++20 / GoogleTest / `scau::core` / `scau::warnings` / Windows MSVC preset / Linux GCC preset

**入口前提:**
- M1 构建链已完成：`CMakeLists.txt`、`CMakePresets.json`、`vcpkg.json`、`libs/core/`、GoogleTest 与 CI matrix 可用。
- sandbox B 的 `mesh.py` 原型已可用，并已归档证据到 `sandbox/numerical/evidence/mesh_prototype.md`。
- sandbox B 完整退出标准尚未完成；因此本 M2 不得正式实现 `libs/surface2d/dpm/`、`libs/surface2d/riemann/`、`libs/surface2d/source_terms/` 的数值算法。

**退出标准:**
- `libs/mesh/` 暴露 `Node` / `Cell` / `EdgeSpec` / `Edge` / `Mesh` / `build_mesh()` / `cell_area()` / `cell_center()`，支持 `CellType::Triangle` 与 `CellType::Quadrilateral`。
- mesh 单元测试覆盖 mixed minimal mesh、quad-only/tri-only control mesh、面积、中心、边长、边中点、边法向、邻接、非法 edge specs。
- `libs/surface2d/` 暴露最小状态、推进配置、诊断和 CPU backend 空推进 API，且只依赖 `scau::mesh` / `scau::core`，不依赖 SWMM、D-Flow FM 或 `CouplingLib`。
- `cmake --build --preset windows-msvc` 与 `ctest --preset windows-msvc` 本地通过；CI matrix 继续通过。

---

## File Structure

| 文件 | 责任 |
|---|---|
| `CMakeLists.txt` | 新增 `add_subdirectory(libs/mesh)` 与 `add_subdirectory(libs/surface2d)`，顺序位于 `libs/core` 后、`tests` 前。 |
| `libs/mesh/CMakeLists.txt` | 定义 `scau_mesh` static library 与 alias `scau::mesh`。 |
| `libs/mesh/include/mesh/mesh.hpp` | 混合网格公共 API：拓扑类型、几何函数、mesh 构建入口。 |
| `libs/mesh/src/mesh.cpp` | mesh 几何、法向、邻接和输入验证实现。 |
| `libs/surface2d/CMakeLists.txt` | 定义 `scau_surface2d` static library 与 alias `scau::surface2d`。 |
| `libs/surface2d/include/surface2d/state/state.hpp` | 最小守恒状态与 mesh 对齐状态容器。 |
| `libs/surface2d/include/surface2d/time_integration/step.hpp` | 单步推进配置、诊断与 CPU backend 函数声明。 |
| `libs/surface2d/src/time_integration/step.cpp` | CPU backend 空推进实现：保持状态不变并返回诊断。 |
| `tests/CMakeLists.txt` | 新增 `add_subdirectory(unit/mesh)` 与 `add_subdirectory(unit/surface2d)`。 |
| `tests/unit/mesh/CMakeLists.txt` | 注册 mesh 单元测试。 |
| `tests/unit/mesh/test_mesh_geometry.cpp` | 验证 mixed/quad/tri mesh 几何和拓扑。 |
| `tests/unit/mesh/test_mesh_validation.cpp` | 验证非法 mesh / edge specs 被拒绝。 |
| `tests/unit/surface2d/CMakeLists.txt` | 注册 surface2d 单元测试。 |
| `tests/unit/surface2d/test_surface_state.cpp` | 验证 surface state 与 mesh cell 数对齐。 |
| `tests/unit/surface2d/test_step.cpp` | 验证 CPU 空推进保持状态并填充诊断。 |
| `superpowers/specs/2026-05-11-plan-m2-surface2d-core-skeleton-evidence.md` | M2 退出证据，记录本地命令与 CI 结果。 |

---

## Task 1: Mesh CMake Target

**Files:**
- Create: `libs/mesh/CMakeLists.txt`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: 写失败期望**

先确认当前构建还没有 `scau::mesh` target。后续 Task 2 才会加入源文件，因此本任务只串接目录，不单独运行构建。

- [ ] **Step 2: 创建 `libs/mesh/CMakeLists.txt`**

```cmake
add_library(scau_mesh
    src/mesh.cpp
)
add_library(scau::mesh ALIAS scau_mesh)

target_include_directories(scau_mesh
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(scau_mesh
    PUBLIC
        scau::core
    PRIVATE
        scau::warnings
)

target_compile_features(scau_mesh PUBLIC cxx_std_20)
```

- [ ] **Step 3: 修改根 `CMakeLists.txt`**

把末尾从：

```cmake
add_subdirectory(libs/core)
add_subdirectory(tests)
```

改为：

```cmake
add_subdirectory(libs/core)
add_subdirectory(libs/mesh)
add_subdirectory(tests)
```

- [ ] **Step 4: 提交**

```bash
git add CMakeLists.txt libs/mesh/CMakeLists.txt
git commit -m "build: add mesh library target"
```

---

## Task 2: Mesh Topology And Geometry API

**Files:**
- Create: `libs/mesh/include/mesh/mesh.hpp`
- Create: `libs/mesh/src/mesh.cpp`
- Create: `tests/unit/mesh/CMakeLists.txt`
- Create: `tests/unit/mesh/test_mesh_geometry.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: 写 mesh geometry 测试**

创建 `tests/unit/mesh/test_mesh_geometry.cpp`：

```cpp
#include <cmath>
#include <map>
#include <string>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"

namespace {

void expect_near(scau::core::Real actual, scau::core::Real expected) {
    EXPECT_NEAR(actual, expected, 1.0e-15);
}

std::map<std::string, scau::mesh::Edge> edge_map(const scau::mesh::Mesh& mesh) {
    std::map<std::string, scau::mesh::Edge> edges;
    for (const auto& edge : mesh.edges) {
        edges.emplace(edge.id, edge);
    }
    return edges;
}

}  // namespace

TEST(MeshGeometry, BuildsMixedMinimalMesh) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();

    ASSERT_EQ(mesh.nodes.size(), 6U);
    ASSERT_EQ(mesh.cells.size(), 3U);
    EXPECT_EQ(mesh.cells[0].cell_type, scau::mesh::CellType::Quadrilateral);
    EXPECT_EQ(mesh.cells[1].cell_type, scau::mesh::CellType::Triangle);
    EXPECT_EQ(mesh.cells[2].cell_type, scau::mesh::CellType::Triangle);

    const auto edges = edge_map(mesh);
    ASSERT_EQ(edges.size(), 8U);

    EXPECT_EQ(edges.at("E0").node_ids, (std::array<std::string, 2>{"N0", "N1"}));
    EXPECT_EQ(edges.at("E0").left_cell, std::nullopt);
    EXPECT_EQ(edges.at("E0").right_cell, std::optional<std::string>{"C0"});
    expect_near(edges.at("E0").normal.x, 0.0);
    expect_near(edges.at("E0").normal.y, 1.0);

    EXPECT_EQ(edges.at("E1").node_ids, (std::array<std::string, 2>{"N1", "N4"}));
    EXPECT_EQ(edges.at("E1").left_cell, std::optional<std::string>{"C0"});
    EXPECT_EQ(edges.at("E1").right_cell, std::optional<std::string>{"C1"});
    expect_near(edges.at("E1").normal.x, 1.0);
    expect_near(edges.at("E1").normal.y, 0.0);

    EXPECT_EQ(edges.at("E3").node_ids, (std::array<std::string, 2>{"N2", "N4"}));
    EXPECT_EQ(edges.at("E3").left_cell, std::optional<std::string>{"C1"});
    EXPECT_EQ(edges.at("E3").right_cell, std::optional<std::string>{"C2"});
    expect_near(edges.at("E3").normal.x, std::sqrt(0.5));
    expect_near(edges.at("E3").normal.y, std::sqrt(0.5));

    EXPECT_EQ(edges.at("E6").node_ids, (std::array<std::string, 2>{"N2", "N5"}));
    EXPECT_EQ(edges.at("E6").left_cell, std::nullopt);
    EXPECT_EQ(edges.at("E6").right_cell, std::optional<std::string>{"C2"});
    expect_near(edges.at("E6").normal.x, -1.0);
    expect_near(edges.at("E6").normal.y, 0.0);
}

TEST(MeshGeometry, ComputesAreasCentersLengthsAndMidpoints) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto nodes = scau::mesh::node_lookup(mesh.nodes);
    const auto edges = edge_map(mesh);

    expect_near(scau::mesh::cell_area(mesh.cells[0], nodes), 1.0);
    expect_near(scau::mesh::cell_area(mesh.cells[1], nodes), 0.5);
    expect_near(scau::mesh::cell_area(mesh.cells[2], nodes), 0.5);

    const auto c0 = scau::mesh::cell_center(mesh.cells[0], nodes);
    expect_near(c0.x, 0.5);
    expect_near(c0.y, 0.5);

    const auto c1 = scau::mesh::cell_center(mesh.cells[1], nodes);
    expect_near(c1.x, 4.0 / 3.0);
    expect_near(c1.y, 1.0 / 3.0);

    expect_near(edges.at("E3").length, std::sqrt(2.0));
    expect_near(edges.at("E3").midpoint.x, 1.5);
    expect_near(edges.at("E3").midpoint.y, 0.5);
}

TEST(MeshGeometry, BuildsControlMeshes) {
    EXPECT_EQ(scau::mesh::build_quad_only_control_mesh().cells.size(), 2U);
    EXPECT_EQ(scau::mesh::build_tri_only_control_mesh().cells.size(), 4U);
}
```

- [ ] **Step 2: 注册 mesh 测试并确认失败**

创建 `tests/unit/mesh/CMakeLists.txt`：

```cmake
add_executable(test_mesh_geometry test_mesh_geometry.cpp)
target_link_libraries(test_mesh_geometry
    PRIVATE
        scau::mesh
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_mesh_geometry COMMAND test_mesh_geometry)
```

修改 `tests/CMakeLists.txt`，从：

```cmake
find_package(GTest CONFIG REQUIRED)

add_subdirectory(unit/core)
```

改为：

```cmake
find_package(GTest CONFIG REQUIRED)

add_subdirectory(unit/core)
add_subdirectory(unit/mesh)
```

运行：

```bash
cmake --build --preset windows-msvc --target test_mesh_geometry
```

Expected: FAIL，原因是 `mesh/mesh.hpp` 与 `libs/mesh/src/mesh.cpp` 尚未存在。

- [ ] **Step 3: 写 mesh 公共 API**

创建 `libs/mesh/include/mesh/mesh.hpp`：

```cpp
#pragma once

#include <array>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/types.hpp"

namespace scau::mesh {

enum class CellType {
    Triangle,
    Quadrilateral,
};

struct Node {
    std::string id;
    core::Real x{0.0};
    core::Real y{0.0};
};

struct Cell {
    std::string id;
    CellType cell_type{CellType::Triangle};
    std::vector<std::string> node_ids;

    [[nodiscard]] std::size_t node_count() const noexcept;
    [[nodiscard]] std::size_t edge_count() const noexcept;
};

struct Edge {
    std::string id;
    std::array<std::string, 2> node_ids;
    std::optional<std::string> left_cell;
    std::optional<std::string> right_cell;
    core::Vector2 normal;
    core::Real length{0.0};
    core::Vector2 midpoint;
};

struct EdgeSpec {
    std::string id;
    std::array<std::string, 2> node_ids;
    std::optional<std::string> left_cell;
    std::optional<std::string> right_cell;
};

struct Mesh {
    std::vector<Node> nodes;
    std::vector<Cell> cells;
    std::vector<Edge> edges;
};

using NodeLookup = std::unordered_map<std::string, Node>;

[[nodiscard]] NodeLookup node_lookup(const std::vector<Node>& nodes);
[[nodiscard]] Mesh build_mesh(std::vector<Node> nodes, std::vector<Cell> cells);
[[nodiscard]] Mesh build_mesh(std::vector<Node> nodes, std::vector<Cell> cells, std::vector<EdgeSpec> edge_specs);
[[nodiscard]] Mesh build_mixed_minimal_mesh();
[[nodiscard]] Mesh build_quad_only_control_mesh();
[[nodiscard]] Mesh build_tri_only_control_mesh();
[[nodiscard]] core::Real cell_area(const Cell& cell, const NodeLookup& nodes);
[[nodiscard]] core::Vector2 cell_center(const Cell& cell, const NodeLookup& nodes);

}  // namespace scau::mesh
```

- [ ] **Step 4: 写 mesh 实现**

创建 `libs/mesh/src/mesh.cpp`：

```cpp
#include "mesh/mesh.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
#include <utility>

namespace scau::mesh {
namespace {

using EdgeKey = std::set<std::string>;
using Owner = std::pair<std::string, std::array<std::string, 2>>;
using EdgeOwners = std::unordered_map<std::string, std::vector<Owner>>;
using CellLookup = std::unordered_map<std::string, Cell>;

std::string edge_key(const std::array<std::string, 2>& node_ids) {
    if (node_ids[0] < node_ids[1]) {
        return node_ids[0] + "|" + node_ids[1];
    }
    return node_ids[1] + "|" + node_ids[0];
}

CellLookup cell_lookup(const std::vector<Cell>& cells) {
    CellLookup lookup;
    for (const auto& cell : cells) {
        lookup.emplace(cell.id, cell);
    }
    return lookup;
}

void validate_cells(const std::vector<Cell>& cells, const NodeLookup& nodes) {
    for (const auto& cell : cells) {
        if (cell.cell_type == CellType::Triangle && cell.node_count() != 3U) {
            throw std::invalid_argument("triangle " + cell.id + " must have 3 nodes");
        }
        if (cell.cell_type == CellType::Quadrilateral && cell.node_count() != 4U) {
            throw std::invalid_argument("quadrilateral " + cell.id + " must have 4 nodes");
        }
        std::set<std::string> unique_nodes(cell.node_ids.begin(), cell.node_ids.end());
        if (unique_nodes.size() != cell.node_count()) {
            throw std::invalid_argument("cell " + cell.id + " has duplicate nodes");
        }
        for (const auto& node_id : cell.node_ids) {
            if (!nodes.contains(node_id)) {
                throw std::invalid_argument("cell " + cell.id + " references unknown node " + node_id);
            }
        }
        if (cell_area(cell, nodes) == 0.0) {
            throw std::invalid_argument("cell " + cell.id + " has zero area");
        }
    }
}

EdgeOwners collect_edge_owners(const std::vector<Cell>& cells) {
    EdgeOwners edge_owners;
    for (const auto& cell : cells) {
        for (std::size_t index = 0; index < cell.edge_count(); ++index) {
            const std::array<std::string, 2> node_ids{
                cell.node_ids[index],
                cell.node_ids[(index + 1U) % cell.edge_count()],
            };
            edge_owners[edge_key(node_ids)].push_back({cell.id, node_ids});
        }
    }
    return edge_owners;
}

std::vector<EdgeSpec> default_edge_specs(const EdgeOwners& edge_owners) {
    std::vector<EdgeSpec> specs;
    specs.reserve(edge_owners.size());
    std::size_t index = 0;
    for (const auto& [key, owners] : edge_owners) {
        if (owners.size() > 2U) {
            throw std::invalid_argument("edge " + key + " has more than two adjacent cells");
        }
        specs.push_back(EdgeSpec{
            "E" + std::to_string(index),
            owners[0].second,
            owners[0].first,
            owners.size() == 2U ? std::optional<std::string>{owners[1].first} : std::nullopt,
        });
        ++index;
    }
    return specs;
}

void validate_edge_specs(const std::vector<EdgeSpec>& specs, const EdgeOwners& edge_owners) {
    std::set<std::string> ids;
    std::set<std::string> topology;
    for (const auto& spec : specs) {
        if (!ids.insert(spec.id).second) {
            throw std::invalid_argument("edge specs contain duplicate ids");
        }
        if (!topology.insert(edge_key(spec.node_ids)).second) {
            throw std::invalid_argument("edge specs contain duplicate topological edges");
        }
    }
    std::set<std::string> expected_topology;
    for (const auto& [key, owners] : edge_owners) {
        expected_topology.insert(key);
    }
    if (topology != expected_topology) {
        throw std::invalid_argument("edge specs do not cover the cell topology exactly");
    }
    for (const auto& spec : specs) {
        const auto owners = edge_owners.at(edge_key(spec.node_ids));
        if (owners.size() > 2U) {
            throw std::invalid_argument("edge " + spec.id + " has more than two adjacent cells");
        }
        std::set<std::string> owner_cells;
        for (const auto& [cell_id, orientation] : owners) {
            owner_cells.insert(cell_id);
        }
        std::set<std::string> expected_cells;
        if (spec.left_cell.has_value()) {
            expected_cells.insert(*spec.left_cell);
        }
        if (spec.right_cell.has_value()) {
            expected_cells.insert(*spec.right_cell);
        }
        if (owner_cells != expected_cells) {
            throw std::invalid_argument("edge " + spec.id + " adjacency does not match cell topology");
        }
        if (spec.left_cell.has_value()) {
            const auto left_owner = std::find_if(owners.begin(), owners.end(), [&](const Owner& owner) {
                return owner.first == *spec.left_cell;
            });
            if (left_owner == owners.end() || left_owner->second != spec.node_ids) {
                throw std::invalid_argument("edge " + spec.id + " direction must match the left cell orientation");
            }
        }
    }
}

core::Real cell_side(
    const std::string& cell_id,
    const CellLookup& cells,
    const NodeLookup& nodes,
    const core::Vector2& midpoint,
    const core::Vector2& normal) {
    const auto center = cell_center(cells.at(cell_id), nodes);
    return ((center.x - midpoint.x) * normal.x) + ((center.y - midpoint.y) * normal.y);
}

core::Vector2 negate(core::Vector2 value) {
    return core::Vector2{.x = -value.x, .y = -value.y};
}

core::Vector2 normal_toward_cell(
    const std::optional<std::string>& cell_id,
    const CellLookup& cells,
    const NodeLookup& nodes,
    const core::Vector2& midpoint,
    core::Vector2 normal) {
    if (!cell_id.has_value()) {
        throw std::invalid_argument("boundary edge must reference one cell");
    }
    if (cell_side(*cell_id, cells, nodes, midpoint, normal) < 0.0) {
        return negate(normal);
    }
    return normal;
}

core::Vector2 normal_away_from_cell(
    const std::string& cell_id,
    const CellLookup& cells,
    const NodeLookup& nodes,
    const core::Vector2& midpoint,
    core::Vector2 normal) {
    if (cell_side(cell_id, cells, nodes, midpoint, normal) > 0.0) {
        return negate(normal);
    }
    return normal;
}

core::Vector2 normal_from_cell_to_cell(
    const std::string& left_cell,
    const std::string& right_cell,
    const CellLookup& cells,
    const NodeLookup& nodes,
    const core::Vector2& midpoint,
    core::Vector2 normal) {
    if (cell_side(right_cell, cells, nodes, midpoint, normal) < 0.0) {
        normal = negate(normal);
    }
    if (cell_side(left_cell, cells, nodes, midpoint, normal) > 0.0) {
        throw std::invalid_argument("edge normal does not separate left and right cells");
    }
    return normal;
}

core::Vector2 oriented_normal(
    const EdgeSpec& spec,
    const CellLookup& cells,
    const NodeLookup& nodes,
    core::Real length,
    core::Real dx,
    core::Real dy,
    const core::Vector2& midpoint) {
    const core::Vector2 normal{.x = dy / length, .y = -dx / length};
    if (!spec.left_cell.has_value()) {
        return normal_toward_cell(spec.right_cell, cells, nodes, midpoint, normal);
    }
    if (!spec.right_cell.has_value()) {
        return normal_away_from_cell(*spec.left_cell, cells, nodes, midpoint, normal);
    }
    return normal_from_cell_to_cell(*spec.left_cell, *spec.right_cell, cells, nodes, midpoint, normal);
}

std::vector<Edge> build_edges(
    const std::vector<Cell>& cells,
    const NodeLookup& nodes,
    const std::optional<std::vector<EdgeSpec>>& edge_specs) {
    const auto edge_owners = collect_edge_owners(cells);
    const auto specs = edge_specs.has_value() ? *edge_specs : default_edge_specs(edge_owners);
    const auto cells_by_id = cell_lookup(cells);
    validate_edge_specs(specs, edge_owners);

    std::vector<Edge> edges;
    edges.reserve(specs.size());
    for (const auto& spec : specs) {
        const auto& start = nodes.at(spec.node_ids[0]);
        const auto& end = nodes.at(spec.node_ids[1]);
        const auto dx = end.x - start.x;
        const auto dy = end.y - start.y;
        const auto length = std::hypot(dx, dy);
        if (length == 0.0) {
            throw std::invalid_argument("edge " + spec.id + " has zero length");
        }
        const core::Vector2 midpoint{.x = (start.x + end.x) * 0.5, .y = (start.y + end.y) * 0.5};
        edges.push_back(Edge{
            .id = spec.id,
            .node_ids = spec.node_ids,
            .left_cell = spec.left_cell,
            .right_cell = spec.right_cell,
            .normal = oriented_normal(spec, cells_by_id, nodes, length, dx, dy, midpoint),
            .length = length,
            .midpoint = midpoint,
        });
    }
    return edges;
}

std::vector<Node> control_nodes() {
    return {
        Node{.id = "N0", .x = 0.0, .y = 0.0},
        Node{.id = "N1", .x = 1.0, .y = 0.0},
        Node{.id = "N2", .x = 2.0, .y = 0.0},
        Node{.id = "N3", .x = 0.0, .y = 1.0},
        Node{.id = "N4", .x = 1.0, .y = 1.0},
        Node{.id = "N5", .x = 2.0, .y = 1.0},
    };
}

}  // namespace

std::size_t Cell::node_count() const noexcept {
    return node_ids.size();
}

std::size_t Cell::edge_count() const noexcept {
    return node_ids.size();
}

NodeLookup node_lookup(const std::vector<Node>& nodes) {
    NodeLookup lookup;
    for (const auto& node : nodes) {
        lookup.emplace(node.id, node);
    }
    return lookup;
}

Mesh build_mesh(std::vector<Node> nodes, std::vector<Cell> cells) {
    const auto nodes_by_id = node_lookup(nodes);
    validate_cells(cells, nodes_by_id);
    return Mesh{.nodes = std::move(nodes), .cells = std::move(cells), .edges = build_edges(cells, nodes_by_id, std::nullopt)};
}

Mesh build_mesh(std::vector<Node> nodes, std::vector<Cell> cells, std::vector<EdgeSpec> edge_specs) {
    const auto nodes_by_id = node_lookup(nodes);
    validate_cells(cells, nodes_by_id);
    return Mesh{.nodes = std::move(nodes), .cells = std::move(cells), .edges = build_edges(cells, nodes_by_id, std::move(edge_specs))};
}

Mesh build_mixed_minimal_mesh() {
    return build_mesh(
        control_nodes(),
        {
            Cell{.id = "C0", .cell_type = CellType::Quadrilateral, .node_ids = {"N0", "N1", "N4", "N3"}},
            Cell{.id = "C1", .cell_type = CellType::Triangle, .node_ids = {"N1", "N2", "N4"}},
            Cell{.id = "C2", .cell_type = CellType::Triangle, .node_ids = {"N2", "N5", "N4"}},
        },
        {
            EdgeSpec{.id = "E0", .node_ids = {"N0", "N1"}, .left_cell = std::nullopt, .right_cell = "C0"},
            EdgeSpec{.id = "E1", .node_ids = {"N1", "N4"}, .left_cell = "C0", .right_cell = "C1"},
            EdgeSpec{.id = "E2", .node_ids = {"N1", "N2"}, .left_cell = std::nullopt, .right_cell = "C1"},
            EdgeSpec{.id = "E3", .node_ids = {"N2", "N4"}, .left_cell = "C1", .right_cell = "C2"},
            EdgeSpec{.id = "E4", .node_ids = {"N4", "N3"}, .left_cell = "C0", .right_cell = std::nullopt},
            EdgeSpec{.id = "E5", .node_ids = {"N0", "N3"}, .left_cell = std::nullopt, .right_cell = "C0"},
            EdgeSpec{.id = "E6", .node_ids = {"N2", "N5"}, .left_cell = std::nullopt, .right_cell = "C2"},
            EdgeSpec{.id = "E7", .node_ids = {"N5", "N4"}, .left_cell = "C2", .right_cell = std::nullopt},
        });
}

Mesh build_quad_only_control_mesh() {
    return build_mesh(
        control_nodes(),
        {
            Cell{.id = "C0", .cell_type = CellType::Quadrilateral, .node_ids = {"N0", "N1", "N4", "N3"}},
            Cell{.id = "C1", .cell_type = CellType::Quadrilateral, .node_ids = {"N1", "N2", "N5", "N4"}},
        });
}

Mesh build_tri_only_control_mesh() {
    return build_mesh(
        control_nodes(),
        {
            Cell{.id = "C0", .cell_type = CellType::Triangle, .node_ids = {"N0", "N1", "N3"}},
            Cell{.id = "C1", .cell_type = CellType::Triangle, .node_ids = {"N1", "N4", "N3"}},
            Cell{.id = "C2", .cell_type = CellType::Triangle, .node_ids = {"N1", "N2", "N4"}},
            Cell{.id = "C3", .cell_type = CellType::Triangle, .node_ids = {"N2", "N5", "N4"}},
        });
}

core::Real cell_area(const Cell& cell, const NodeLookup& nodes) {
    core::Real twice_area = 0.0;
    for (std::size_t index = 0; index < cell.node_count(); ++index) {
        const auto& current = nodes.at(cell.node_ids[index]);
        const auto& following = nodes.at(cell.node_ids[(index + 1U) % cell.node_count()]);
        twice_area += (current.x * following.y) - (following.x * current.y);
    }
    return std::abs(twice_area) * 0.5;
}

core::Vector2 cell_center(const Cell& cell, const NodeLookup& nodes) {
    core::Real signed_area_twice = 0.0;
    core::Real centroid_x = 0.0;
    core::Real centroid_y = 0.0;
    for (std::size_t index = 0; index < cell.node_count(); ++index) {
        const auto& current = nodes.at(cell.node_ids[index]);
        const auto& following = nodes.at(cell.node_ids[(index + 1U) % cell.node_count()]);
        const auto cross = (current.x * following.y) - (following.x * current.y);
        signed_area_twice += cross;
        centroid_x += (current.x + following.x) * cross;
        centroid_y += (current.y + following.y) * cross;
    }
    if (signed_area_twice == 0.0) {
        throw std::invalid_argument("cell " + cell.id + " has zero area");
    }
    return core::Vector2{.x = centroid_x / (3.0 * signed_area_twice), .y = centroid_y / (3.0 * signed_area_twice)};
}

}  // namespace scau::mesh
```

- [ ] **Step 5: 运行测试并确认通过**

```bash
cmake --build --preset windows-msvc --target test_mesh_geometry
ctest --preset windows-msvc -R test_mesh_geometry
```

Expected: PASS。

- [ ] **Step 6: 提交**

```bash
git add libs/mesh/include/mesh/mesh.hpp libs/mesh/src/mesh.cpp tests/CMakeLists.txt tests/unit/mesh/CMakeLists.txt tests/unit/mesh/test_mesh_geometry.cpp
git commit -m "feat(mesh): add mixed topology geometry skeleton"
```

---

## Task 3: Mesh Validation Coverage

**Files:**
- Create: `tests/unit/mesh/test_mesh_validation.cpp`
- Modify: `tests/unit/mesh/CMakeLists.txt`

- [ ] **Step 1: 写 validation 测试**

创建 `tests/unit/mesh/test_mesh_validation.cpp`：

```cpp
#include <gtest/gtest.h>

#include "mesh/mesh.hpp"

namespace {

std::vector<scau::mesh::Node> triangle_nodes() {
    return {
        scau::mesh::Node{.id = "N0", .x = 0.0, .y = 0.0},
        scau::mesh::Node{.id = "N1", .x = 1.0, .y = 0.0},
        scau::mesh::Node{.id = "N3", .x = 0.0, .y = 1.0},
    };
}

std::vector<scau::mesh::Cell> triangle_cells() {
    return {scau::mesh::Cell{.id = "C0", .cell_type = scau::mesh::CellType::Triangle, .node_ids = {"N0", "N1", "N3"}}};
}

}  // namespace

TEST(MeshValidation, RejectsInvalidCellTopology) {
    EXPECT_THROW(
        scau::mesh::build_mesh(
            triangle_nodes(),
            {scau::mesh::Cell{.id = "C0", .cell_type = scau::mesh::CellType::Triangle, .node_ids = {"N0", "N1", "N3", "N0"}}}),
        std::invalid_argument);

    EXPECT_THROW(
        scau::mesh::build_mesh(
            triangle_nodes(),
            {scau::mesh::Cell{.id = "C0", .cell_type = scau::mesh::CellType::Quadrilateral, .node_ids = {"N0", "N1", "N3"}}}),
        std::invalid_argument);

    EXPECT_THROW(
        scau::mesh::build_mesh(
            triangle_nodes(),
            {scau::mesh::Cell{.id = "C0", .cell_type = scau::mesh::CellType::Triangle, .node_ids = {"N0", "N1", "NX"}}}),
        std::invalid_argument);
}

TEST(MeshValidation, RejectsInvalidEdgeSpecs) {
    EXPECT_THROW(
        scau::mesh::build_mesh(
            triangle_nodes(),
            triangle_cells(),
            {
                scau::mesh::EdgeSpec{.id = "E0", .node_ids = {"N1", "N0"}, .left_cell = "C0", .right_cell = std::nullopt},
                scau::mesh::EdgeSpec{.id = "E1", .node_ids = {"N1", "N3"}, .left_cell = "C0", .right_cell = std::nullopt},
                scau::mesh::EdgeSpec{.id = "E2", .node_ids = {"N3", "N0"}, .left_cell = "C0", .right_cell = std::nullopt},
            }),
        std::invalid_argument);

    EXPECT_THROW(
        scau::mesh::build_mesh(
            triangle_nodes(),
            triangle_cells(),
            {
                scau::mesh::EdgeSpec{.id = "E0", .node_ids = {"N0", "N1"}, .left_cell = "C0", .right_cell = std::nullopt},
                scau::mesh::EdgeSpec{.id = "E0", .node_ids = {"N1", "N3"}, .left_cell = "C0", .right_cell = std::nullopt},
                scau::mesh::EdgeSpec{.id = "E2", .node_ids = {"N3", "N0"}, .left_cell = "C0", .right_cell = std::nullopt},
            }),
        std::invalid_argument);

    EXPECT_THROW(
        scau::mesh::build_mesh(
            triangle_nodes(),
            triangle_cells(),
            {
                scau::mesh::EdgeSpec{.id = "E0", .node_ids = {"N0", "N1"}, .left_cell = "C0", .right_cell = std::nullopt},
                scau::mesh::EdgeSpec{.id = "E1", .node_ids = {"N1", "N3"}, .left_cell = "C0", .right_cell = std::nullopt},
            }),
        std::invalid_argument);

    EXPECT_THROW(
        scau::mesh::build_mesh(
            triangle_nodes(),
            triangle_cells(),
            {
                scau::mesh::EdgeSpec{.id = "E0", .node_ids = {"N0", "N1"}, .left_cell = "C0", .right_cell = std::nullopt},
                scau::mesh::EdgeSpec{.id = "E1", .node_ids = {"N1", "N3"}, .left_cell = "C0", .right_cell = std::nullopt},
                scau::mesh::EdgeSpec{.id = "E2", .node_ids = {"N3", "N0"}, .left_cell = "C0", .right_cell = std::nullopt},
                scau::mesh::EdgeSpec{.id = "E3", .node_ids = {"N0", "N3"}, .left_cell = "C0", .right_cell = std::nullopt},
            }),
        std::invalid_argument);
}
```

- [ ] **Step 2: 注册 validation 测试并确认通过**

修改 `tests/unit/mesh/CMakeLists.txt` 为：

```cmake
add_executable(test_mesh_geometry test_mesh_geometry.cpp)
target_link_libraries(test_mesh_geometry
    PRIVATE
        scau::mesh
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_mesh_geometry COMMAND test_mesh_geometry)

add_executable(test_mesh_validation test_mesh_validation.cpp)
target_link_libraries(test_mesh_validation
    PRIVATE
        scau::mesh
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_mesh_validation COMMAND test_mesh_validation)
```

运行：

```bash
cmake --build --preset windows-msvc --target test_mesh_validation
ctest --preset windows-msvc -R test_mesh_validation
```

Expected: PASS。

- [ ] **Step 3: 提交**

```bash
git add tests/unit/mesh/CMakeLists.txt tests/unit/mesh/test_mesh_validation.cpp
git commit -m "test(mesh): cover mixed topology validation failures"
```

---

## Task 4: Surface2D Library Target

**Files:**
- Create: `libs/surface2d/CMakeLists.txt`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: 写 surface2d target**

创建 `libs/surface2d/CMakeLists.txt`：

```cmake
add_library(scau_surface2d
    src/time_integration/step.cpp
)
add_library(scau::surface2d ALIAS scau_surface2d)

target_include_directories(scau_surface2d
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(scau_surface2d
    PUBLIC
        scau::core
        scau::mesh
    PRIVATE
        scau::warnings
)

target_compile_features(scau_surface2d PUBLIC cxx_std_20)
```

- [ ] **Step 2: 修改根 `CMakeLists.txt`**

把：

```cmake
add_subdirectory(libs/core)
add_subdirectory(libs/mesh)
add_subdirectory(tests)
```

改为：

```cmake
add_subdirectory(libs/core)
add_subdirectory(libs/mesh)
add_subdirectory(libs/surface2d)
add_subdirectory(tests)
```

- [ ] **Step 3: 提交**

```bash
git add CMakeLists.txt libs/surface2d/CMakeLists.txt
git commit -m "build(surface2d): add surface2d library target"
```

---

## Task 5: Surface State Skeleton

**Files:**
- Create: `libs/surface2d/include/surface2d/state/state.hpp`
- Create: `tests/unit/surface2d/CMakeLists.txt`
- Create: `tests/unit/surface2d/test_surface_state.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: 写 surface state 测试**

创建 `tests/unit/surface2d/test_surface_state.cpp`：

```cpp
#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/state/state.hpp"

TEST(SurfaceState, CreatesCellAlignedState) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    const auto state = scau::surface2d::SurfaceState::for_mesh(mesh);

    ASSERT_EQ(state.cells.size(), mesh.cells.size());
    for (const auto& cell_state : state.cells) {
        EXPECT_EQ(cell_state.conserved.h, 0.0);
        EXPECT_EQ(cell_state.conserved.hu, 0.0);
        EXPECT_EQ(cell_state.conserved.hv, 0.0);
    }
}

TEST(SurfaceState, RejectsWrongCellCount) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    scau::surface2d::SurfaceState state;
    state.cells.resize(mesh.cells.size() - 1U);

    EXPECT_THROW(scau::surface2d::validate_state_matches_mesh(state, mesh), std::invalid_argument);
}
```

- [ ] **Step 2: 注册 surface2d state 测试并确认失败**

创建 `tests/unit/surface2d/CMakeLists.txt`：

```cmake
add_executable(test_surface_state test_surface_state.cpp)
target_link_libraries(test_surface_state
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_surface_state COMMAND test_surface_state)
```

修改 `tests/CMakeLists.txt` 为：

```cmake
find_package(GTest CONFIG REQUIRED)

add_subdirectory(unit/core)
add_subdirectory(unit/mesh)
add_subdirectory(unit/surface2d)
```

运行：

```bash
cmake --build --preset windows-msvc --target test_surface_state
```

Expected: FAIL，原因是 `surface2d/state/state.hpp` 尚未存在。

- [ ] **Step 3: 写 surface state header**

创建 `libs/surface2d/include/surface2d/state/state.hpp`：

```cpp
#pragma once

#include <stdexcept>
#include <vector>

#include "core/types.hpp"
#include "mesh/mesh.hpp"

namespace scau::surface2d {

struct ConservedState {
    core::Real h{0.0};
    core::Real hu{0.0};
    core::Real hv{0.0};
};

struct CellState {
    ConservedState conserved;
};

struct SurfaceState {
    std::vector<CellState> cells;

    [[nodiscard]] static SurfaceState for_mesh(const mesh::Mesh& mesh) {
        SurfaceState state;
        state.cells.resize(mesh.cells.size());
        return state;
    }
};

inline void validate_state_matches_mesh(const SurfaceState& state, const mesh::Mesh& mesh) {
    if (state.cells.size() != mesh.cells.size()) {
        throw std::invalid_argument("surface state cell count must match mesh cell count");
    }
}

}  // namespace scau::surface2d
```

- [ ] **Step 4: 运行测试并确认通过**

```bash
cmake --build --preset windows-msvc --target test_surface_state
ctest --preset windows-msvc -R test_surface_state
```

Expected: PASS。

- [ ] **Step 5: 提交**

```bash
git add libs/surface2d/include/surface2d/state/state.hpp tests/CMakeLists.txt tests/unit/surface2d/CMakeLists.txt tests/unit/surface2d/test_surface_state.cpp
git commit -m "feat(surface2d): add cell-aligned surface state skeleton"
```

---

## Task 6: CPU Step Skeleton

**Files:**
- Create: `libs/surface2d/include/surface2d/time_integration/step.hpp`
- Create: `libs/surface2d/src/time_integration/step.cpp`
- Create: `tests/unit/surface2d/test_step.cpp`
- Modify: `tests/unit/surface2d/CMakeLists.txt`

- [ ] **Step 1: 写 step 测试**

创建 `tests/unit/surface2d/test_step.cpp`：

```cpp
#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/state/state.hpp"
#include "surface2d/time_integration/step.hpp"

TEST(SurfaceStep, CpuSkeletonPreservesStateAndReportsDiagnostics) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::for_mesh(mesh);
    state.cells[0].conserved.h = 1.25;
    state.cells[0].conserved.hu = 0.5;
    state.cells[0].conserved.hv = -0.25;

    const scau::surface2d::StepConfig config{.dt = 0.5, .cfl_safety = 0.45, .c_rollback = 1.0};
    const auto diagnostics = scau::surface2d::advance_one_step_cpu(mesh, state, config);

    EXPECT_EQ(state.cells[0].conserved.h, 1.25);
    EXPECT_EQ(state.cells[0].conserved.hu, 0.5);
    EXPECT_EQ(state.cells[0].conserved.hv, -0.25);
    EXPECT_EQ(diagnostics.cell_count, mesh.cells.size());
    EXPECT_EQ(diagnostics.edge_count, mesh.edges.size());
    EXPECT_EQ(diagnostics.max_cell_cfl, 0.0);
    EXPECT_FALSE(diagnostics.rollback_required);
}

TEST(SurfaceStep, RejectsInvalidStepInputs) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::for_mesh(mesh);

    EXPECT_THROW(
        scau::surface2d::advance_one_step_cpu(mesh, state, scau::surface2d::StepConfig{.dt = 0.0, .cfl_safety = 0.45, .c_rollback = 1.0}),
        std::invalid_argument);

    state.cells.pop_back();
    EXPECT_THROW(
        scau::surface2d::advance_one_step_cpu(mesh, state, scau::surface2d::StepConfig{.dt = 0.5, .cfl_safety = 0.45, .c_rollback = 1.0}),
        std::invalid_argument);
}
```

- [ ] **Step 2: 注册 step 测试并确认失败**

修改 `tests/unit/surface2d/CMakeLists.txt` 为：

```cmake
add_executable(test_surface_state test_surface_state.cpp)
target_link_libraries(test_surface_state
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_surface_state COMMAND test_surface_state)

add_executable(test_surface_step test_step.cpp)
target_link_libraries(test_surface_step
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_surface_step COMMAND test_surface_step)
```

运行：

```bash
cmake --build --preset windows-msvc --target test_surface_step
```

Expected: FAIL，原因是 `surface2d/time_integration/step.hpp` 和实现尚未存在。

- [ ] **Step 3: 写 step header**

创建 `libs/surface2d/include/surface2d/time_integration/step.hpp`：

```cpp
#pragma once

#include <cstddef>

#include "core/types.hpp"
#include "mesh/mesh.hpp"
#include "surface2d/state/state.hpp"

namespace scau::surface2d {

struct StepConfig {
    core::Real dt{0.0};
    core::Real cfl_safety{0.45};
    core::Real c_rollback{1.0};
};

struct StepDiagnostics {
    std::size_t cell_count{0U};
    std::size_t edge_count{0U};
    core::Real max_cell_cfl{0.0};
    bool rollback_required{false};
};

[[nodiscard]] StepDiagnostics advance_one_step_cpu(
    const mesh::Mesh& mesh,
    SurfaceState& state,
    const StepConfig& config);

}  // namespace scau::surface2d
```

- [ ] **Step 4: 写 step implementation**

创建 `libs/surface2d/src/time_integration/step.cpp`：

```cpp
#include "surface2d/time_integration/step.hpp"

#include <stdexcept>

namespace scau::surface2d {

StepDiagnostics advance_one_step_cpu(const mesh::Mesh& mesh, SurfaceState& state, const StepConfig& config) {
    if (config.dt <= 0.0) {
        throw std::invalid_argument("step dt must be positive");
    }
    if (config.cfl_safety <= 0.0) {
        throw std::invalid_argument("CFL_safety must be positive");
    }
    if (config.c_rollback <= 0.0) {
        throw std::invalid_argument("C_rollback must be positive");
    }
    validate_state_matches_mesh(state, mesh);

    return StepDiagnostics{
        .cell_count = mesh.cells.size(),
        .edge_count = mesh.edges.size(),
        .max_cell_cfl = 0.0,
        .rollback_required = false,
    };
}

}  // namespace scau::surface2d
```

- [ ] **Step 5: 运行测试并确认通过**

```bash
cmake --build --preset windows-msvc --target test_surface_step
ctest --preset windows-msvc -R test_surface_step
```

Expected: PASS。

- [ ] **Step 6: 提交**

```bash
git add libs/surface2d/include/surface2d/time_integration/step.hpp libs/surface2d/src/time_integration/step.cpp tests/unit/surface2d/CMakeLists.txt tests/unit/surface2d/test_step.cpp
git commit -m "feat(surface2d): add CPU step skeleton diagnostics"
```

---

## Task 7: Full Build And M2 Evidence

**Files:**
- Create: `superpowers/specs/2026-05-11-plan-m2-surface2d-core-skeleton-evidence.md`

- [ ] **Step 1: 运行完整本地验证**

```bash
cmake --build --preset windows-msvc
ctest --preset windows-msvc
```

Expected: PASS；CTest 至少包含 core、mesh、surface2d 三组单元测试。

- [ ] **Step 2: 写 M2 evidence 文件**

创建 `superpowers/specs/2026-05-11-plan-m2-surface2d-core-skeleton-evidence.md`：

```markdown
# Plan-M2 最小 2D Surface2DCore 骨架退出证据

**日期**: 2026-05-11
**对应实施计划**: `superpowers/plans/2026-05-11-plan-m2-surface2d-core-skeleton.md`

## 1. 退出标准核对

| 标准 | 状态 | 证据 |
|---|---|---|
| `libs/mesh/` 支持 Triangle / Quadrilateral 混合拓扑 | 已完成 | `tests/unit/mesh/test_mesh_geometry.cpp` 覆盖 mixed minimal、quad-only、tri-only。 |
| mesh 计算面积、中心、边长、边中点、法向和邻接 | 已完成 | `test_mesh_geometry` 覆盖 C0/C1/C2 面积中心与 E0/E1/E3/E6 几何。 |
| mesh 拒绝非法 cell / edge specs | 已完成 | `tests/unit/mesh/test_mesh_validation.cpp` 覆盖错误拓扑、重复 ID、缺失/额外边、反向 left-cell。 |
| `libs/surface2d/` 只提供状态与 CPU 空推进骨架 | 已完成 | `tests/unit/surface2d/test_surface_state.cpp` 与 `test_surface_step.cpp` 通过；未实现正式 DPM/HLLC/source terms。 |
| 本地 Windows preset 构建和测试通过 | 已通过 | `cmake --build --preset windows-msvc` 与 `ctest --preset windows-msvc` 通过。 |
| GitHub Actions matrix 通过 | 待回填 | 推送后回填 run URL、Windows/Linux job 状态与耗时。 |

## 2. 范围约束

- 本 M2 不引入 SWMM、D-Flow FM 或任何 1D engine ABI。
- 本 M2 不正式实现 `libs/surface2d/dpm/`、`libs/surface2d/riemann/`、`libs/surface2d/source_terms/`，等待 sandbox B 完整退出。
- `max_cell_cfl` 保持原始物理 CFL 诊断字段；当前 CPU skeleton 返回 `0.0`，后续正式数值推进再填充真实值。
```

- [ ] **Step 3: 提交**

```bash
git add superpowers/specs/2026-05-11-plan-m2-surface2d-core-skeleton-evidence.md
git commit -m "docs(m2): record surface2d skeleton exit evidence"
```

---

## Task 8: CI Verification And Evidence Backfill

**Files:**
- Modify: `superpowers/specs/2026-05-11-plan-m2-surface2d-core-skeleton-evidence.md`

- [ ] **Step 1: 推送 M2 commits**

```bash
git push origin master
```

- [ ] **Step 2: 查询 GitHub Actions run**

使用仓库已有远端对应的 GitHub Actions 页面或 API 查询最新 commit 的 run。记录：

```text
Run URL: <实际 run URL>
linux-gcc: success, <实际耗时>
windows-msvc: success, <实际耗时>
```

- [ ] **Step 3: 回填 evidence**

把 evidence 表格中的：

```markdown
| GitHub Actions matrix 通过 | 待回填 | 推送后回填 run URL、Windows/Linux job 状态与耗时。 |
```

改为：

```markdown
| GitHub Actions matrix 通过 | 已通过 | Run `<实际 run URL>` 通过；`linux-gcc` <实际耗时>，`windows-msvc` <实际耗时>。 |
```

- [ ] **Step 4: 提交 evidence 回填**

```bash
git add superpowers/specs/2026-05-11-plan-m2-surface2d-core-skeleton-evidence.md
git commit -m "docs(m2): record CI exit evidence"
git push origin master
```

---

## Self-Review

- Spec coverage: 覆盖 `project-layout-design.md` 对 `libs/mesh/` 与 `libs/surface2d/` 的最小落位要求；覆盖主架构 Phase 1 中 MeshLib 与 CPU reference solver skeleton 的入口；遵守 sandbox B 未完整退出前不得正式实现 DPM/HLLC/source terms 的限制。
- Placeholder scan: 计划中没有 `TBD`、`TODO`、未定义文件或未定义函数；CI run URL 仅在 Task 8 明确作为实施后实际回填项出现。
- Type consistency: 所有 C++ API 使用 `scau::core::Real`、`scau::core::Vector2`、`scau::mesh::*`、`scau::surface2d::*`；测试中调用的函数均在同任务或前置任务定义。
- Scope check: 本计划只建立 M2 skeleton，不进入 coupling、SWMM、D-Flow FM、正式 DPM、正式 HLLC 或 source terms。

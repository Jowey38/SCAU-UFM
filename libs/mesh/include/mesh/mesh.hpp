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

#include "mesh/mesh.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
#include <utility>

namespace scau::mesh {
namespace {

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
            .id = "E" + std::to_string(index),
            .node_ids = owners[0].second,
            .left_cell = owners[0].first,
            .right_cell = owners.size() == 2U ? std::optional<std::string>{owners[1].first} : std::nullopt,
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
    const auto edges = build_edges(cells, nodes_by_id, std::nullopt);
    return Mesh{.nodes = std::move(nodes), .cells = std::move(cells), .edges = edges};
}

Mesh build_mesh(std::vector<Node> nodes, std::vector<Cell> cells, std::vector<EdgeSpec> edge_specs) {
    const auto nodes_by_id = node_lookup(nodes);
    validate_cells(cells, nodes_by_id);
    const auto edges = build_edges(cells, nodes_by_id, std::move(edge_specs));
    return Mesh{.nodes = std::move(nodes), .cells = std::move(cells), .edges = edges};
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

#include "surface2d/geometry/cache.hpp"

#include <stdexcept>
#include <string>
#include <unordered_map>

namespace scau::surface2d {
namespace {

std::unordered_map<std::string, std::size_t> cell_indices_by_id(const mesh::Mesh& mesh) {
    std::unordered_map<std::string, std::size_t> indices;
    indices.reserve(mesh.cells.size());
    for (std::size_t index = 0; index < mesh.cells.size(); ++index) {
        const auto [it, inserted] = indices.emplace(mesh.cells[index].id, index);
        if (!inserted) {
            throw std::invalid_argument("mesh cell ids must be unique: " + mesh.cells[index].id);
        }
    }
    return indices;
}

std::optional<std::size_t> resolve_cell(
    const std::unordered_map<std::string, std::size_t>& indices,
    const std::optional<std::string>& cell_id) {
    if (!cell_id.has_value()) {
        return std::nullopt;
    }
    const auto it = indices.find(*cell_id);
    if (it == indices.end()) {
        throw std::invalid_argument("edge references unknown cell id: " + *cell_id);
    }
    return it->second;
}

}  // namespace

GeometryCache GeometryCache::for_mesh(const mesh::Mesh& mesh) {
    const auto indices = cell_indices_by_id(mesh);
    const auto nodes = mesh::node_lookup(mesh.nodes);

    GeometryCache cache;
    cache.cell_areas.reserve(mesh.cells.size());
    for (const auto& cell : mesh.cells) {
        cache.cell_areas.push_back(mesh::cell_area(cell, nodes));
    }

    cache.edge_cells.reserve(mesh.edges.size());
    for (const auto& edge : mesh.edges) {
        EdgeAdjacency adjacency{
            .left = resolve_cell(indices, edge.left_cell),
            .right = resolve_cell(indices, edge.right_cell),
        };
        if (!adjacency.left.has_value() && !adjacency.right.has_value()) {
            throw std::invalid_argument("edge must reference at least one cell: " + edge.id);
        }
        cache.edge_cells.push_back(adjacency);
    }
    return cache;
}

void validate_geometry_cache_matches_mesh(const GeometryCache& cache, const mesh::Mesh& mesh) {
    if (cache.cell_areas.size() != mesh.cells.size()) {
        throw std::invalid_argument("geometry cache cell count must match mesh cell count");
    }
    if (cache.edge_cells.size() != mesh.edges.size()) {
        throw std::invalid_argument("geometry cache edge count must match mesh edge count");
    }
    for (const auto& adjacency : cache.edge_cells) {
        if (!adjacency.left.has_value() && !adjacency.right.has_value()) {
            throw std::invalid_argument("geometry cache edge must reference at least one cell");
        }
        if (adjacency.left.has_value() && *adjacency.left >= mesh.cells.size()) {
            throw std::invalid_argument("geometry cache left cell index out of range");
        }
        if (adjacency.right.has_value() && *adjacency.right >= mesh.cells.size()) {
            throw std::invalid_argument("geometry cache right cell index out of range");
        }
    }
}

}  // namespace scau::surface2d

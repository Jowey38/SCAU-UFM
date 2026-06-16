#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "core/types.hpp"
#include "mesh/mesh.hpp"

namespace scau::surface2d {

struct EdgeAdjacency {
    std::optional<std::size_t> left;
    std::optional<std::size_t> right;

    [[nodiscard]] bool is_internal() const noexcept {
        return left.has_value() && right.has_value();
    }

    [[nodiscard]] std::size_t inside() const {
        return left.has_value() ? *left : *right;
    }
};

// Precomputed index-based mesh geometry so per-step code never touches
// string ids or recomputes cell areas. Build once per mesh, reuse across steps.
struct GeometryCache {
    std::vector<core::Real> cell_areas;
    std::vector<EdgeAdjacency> edge_cells;

    [[nodiscard]] static GeometryCache for_mesh(const mesh::Mesh& mesh);
};

void validate_geometry_cache_matches_mesh(const GeometryCache& cache, const mesh::Mesh& mesh);

}  // namespace scau::surface2d

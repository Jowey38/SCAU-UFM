#pragma once

#include <cstddef>
#include <vector>

#include "mesh/mesh.hpp"

namespace scau::surface2d {

enum class BoundaryKind {
    Wall,
    Open,
};

struct BoundaryConditions {
    std::vector<BoundaryKind> edges;

    [[nodiscard]] static BoundaryConditions for_mesh(const mesh::Mesh& mesh);
};

void validate_boundary_conditions_match_mesh(const BoundaryConditions& boundary, const mesh::Mesh& mesh);

}  // namespace scau::surface2d

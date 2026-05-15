#include "surface2d/boundary/conditions.hpp"

#include <stdexcept>

namespace scau::surface2d {

BoundaryConditions BoundaryConditions::for_mesh(const mesh::Mesh& mesh) {
    return BoundaryConditions{
        .edges = std::vector<BoundaryKind>(mesh.edges.size(), BoundaryKind::Wall),
    };
}

void validate_boundary_conditions_match_mesh(const BoundaryConditions& boundary, const mesh::Mesh& mesh) {
    if (boundary.edges.size() != mesh.edges.size()) {
        throw std::invalid_argument("boundary edge count must match mesh edge count");
    }
}

}  // namespace scau::surface2d

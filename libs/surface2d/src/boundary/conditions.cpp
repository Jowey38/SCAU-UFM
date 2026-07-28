#include "surface2d/boundary/conditions.hpp"

#include <cmath>
#include <stdexcept>
#include <string>

namespace scau::surface2d {

BoundaryConditions BoundaryConditions::for_mesh(const mesh::Mesh& mesh) {
    return BoundaryConditions{
        .edges = std::vector<BoundaryKind>(mesh.edges.size(), BoundaryKind::Wall),
        .discharge_per_width = {},
        .water_level = {},
    };
}

void validate_boundary_conditions_match_mesh(const BoundaryConditions& boundary, const mesh::Mesh& mesh) {
    if (boundary.edges.size() != mesh.edges.size()) {
        throw std::invalid_argument("boundary edge count must match mesh edge count");
    }
    if (!boundary.discharge_per_width.empty()
        && boundary.discharge_per_width.size() != mesh.edges.size()) {
        throw std::invalid_argument(
            "boundary discharge_per_width must be empty or match mesh edge count");
    }
    if (!boundary.water_level.empty() && boundary.water_level.size() != mesh.edges.size()) {
        throw std::invalid_argument("boundary water_level must be empty or match mesh edge count");
    }
    for (std::size_t edge_index = 0; edge_index < boundary.edges.size(); ++edge_index) {
        if (boundary.edges[edge_index] == BoundaryKind::DischargeInflow) {
            const core::Real q = boundary.discharge_at(edge_index);
            if (!std::isfinite(q) || q < 0.0) {
                throw std::invalid_argument(
                    "DischargeInflow edge " + std::to_string(edge_index)
                    + " requires finite discharge_per_width >= 0");
            }
        }
        if (boundary.edges[edge_index] == BoundaryKind::WaterLevel) {
            if (boundary.water_level.size() != mesh.edges.size()) {
                throw std::invalid_argument(
                    "WaterLevel edge " + std::to_string(edge_index)
                    + " requires a mesh-edge-sized water_level vector");
            }
            if (!std::isfinite(boundary.water_level[edge_index])) {
                throw std::invalid_argument(
                    "WaterLevel edge " + std::to_string(edge_index)
                    + " requires a finite water_level value");
            }
        }
    }
}

}  // namespace scau::surface2d

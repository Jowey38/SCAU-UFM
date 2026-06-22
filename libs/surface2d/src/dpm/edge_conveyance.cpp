#include "surface2d/dpm/edge_conveyance.hpp"

#include "surface2d/dpm/tensor_projection.hpp"

namespace scau::surface2d {

EdgeConveyanceAssemblyReport assemble_edge_conveyance_from_tensors(
    const mesh::Mesh& mesh,
    const GeometryCache& geometry,
    DpmFields& dpm_fields) {
    validate_dpm_fields_match_mesh(dpm_fields, mesh);
    validate_geometry_cache_matches_mesh(geometry, mesh);

    EdgeConveyanceAssemblyReport report;
    for (std::size_t edge_index = 0; edge_index < mesh.edges.size(); ++edge_index) {
        const auto& adjacency = geometry.edge_cells[edge_index];
        const std::size_t left_index = adjacency.left.has_value() ? *adjacency.left : *adjacency.right;
        const std::size_t right_index = adjacency.right.has_value() ? *adjacency.right : *adjacency.left;

        const auto projection = project_edge_conveyance(
            dpm_fields.cells[left_index].Phi_c,
            dpm_fields.cells[right_index].Phi_c,
            mesh.edges[edge_index].normal,
            dpm_fields.edges[edge_index].omega_edge);

        dpm_fields.edges[edge_index].phi_e_n = projection.phi_e_n;
        if (projection.weak_dpm_guarantee) {
            ++report.weak_dpm_guarantee_edge_count;
        }
    }
    return report;
}

}  // namespace scau::surface2d

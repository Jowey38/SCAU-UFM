#include "surface2d/stcf_bridge/assemble.hpp"

#include <stdexcept>
#include <string>

namespace scau::surface2d {
namespace {

void require_counts(
    const mesh::Mesh& mesh,
    const stcf::StcfDataset& dataset,
    bool check_edges) {
    if (dataset.cells.phi_t.size() != mesh.cells.size()) {
        throw std::invalid_argument(
            "STCF cell count (" + std::to_string(dataset.cells.phi_t.size())
            + ") must match mesh cell count (" + std::to_string(mesh.cells.size()) + ")");
    }
    if (check_edges && dataset.edges.omega_edge.size() != mesh.edges.size()) {
        throw std::invalid_argument(
            "STCF edge count (" + std::to_string(dataset.edges.omega_edge.size())
            + ") must match mesh edge count (" + std::to_string(mesh.edges.size()) + ")");
    }
}

}  // namespace

DpmFields assemble_dpm_fields(const mesh::Mesh& mesh, const stcf::StcfDataset& dataset) {
    require_counts(mesh, dataset, true);
    DpmFields fields;
    fields.cells.resize(mesh.cells.size());
    fields.edges.resize(mesh.edges.size());
    for (std::size_t i = 0; i < mesh.cells.size(); ++i) {
        fields.cells[i].phi_t = dataset.cells.phi_t[i];
        fields.cells[i].Phi_c = Tensor2Symmetric{
            .xx = dataset.cells.phi_xx[i],
            .xy = dataset.cells.phi_xy[i],
            .yy = dataset.cells.phi_yy[i],
        };
    }
    for (std::size_t e = 0; e < mesh.edges.size(); ++e) {
        fields.edges[e].phi_e_n = dataset.edges.phi_e_n[e];
        fields.edges[e].omega_edge = dataset.edges.omega_edge[e];
    }
    return fields;
}

SourceTermFields assemble_source_term_fields(
    const mesh::Mesh& mesh, const stcf::StcfDataset& dataset) {
    require_counts(mesh, dataset, false);
    SourceTermFields fields;
    fields.manning_n = dataset.cells.manning_n;
    fields.exchange_volume.assign(mesh.cells.size(), 0.0);
    return fields;
}

std::vector<core::Real> assemble_bed_elevations(
    const mesh::Mesh& mesh, const stcf::StcfDataset& dataset) {
    require_counts(mesh, dataset, false);
    return dataset.cells.z_b;
}

}  // namespace scau::surface2d

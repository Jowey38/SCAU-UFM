#include "stcf/schema.hpp"

namespace scau::stcf {

StcfDataset make_uniform_dataset(std::size_t cell_count, std::size_t edge_count) {
    StcfDataset dataset;
    dataset.cells.phi_t.assign(cell_count, 1.0);
    dataset.cells.phi_xx.assign(cell_count, 1.0);
    dataset.cells.phi_xy.assign(cell_count, 0.0);
    dataset.cells.phi_yy.assign(cell_count, 1.0);
    dataset.cells.manning_n.assign(cell_count, 0.0);
    dataset.cells.z_b.assign(cell_count, 0.0);
    dataset.cells.soil_type.assign(cell_count, 0);
    dataset.edges.omega_edge.assign(edge_count, 1.0);
    dataset.edges.phi_e_n.assign(edge_count, 1.0);
    dataset.edges.phi_et.assign(edge_count, 1.0);
    dataset.soil_params.push_back(SoilParamsEntry{
        .K_s = 1.0e-6,
        .psi_f = 0.1,
        .theta_s = 0.4,
        .theta_i = 0.1,
    });
    return dataset;
}

}  // namespace scau::stcf

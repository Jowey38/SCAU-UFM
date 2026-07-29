#include "stcf/case_profiles.hpp"

#include <stdexcept>
#include <string>
#include <vector>

#include "stcf/schema.hpp"

namespace scau::stcf {

StcfCase make_mixed_minimal_case() {
    StcfCase stcf_case;
    stcf_case.topology.node_x = {0.0, 1.0, 1.0, 0.0, 2.0};
    stcf_case.topology.node_y = {0.0, 0.0, 1.0, 1.0, 0.0};
    stcf_case.topology.face_nodes = {
        {0, 1, 2, 3},
        {1, 4, 2, kConnectivityFillValue},
    };
    stcf_case.topology.edge_nodes = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0}, {1, 4}, {4, 2},
    };
    stcf_case.topology.edge_faces = {
        {0, kConnectivityFillValue}, {0, 1}, {0, kConnectivityFillValue},
        {0, kConnectivityFillValue}, {1, kConnectivityFillValue},
        {1, kConnectivityFillValue},
    };
    stcf_case.topology.face_edges = {
        {0, 1, 2, 3},
        {4, 5, 1, kConnectivityFillValue},
    };

    stcf_case.fields = make_uniform_dataset(2, 6);
    stcf_case.fields.cells.phi_t = {1.0, 0.8};
    stcf_case.fields.cells.phi_xx = {0.95, 0.7};
    stcf_case.fields.cells.phi_xy = {0.0, 0.05};
    stcf_case.fields.cells.phi_yy = {0.9, 0.75};
    stcf_case.fields.cells.manning_n = {0.02, 0.03};
    stcf_case.fields.cells.z_b = {0.0, 0.1};
    stcf_case.fields.cells.soil_type = {0, 1};
    stcf_case.fields.soil_params = {
        SoilParamsEntry{.K_s = 1.0e-6, .psi_f = 0.1, .theta_s = 0.4, .theta_i = 0.1},
        SoilParamsEntry{.K_s = 5.0e-6, .psi_f = 0.2, .theta_s = 0.45, .theta_i = 0.05},
    };
    stcf_case.fields.edges.omega_edge = {1.0, 0.9, 1.0, 1.0, 0.8, 1.0};
    stcf_case.fields.edges.phi_e_n = {0.95, 0.7, 0.9, 0.9, 0.6, 0.7};
    stcf_case.fields.edges.phi_et = {0.9, 0.65, 0.85, 0.85, 0.55, 0.65};
    return stcf_case;
}

std::vector<std::string> case_profile_names() {
    return {kMixedMinimalProfile};
}

StcfCase make_case_profile(const std::string& profile_name) {
    if (profile_name == kMixedMinimalProfile) {
        return make_mixed_minimal_case();
    }
    throw std::invalid_argument("unknown STCF case profile: " + profile_name);
}

}  // namespace scau::stcf

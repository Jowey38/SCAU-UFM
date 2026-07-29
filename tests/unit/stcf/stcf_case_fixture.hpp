#pragma once

#include "stcf/schema.hpp"
#include "stcf/topology.hpp"

namespace scau::stcf::test {

// Two-face mixed mesh:
//   quad:     0 -- 1
//             |    | shared edge 1-2
//             3 -- 2
//   triangle: 1 -- 4
//              \  /
//               2
inline StcfCase make_mixed_case() {
    StcfCase stcf_case;
    stcf_case.topology.node_x = {0.0, 1.0, 1.0, 0.0, 2.0};
    stcf_case.topology.node_y = {0.0, 0.0, 1.0, 1.0, 0.0};
    stcf_case.topology.face_nodes = {
        {0, 1, 2, 3},
        {1, 4, 2, kConnectivityFillValue},
    };
    stcf_case.topology.edge_nodes = {
        {0, 1},
        {1, 2},
        {2, 3},
        {3, 0},
        {1, 4},
        {4, 2},
    };
    stcf_case.topology.edge_faces = {
        {0, kConnectivityFillValue},
        {0, 1},
        {0, kConnectivityFillValue},
        {0, kConnectivityFillValue},
        {1, kConnectivityFillValue},
        {1, kConnectivityFillValue},
    };
    stcf_case.topology.face_edges = {
        {0, 1, 2, 3},
        {4, 5, 1, kConnectivityFillValue},
    };
    stcf_case.fields = make_uniform_dataset(2, 6);
    stcf_case.fields.cells.phi_t = {1.0, 0.8};
    stcf_case.fields.cells.phi_xx = {1.0, 0.7};
    stcf_case.fields.cells.phi_yy = {1.0, 0.75};
    stcf_case.fields.cells.manning_n = {0.02, 0.03};
    stcf_case.fields.cells.z_b = {0.0, 0.1};
    return stcf_case;
}

}  // namespace scau::stcf::test

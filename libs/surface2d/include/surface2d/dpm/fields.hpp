#pragma once

#include <vector>

#include "core/types.hpp"
#include "mesh/mesh.hpp"

namespace scau::surface2d {

struct CellDpmFields {
    core::Real phi_t{1.0};
    core::Real Phi_c{1.0};
};

struct EdgeDpmFields {
    core::Real phi_e_n{1.0};
    core::Real omega_edge{1.0};
};

struct DpmFields {
    std::vector<CellDpmFields> cells;
    std::vector<EdgeDpmFields> edges;

    [[nodiscard]] static DpmFields for_mesh(const mesh::Mesh& mesh);
};

void validate_dpm_fields_match_mesh(const DpmFields& fields, const mesh::Mesh& mesh);

}  // namespace scau::surface2d

#pragma once

#include <vector>

#include "core/types.hpp"
#include "mesh/mesh.hpp"
#include "surface2d/dpm/tensor_projection.hpp"

namespace scau::surface2d {

struct CellDpmFields {
    core::Real phi_t{1.0};
    // Directional conveyance tensor (transport sovereignty). Default identity
    // keeps edge phi_e_n derivation at 1.0, matching the pre-tensor baseline.
    Tensor2Symmetric Phi_c{};
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

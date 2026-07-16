#pragma once

#include <vector>

#include "core/types.hpp"
#include "mesh/mesh.hpp"

namespace scau::surface2d {

// Per-cell source-term inputs for one substep. Defaults are neutral:
// zero Manning roughness, no exchange. Rainfall and infiltration are
// owned by the runoff stage (RunoffStepInputs); these fields carry only
// the coupling-decided exchange volumes and Manning roughness.
// exchange_volume carries coupling-decided volumes only; the surface layer
// never originates exchange decisions (CouplingLib sovereignty).
struct SourceTermFields {
    std::vector<core::Real> manning_n;
    std::vector<core::Real> exchange_volume;

    [[nodiscard]] static SourceTermFields for_mesh(const mesh::Mesh& mesh);
};

void validate_source_term_fields_match_mesh(const SourceTermFields& fields, const mesh::Mesh& mesh);

}  // namespace scau::surface2d

#pragma once

#include <cstddef>

#include "mesh/mesh.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/geometry/cache.hpp"

namespace scau::surface2d {

struct EdgeConveyanceAssemblyReport {
    std::size_t weak_dpm_guarantee_edge_count{0};
};

// Derive edge phi_e_n from the cell Phi_c conveyance tensors and write it into
// dpm_fields.edges (omega_edge is read and preserved). Internal edges use the
// arithmetic-mean edge tensor projected onto the edge normal (main-spec 5.5.2);
// boundary edges use the single incident cell tensor. Edges whose edge-normal
// anisotropy metric exceeds the WARN threshold are counted as weak-DPM-guarantee
// (audit, never blocking).
//
// This is the analytic derivation path (main-spec 5.3 rule 2). PreProc rule-1
// phi_e_n must be set on the edge fields directly and not routed through here.
// On a default DpmFields (identity Phi_c, omega_edge = 1) this leaves every
// phi_e_n at 1.0, preserving the pre-tensor numerical baseline.
EdgeConveyanceAssemblyReport assemble_edge_conveyance_from_tensors(
    const mesh::Mesh& mesh,
    const GeometryCache& geometry,
    DpmFields& dpm_fields);

}  // namespace scau::surface2d

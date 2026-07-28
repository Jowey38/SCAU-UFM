#pragma once

#include <vector>

#include "core/types.hpp"
#include "mesh/mesh.hpp"
#include "stcf/schema.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/source_terms/fields.hpp"

namespace scau::surface2d {

// STCF -> Surface2D assembly seam (dependency direction surface2d -> stcf,
// never the reverse). Maps a validated StcfDataset onto the solver-facing
// DpmFields, SourceTermFields, and bed elevations. This is the load-time
// bridge that lets the solver consume real case data instead of
// programmatically constructed fixtures.
//
// The dataset must already be shape- and constraint-valid for the mesh
// (cell/edge counts agree; run stcf::validate_stcf_dataset first). Each
// function re-checks the mesh count agreement fail-closed.

// phi_t + Phi_c per cell; phi_e_n + omega_edge per edge. Edge phi_e_n is
// taken directly from the STCF file (main-spec 5.3 rule-1 PreProc primary
// assembly), NOT re-derived from cell tensors here.
[[nodiscard]] DpmFields assemble_dpm_fields(const mesh::Mesh& mesh, const stcf::StcfDataset& dataset);

// Manning roughness per cell; exchange volume defaults to zero (coupling
// sovereignty: the surface layer never originates exchange decisions).
[[nodiscard]] SourceTermFields assemble_source_term_fields(
    const mesh::Mesh& mesh, const stcf::StcfDataset& dataset);

// Per-cell bed elevation z_b (m). Callers build the initial hydrostatic state
// as eta = z_b + h, keeping the solver's z_b = eta - h invariant.
[[nodiscard]] std::vector<core::Real> assemble_bed_elevations(
    const mesh::Mesh& mesh, const stcf::StcfDataset& dataset);

}  // namespace scau::surface2d

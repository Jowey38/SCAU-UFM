#include "surface2d/dpm/phi_t_remap.hpp"

#include <cmath>
#include <stdexcept>
#include <vector>

namespace scau::surface2d {
namespace {

void validate_positive_phi_t(core::Real phi_t) {
    if (!std::isfinite(phi_t) || phi_t <= 0.0) {
        throw std::invalid_argument("phi_t remap requires positive finite phi_t");
    }
}

void accumulate_audit(
    PhiTRemapDiagnostics& diagnostics,
    const CellState& cell,
    core::Real phi_t,
    core::Real area,
    bool after) {
    const core::Real volume = phi_t * cell.conserved.h * area;
    const core::Real momentum_x = phi_t * cell.conserved.hu * area;
    const core::Real momentum_y = phi_t * cell.conserved.hv * area;
    if (after) {
        diagnostics.volume_after += volume;
        diagnostics.momentum_x_after += momentum_x;
        diagnostics.momentum_y_after += momentum_y;
        return;
    }
    diagnostics.volume_before += volume;
    diagnostics.momentum_x_before += momentum_x;
    diagnostics.momentum_y_before += momentum_y;
}

}  // namespace

PhiTRemapDiagnostics remap_state_for_phi_t_change(
    const mesh::Mesh& mesh,
    SurfaceState& state,
    const DpmFields& old_dpm_fields,
    const DpmFields& new_dpm_fields,
    const GeometryCache& geometry) {
    validate_state_matches_mesh(state, mesh);
    validate_dpm_fields_match_mesh(old_dpm_fields, mesh);
    validate_dpm_fields_match_mesh(new_dpm_fields, mesh);
    validate_geometry_cache_matches_mesh(geometry, mesh);

    auto remapped_cells = state.cells;
    PhiTRemapDiagnostics diagnostics{};

    for (std::size_t cell_index = 0; cell_index < mesh.cells.size(); ++cell_index) {
        const core::Real area = geometry.cell_areas[cell_index];
        if (!std::isfinite(area) || area <= 0.0) {
            throw std::invalid_argument("phi_t remap requires positive finite cell area");
        }
        const core::Real old_phi_t = old_dpm_fields.cells[cell_index].phi_t;
        const core::Real new_phi_t = new_dpm_fields.cells[cell_index].phi_t;
        validate_positive_phi_t(old_phi_t);
        validate_positive_phi_t(new_phi_t);

        const auto& old_cell = state.cells[cell_index];
        accumulate_audit(diagnostics, old_cell, old_phi_t, area, false);

        const core::Real old_h = old_cell.conserved.h;
        const core::Real stored_volume = old_phi_t * old_h * area;
        const core::Real new_h = stored_volume / (new_phi_t * area);
        const core::Real bed = old_cell.eta - old_h;

        auto& new_cell = remapped_cells[cell_index];
        new_cell.conserved.h = new_h;
        new_cell.eta = bed + new_h;
        if (old_h > 0.0 && new_h > 0.0) {
            const core::Real u = old_cell.u();
            const core::Real v = old_cell.v();
            new_cell.conserved.hu = new_h * u;
            new_cell.conserved.hv = new_h * v;
        } else {
            new_cell.conserved.hu = 0.0;
            new_cell.conserved.hv = 0.0;
        }
    }

    state.cells = remapped_cells;
    diagnostics.remapped_cells = state.cells.size();
    for (std::size_t cell_index = 0; cell_index < mesh.cells.size(); ++cell_index) {
        accumulate_audit(diagnostics, state.cells[cell_index], new_dpm_fields.cells[cell_index].phi_t, geometry.cell_areas[cell_index], true);
    }
    return diagnostics;
}

}  // namespace scau::surface2d

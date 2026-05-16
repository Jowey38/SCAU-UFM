#include "surface2d/time_integration/step.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "surface2d/riemann/hllc.hpp"
#include "surface2d/source_terms/phi_t.hpp"

namespace scau::surface2d {
namespace {

void validate_step_inputs(const mesh::Mesh& mesh, const SurfaceState& state, const StepConfig& config) {
    if (config.dt <= 0.0) {
        throw std::invalid_argument("step dt must be positive");
    }
    if (config.cfl_safety <= 0.0) {
        throw std::invalid_argument("CFL_safety must be positive");
    }
    if (config.c_rollback <= 0.0) {
        throw std::invalid_argument("C_rollback must be positive");
    }
    if (config.h_min < 0.0) {
        throw std::invalid_argument("h_min must be non-negative");
    }
    validate_state_matches_mesh(state, mesh);
}

std::unordered_map<std::string, std::size_t> cell_indices_by_id(const mesh::Mesh& mesh) {
    std::unordered_map<std::string, std::size_t> indices;
    for (std::size_t index = 0; index < mesh.cells.size(); ++index) {
        indices.emplace(mesh.cells[index].id, index);
    }
    return indices;
}

std::size_t edge_cell_index(
    const std::unordered_map<std::string, std::size_t>& cell_indices,
    const std::optional<std::string>& primary_cell,
    const std::optional<std::string>& fallback_cell) {
    const auto& cell_id = primary_cell.has_value() ? *primary_cell : *fallback_cell;
    return cell_indices.at(cell_id);
}

core::Real cell_speed(const CellState& cell) {
    return std::hypot(cell.u(), cell.v());
}

core::Real raw_cell_cfl(const mesh::Mesh& mesh, const SurfaceState& state, const StepConfig& config) {
    core::Real max_cfl = 0.0;
    const auto nodes = mesh::node_lookup(mesh.nodes);
    for (std::size_t cell_index = 0; cell_index < mesh.cells.size(); ++cell_index) {
        const core::Real area = mesh::cell_area(mesh.cells[cell_index], nodes);
        if (area > 0.0) {
            max_cfl = std::max(max_cfl, config.dt * cell_speed(state.cells[cell_index]) / area);
        }
    }
    return max_cfl;
}

void apply_depth_update(
    const mesh::Mesh& mesh,
    SurfaceState& state,
    const StepConfig& config,
    const std::vector<CellStepDiagnostics>& cells) {
    const auto nodes = mesh::node_lookup(mesh.nodes);
    for (std::size_t cell_index = 0; cell_index < mesh.cells.size(); ++cell_index) {
        const core::Real area = mesh::cell_area(mesh.cells[cell_index], nodes);
        if (area > 0.0) {
            state.cells[cell_index].conserved.h = std::max(
                0.0,
                state.cells[cell_index].conserved.h + config.dt * cells[cell_index].mass_residual / area);
        }
    }
}

EdgeStepDiagnostics edge_step_diagnostics(
    const mesh::Edge& edge,
    const SurfaceState& state,
    const DpmFields& dpm_fields,
    const std::unordered_map<std::string, std::size_t>& cell_indices,
    std::size_t edge_index,
    core::Real h_min) {
    const std::size_t left_index = edge_cell_index(cell_indices, edge.left_cell, edge.right_cell);
    const std::size_t right_index = edge_cell_index(cell_indices, edge.right_cell, edge.left_cell);
    const auto& left = state.cells[left_index];
    const auto& right = state.cells[right_index];
    const auto flux = hllc_normal_flux(
        left,
        right,
        dpm_fields.edges[edge_index],
        Normal2{.x = edge.normal.x, .y = edge.normal.y},
        h_min);

    EdgeStepDiagnostics diagnostics{
        .mass_flux = flux.mass,
        .momentum_flux_n = flux.momentum_n,
    };

    if (edge.left_cell.has_value() && edge.right_cell.has_value()) {
        const core::Real h = 0.5 * (left.conserved.h + right.conserved.h);
        diagnostics.pressure_pairing = pressure_pairing_phi_t(
            dpm_fields.cells[left_index].phi_t,
            dpm_fields.cells[right_index].phi_t,
            h);
        diagnostics.s_phi_t = s_phi_t_centered(
            dpm_fields.cells[left_index].phi_t,
            dpm_fields.cells[right_index].phi_t,
            h);
        diagnostics.residual = diagnostics.pressure_pairing + diagnostics.s_phi_t;
    }

    return diagnostics;
}

StepDiagnostics base_diagnostics(const mesh::Mesh& mesh, const SurfaceState& state, const StepConfig& config) {
    const core::Real max_cell_cfl = raw_cell_cfl(mesh, state, config);
    return StepDiagnostics{
        .cell_count = mesh.cells.size(),
        .edge_count = mesh.edges.size(),
        .max_cell_cfl = max_cell_cfl,
        .rollback_required = max_cell_cfl > config.c_rollback,
        .cells = std::vector<CellStepDiagnostics>(mesh.cells.size()),
    };
}

CellState open_boundary_outside_state(const CellState& inside) {
    return CellState{
        .conserved = {.h = inside.conserved.h, .hu = inside.conserved.hu, .hv = inside.conserved.hv},
        .eta = inside.eta,
    };
}

const CellState& upwind_state(core::Real mass_flux, const CellState& left, const CellState& right) {
    return mass_flux >= 0.0 ? left : right;
}

void accumulate_momentum_residual(
    CellStepDiagnostics& sink,
    core::Real signed_integrated_flux,
    const CellState& upwind) {
    sink.momentum_residual.x += signed_integrated_flux * upwind.u();
    sink.momentum_residual.y += signed_integrated_flux * upwind.v();
}

void apply_momentum_update(
    const mesh::Mesh& mesh,
    SurfaceState& state,
    const StepConfig& config,
    const std::vector<CellStepDiagnostics>& cells) {
    const auto nodes = mesh::node_lookup(mesh.nodes);
    for (std::size_t cell_index = 0; cell_index < mesh.cells.size(); ++cell_index) {
        const core::Real area = mesh::cell_area(mesh.cells[cell_index], nodes);
        if (area <= 0.0) {
            continue;
        }
        if (state.cells[cell_index].conserved.h <= config.h_min) {
            state.cells[cell_index].conserved.hu = 0.0;
            state.cells[cell_index].conserved.hv = 0.0;
            continue;
        }
        state.cells[cell_index].conserved.hu += config.dt * cells[cell_index].momentum_residual.x / area;
        state.cells[cell_index].conserved.hv += config.dt * cells[cell_index].momentum_residual.y / area;
    }
}

}  // namespace

StepDiagnostics advance_one_step_cpu(const mesh::Mesh& mesh, SurfaceState& state, const StepConfig& config) {
    validate_step_inputs(mesh, state, config);
    return base_diagnostics(mesh, state, config);
}

StepDiagnostics advance_one_step_cpu(
    const mesh::Mesh& mesh,
    SurfaceState& state,
    const StepConfig& config,
    const DpmFields& dpm_fields) {
    return advance_one_step_cpu(mesh, state, config, dpm_fields, BoundaryConditions::for_mesh(mesh));
}

StepDiagnostics advance_one_step_cpu(
    const mesh::Mesh& mesh,
    SurfaceState& state,
    const StepConfig& config,
    const DpmFields& dpm_fields,
    const BoundaryConditions& boundary) {
    validate_step_inputs(mesh, state, config);
    validate_dpm_fields_match_mesh(dpm_fields, mesh);
    validate_boundary_conditions_match_mesh(boundary, mesh);

    auto diagnostics = base_diagnostics(mesh, state, config);
    diagnostics.edges.reserve(mesh.edges.size());
    const auto cell_indices = cell_indices_by_id(mesh);
    for (std::size_t edge_index = 0; edge_index < mesh.edges.size(); ++edge_index) {
        const auto& edge = mesh.edges[edge_index];
        const bool is_internal = edge.left_cell.has_value() && edge.right_cell.has_value();

        if (is_internal) {
            const auto edge_diagnostics = edge_step_diagnostics(edge, state, dpm_fields, cell_indices, edge_index, config.h_min);
            diagnostics.edges.push_back(edge_diagnostics);
            const std::size_t left_index = cell_indices.at(*edge.left_cell);
            const std::size_t right_index = cell_indices.at(*edge.right_cell);
            const core::Real integrated_flux = edge_diagnostics.mass_flux * edge.length;
            diagnostics.cells[left_index].mass_residual -= integrated_flux;
            diagnostics.cells[right_index].mass_residual += integrated_flux;
            const auto& upwind = upwind_state(edge_diagnostics.mass_flux, state.cells[left_index], state.cells[right_index]);
            accumulate_momentum_residual(diagnostics.cells[left_index], -integrated_flux, upwind);
            accumulate_momentum_residual(diagnostics.cells[right_index], integrated_flux, upwind);
            continue;
        }

        if (boundary.edges[edge_index] == BoundaryKind::Wall) {
            diagnostics.edges.push_back(EdgeStepDiagnostics{});
            continue;
        }

        const std::size_t inside_index = edge_cell_index(cell_indices, edge.left_cell, edge.right_cell);
        const auto& inside = state.cells[inside_index];
        const auto outside = open_boundary_outside_state(inside);
        const auto flux = hllc_normal_flux(
            inside,
            outside,
            dpm_fields.edges[edge_index],
            Normal2{.x = edge.normal.x, .y = edge.normal.y},
            config.h_min);

        EdgeStepDiagnostics edge_diagnostics{
            .mass_flux = flux.mass,
            .momentum_flux_n = flux.momentum_n,
        };
        diagnostics.edges.push_back(edge_diagnostics);

        const core::Real integrated_flux = edge_diagnostics.mass_flux * edge.length;
        const core::Real signed_integrated_flux = edge.left_cell.has_value() ? -integrated_flux : integrated_flux;
        diagnostics.cells[inside_index].mass_residual += signed_integrated_flux;

        const auto outside_for_upwind = open_boundary_outside_state(inside);
        const auto& upwind_open = upwind_state(edge_diagnostics.mass_flux, inside, outside_for_upwind);
        accumulate_momentum_residual(diagnostics.cells[inside_index], signed_integrated_flux, upwind_open);
    }
    apply_depth_update(mesh, state, config, diagnostics.cells);
    apply_momentum_update(mesh, state, config, diagnostics.cells);
    return diagnostics;
}

}  // namespace scau::surface2d

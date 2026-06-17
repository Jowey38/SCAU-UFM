#include "surface2d/time_integration/step.hpp"

#include <cmath>
#include <stdexcept>
#include <vector>

#include "surface2d/cfl/diagnostics.hpp"
#include "surface2d/dpm/edge_classification.hpp"
#include "surface2d/riemann/hllc.hpp"
#include "surface2d/source_terms/coupling_exchange.hpp"
#include "surface2d/source_terms/friction.hpp"
#include "surface2d/source_terms/infiltration.hpp"
#include "surface2d/source_terms/phi_t.hpp"
#include "surface2d/source_terms/rainfall.hpp"
#include "surface2d/source_terms/runoff/runoff_generation.hpp"
#include "surface2d/source_terms/runoff/step_inputs.hpp"
#include "surface2d/wetting_drying/limits.hpp"

namespace scau::surface2d {
namespace {

void validate_step_inputs(const mesh::Mesh& mesh, const SurfaceState& state, const StepConfig& config) {
    if (!std::isfinite(config.dt) || config.dt <= 0.0) {
        throw std::invalid_argument("step dt must be positive");
    }
    if (!std::isfinite(config.cfl_safety) || config.cfl_safety <= 0.0) {
        throw std::invalid_argument("CFL_safety must be positive");
    }
    if (!std::isfinite(config.c_rollback) || config.c_rollback <= 0.0) {
        throw std::invalid_argument("C_rollback must be positive");
    }
    if (!std::isfinite(config.h_min) || config.h_min < 0.0) {
        throw std::invalid_argument("h_min must be non-negative");
    }
    if (!std::isfinite(config.gravity) || config.gravity <= 0.0) {
        throw std::invalid_argument("gravity must be positive");
    }
    if (mesh.cells.empty()) {
        throw std::invalid_argument("step mesh must contain at least one cell");
    }
    validate_state_matches_mesh(state, mesh);
}

void apply_depth_update(
    SurfaceState& state,
    const StepConfig& config,
    const std::vector<CellStepDiagnostics>& cells,
    const GeometryCache& geometry) {
    for (std::size_t cell_index = 0; cell_index < state.cells.size(); ++cell_index) {
        const core::Real area = geometry.cell_areas[cell_index];
        if (area > 0.0) {
            const core::Real dh = config.dt * cells[cell_index].mass_residual / area;
            state.cells[cell_index].conserved.h = nonnegative_depth_after_increment(
                state.cells[cell_index].conserved.h,
                dh);
        }
    }
}

EdgeStepDiagnostics edge_step_diagnostics(
    const EdgeAdjacency& adjacency,
    const SurfaceState& state,
    const DpmFields& dpm_fields,
    const EdgeFlux& flux,
    bool assemble_wb_pairing) {
    EdgeStepDiagnostics diagnostics{
        .mass_flux = flux.mass,
        .momentum_flux_n = flux.momentum_n,
        .momentum_x = flux.momentum_x,
        .momentum_y = flux.momentum_y,
    };

    // Main-spec 5.6: hard-block edges (omega_edge = 0) assemble no WB
    // pressure/source pairing; soft-block and regular edges do.
    if (adjacency.is_internal() && assemble_wb_pairing) {
        const auto& left = state.cells[*adjacency.left];
        const auto& right = state.cells[*adjacency.right];
        const core::Real h = 0.5 * (left.conserved.h + right.conserved.h);
        diagnostics.pressure_pairing = pressure_pairing_phi_t(
            dpm_fields.cells[*adjacency.left].phi_t,
            dpm_fields.cells[*adjacency.right].phi_t,
            h);
        diagnostics.s_phi_t = s_phi_t_centered(
            dpm_fields.cells[*adjacency.left].phi_t,
            dpm_fields.cells[*adjacency.right].phi_t,
            h);
        diagnostics.residual = diagnostics.pressure_pairing + diagnostics.s_phi_t;
    }

    return diagnostics;
}

StepDiagnostics base_diagnostics(
    const mesh::Mesh& mesh,
    const SurfaceState& state,
    const StepConfig& config,
    const BoundaryConditions& boundary,
    const GeometryCache& geometry) {
    const auto cfl = evaluate_cfl_diagnostics(mesh, state, boundary, geometry, config.dt, config.c_rollback);
    return StepDiagnostics{
        .cell_count = mesh.cells.size(),
        .edge_count = mesh.edges.size(),
        .max_cell_cfl = cfl.max_cell_cfl,
        .rollback_required = cfl.rollback_required,
        .cells = std::vector<CellStepDiagnostics>(mesh.cells.size()),
        .edges = {},
    };
}

CellState open_boundary_outside_state(const CellState& inside) {
    return CellState{
        .conserved = {.h = inside.conserved.h, .hu = inside.conserved.hu, .hv = inside.conserved.hv},
        .eta = inside.eta,
    };
}

void accumulate_momentum_flux_residual(
    CellStepDiagnostics& sink,
    core::Real signed_integrated_flux_x,
    core::Real signed_integrated_flux_y) {
    sink.momentum_residual.x += signed_integrated_flux_x;
    sink.momentum_residual.y += signed_integrated_flux_y;
}

void apply_momentum_update(
    SurfaceState& state,
    const StepConfig& config,
    const std::vector<CellStepDiagnostics>& cells,
    const GeometryCache& geometry) {
    for (std::size_t cell_index = 0; cell_index < state.cells.size(); ++cell_index) {
        const core::Real area = geometry.cell_areas[cell_index];
        if (area <= 0.0) {
            continue;
        }

        state.cells[cell_index].conserved = apply_dry_cell_momentum_limit(
            state.cells[cell_index].conserved,
            config.h_min);
        if (state.cells[cell_index].conserved.h <= config.h_min) {
            continue;
        }
        state.cells[cell_index].conserved.hu += config.dt * cells[cell_index].momentum_residual.x / area;
        state.cells[cell_index].conserved.hv += config.dt * cells[cell_index].momentum_residual.y / area;
    }
}

void apply_coupling_and_friction(
    SurfaceState& state,
    const StepConfig& config,
    const DpmFields& dpm_fields,
    const SourceTermFields& sources,
    const GeometryCache& geometry,
    StepDiagnostics& diagnostics) {
    for (std::size_t cell_index = 0; cell_index < state.cells.size(); ++cell_index) {
        const core::Real area = geometry.cell_areas[cell_index];
        if (area <= 0.0) {
            continue;
        }
        auto& cell = state.cells[cell_index];
        const core::Real phi_t = dpm_fields.cells[cell_index].phi_t;
        if (sources.exchange_volume[cell_index] != 0.0) {
            const auto exchange = exchange_depth_increment(
                sources.exchange_volume[cell_index], cell.conserved.h, phi_t, area);
            cell.conserved.h += exchange.depth_increment;
            diagnostics.exchange_volume += exchange.applied_volume;
        }
        cell.conserved = apply_manning_friction(
            apply_dry_cell_momentum_limit(cell.conserved, config.h_min),
            sources.manning_n[cell_index], config.dt, config.h_min, config.gravity);
    }
}

void apply_source_terms(
    SurfaceState& state,
    const StepConfig& config,
    const DpmFields& dpm_fields,
    const SourceTermFields& sources,
    const GeometryCache& geometry,
    StepDiagnostics& diagnostics) {
    for (std::size_t cell_index = 0; cell_index < state.cells.size(); ++cell_index) {
        const core::Real area = geometry.cell_areas[cell_index];
        if (area <= 0.0) {
            continue;
        }
        auto& cell = state.cells[cell_index];
        const core::Real phi_t = dpm_fields.cells[cell_index].phi_t;

        if (sources.rainfall_rate[cell_index] > 0.0) {
            const core::Real dh = rainfall_depth_increment(
                sources.rainfall_rate[cell_index], phi_t, config.dt);
            cell.conserved.h += dh;
            diagnostics.rainfall_volume += dh * phi_t * area;
        }

        if (sources.exchange_volume[cell_index] != 0.0) {
            const auto exchange = exchange_depth_increment(
                sources.exchange_volume[cell_index], cell.conserved.h, phi_t, area);
            cell.conserved.h += exchange.depth_increment;
            diagnostics.exchange_volume += exchange.applied_volume;
        }

        if (sources.infiltration_rate[cell_index] > 0.0) {
            const core::Real dh = infiltration_depth_decrement(
                sources.infiltration_rate[cell_index], cell.conserved.h, phi_t, config.dt);
            cell.conserved.h -= dh;
            diagnostics.infiltration_volume += dh * phi_t * area;
        }

        cell.conserved = apply_manning_friction(
            apply_dry_cell_momentum_limit(cell.conserved, config.h_min),
            sources.manning_n[cell_index],
            config.dt,
            config.h_min,
            config.gravity);
    }
}

StepDiagnostics run_flux_core(
    const mesh::Mesh& mesh,
    SurfaceState& state,
    const StepConfig& config,
    const DpmFields& dpm_fields,
    const BoundaryConditions& boundary,
    const GeometryCache& geometry) {
    auto diagnostics = base_diagnostics(mesh, state, config, boundary, geometry);
    diagnostics.edges.reserve(mesh.edges.size());
    for (std::size_t edge_index = 0; edge_index < mesh.edges.size(); ++edge_index) {
        const auto& edge = mesh.edges[edge_index];
        const auto& adjacency = geometry.edge_cells[edge_index];

        if (adjacency.is_internal()) {
            const std::size_t left_index = *adjacency.left;
            const std::size_t right_index = *adjacency.right;
            const auto flux = hllc_normal_flux(
                state.cells[left_index],
                state.cells[right_index],
                dpm_fields.edges[edge_index],
                Normal2{.x = edge.normal.x, .y = edge.normal.y},
                config.h_min);
            const bool assemble_wb_pairing = classify_edge(
                dpm_fields.edges[edge_index].omega_edge,
                dpm_fields.edges[edge_index].phi_e_n).wb_pairing_assembled;
            const auto edge_diagnostics = edge_step_diagnostics(adjacency, state, dpm_fields, flux, assemble_wb_pairing);
            diagnostics.edges.push_back(edge_diagnostics);
            const core::Real integrated_flux = edge_diagnostics.mass_flux * edge.length;
            diagnostics.cells[left_index].mass_residual -= integrated_flux;
            diagnostics.cells[right_index].mass_residual += integrated_flux;
            const core::Real integrated_momentum_x = flux.momentum_x * edge.length;
            const core::Real integrated_momentum_y = flux.momentum_y * edge.length;
            accumulate_momentum_flux_residual(diagnostics.cells[left_index], -integrated_momentum_x, -integrated_momentum_y);
            accumulate_momentum_flux_residual(diagnostics.cells[right_index], integrated_momentum_x, integrated_momentum_y);
            continue;
        }

        if (boundary.edges[edge_index] == BoundaryKind::Wall) {
            // Reflective wall (main-spec hard-block / wall boundary): zero mass
            // flux, but the inside cell's hydrostatic pressure pushes back so
            // the closed-polygon pressure sum of a boundary cell balances and a
            // lake at rest stays at rest. Uses the same gravity (9.81) and
            // unscaled 0.5 g h^2 form as the interior HLLC pressure so the
            // balance is exact for uniform fields.
            const std::size_t inside_index = adjacency.inside();
            const core::Real h_inside = state.cells[inside_index].conserved.h;
            const core::Real wall_pressure = 0.5 * 9.81 * h_inside * h_inside;
            diagnostics.edges.push_back(EdgeStepDiagnostics{
                .momentum_flux_n = wall_pressure,
                .momentum_x = wall_pressure * edge.normal.x,
                .momentum_y = wall_pressure * edge.normal.y,
            });

            const core::Real momentum_x = wall_pressure * edge.normal.x * edge.length;
            const core::Real momentum_y = wall_pressure * edge.normal.y * edge.length;
            const core::Real signed_momentum_x = adjacency.left.has_value() ? -momentum_x : momentum_x;
            const core::Real signed_momentum_y = adjacency.left.has_value() ? -momentum_y : momentum_y;
            accumulate_momentum_flux_residual(diagnostics.cells[inside_index], signed_momentum_x, signed_momentum_y);
            continue;
        }

        const std::size_t inside_index = adjacency.inside();
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
            .momentum_x = flux.momentum_x,
            .momentum_y = flux.momentum_y,
        };
        diagnostics.edges.push_back(edge_diagnostics);

        const core::Real integrated_flux = edge_diagnostics.mass_flux * edge.length;
        const core::Real signed_integrated_flux = adjacency.left.has_value() ? -integrated_flux : integrated_flux;
        diagnostics.cells[inside_index].mass_residual += signed_integrated_flux;

        const core::Real signed_momentum_x = adjacency.left.has_value() ? -flux.momentum_x * edge.length : flux.momentum_x * edge.length;
        const core::Real signed_momentum_y = adjacency.left.has_value() ? -flux.momentum_y * edge.length : flux.momentum_y * edge.length;
        accumulate_momentum_flux_residual(diagnostics.cells[inside_index], signed_momentum_x, signed_momentum_y);
    }
    if (diagnostics.rollback_required) {
        return diagnostics;
    }
    apply_depth_update(state, config, diagnostics.cells, geometry);
    apply_momentum_update(state, config, diagnostics.cells, geometry);
    return diagnostics;
}

}  // namespace

void apply_ground_runoff_stage(
    SurfaceState& state,
    const StepConfig& config,
    const DpmFields& dpm_fields,
    const RunoffStepInputs& inputs,
    RunoffState& runoff_state,
    const GeometryCache& geometry,
    StepDiagnostics& diagnostics) {
    for (std::size_t i = 0; i < state.cells.size(); ++i) {
        const core::Real area = geometry.cell_areas[i];
        if (area <= 0.0) {
            continue;
        }
        const core::Real phi_t = dpm_fields.cells[i].phi_t;

        RunoffCellInputs cell_inputs;
        cell_inputs.rainfall_rate = inputs.rainfall_rate[i];
        cell_inputs.phi_t = phi_t;
        cell_inputs.cell_area = area;

        RunoffCellParams params;
        params.pervious_fraction = inputs.fields.pervious_fraction[i];
        params.impervious_fraction = inputs.fields.impervious_fraction[i];
        params.roof_fraction = inputs.fields.roof_fraction[i];
        params.initial_abstraction_capacity = inputs.fields.initial_abstraction_capacity[i];
        params.depression_storage_capacity = inputs.fields.depression_storage_capacity[i];
        params.roof_abstraction_capacity = inputs.fields.roof_abstraction_capacity[i];
        params.roof_storage_capacity = inputs.fields.roof_storage_capacity[i];
        params.roof_drain_capacity = 0.0;
        params.soil = inputs.lut.at(inputs.fields.soil_type[i]);

        RunoffCellState cell_state;
        cell_state.cumulative_infiltration = runoff_state.cumulative_infiltration[i];
        cell_state.ponding_time = runoff_state.ponding_time[i];
        cell_state.abstraction_filled = runoff_state.abstraction_filled[i];
        cell_state.depression_storage_filled = runoff_state.depression_storage_filled[i];
        cell_state.roof_abstraction_filled = runoff_state.roof_abstraction_filled[i];
        cell_state.roof_pending_volume = runoff_state.roof_pending_volume[i];

        const GroundRunoffResult g = evaluate_ground_runoff(
            cell_inputs, params, cell_state, config.dt, inputs.f_inf_floor);

        state.cells[i].conserved.h += g.surface_added_volume / (phi_t * area);

        runoff_state.cumulative_infiltration[i] = cell_state.cumulative_infiltration;
        runoff_state.ponding_time[i] = cell_state.ponding_time;
        runoff_state.abstraction_filled[i] = cell_state.abstraction_filled;
        runoff_state.depression_storage_filled[i] = cell_state.depression_storage_filled;

        const core::Real area_ground =
            (params.pervious_fraction + params.impervious_fraction) * area;
        diagnostics.rainfall_volume += cell_inputs.rainfall_rate * config.dt * area_ground;
        diagnostics.surface_added_volume += g.surface_added_volume;
        diagnostics.infiltration_volume += g.infiltration_volume;
        diagnostics.abstraction_volume += g.abstraction_volume;
        diagnostics.depression_storage_delta_volume += g.depression_storage_delta_volume;
    }
}

StepDiagnostics advance_one_step_cpu(const mesh::Mesh& mesh, SurfaceState& state, const StepConfig& config) {
    validate_step_inputs(mesh, state, config);
    const auto geometry = GeometryCache::for_mesh(mesh);
    return base_diagnostics(mesh, state, config, BoundaryConditions::for_mesh(mesh), geometry);
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
    return advance_one_step_cpu(mesh, state, config, dpm_fields, boundary, SourceTermFields::for_mesh(mesh));
}

StepDiagnostics advance_one_step_cpu(
    const mesh::Mesh& mesh,
    SurfaceState& state,
    const StepConfig& config,
    const DpmFields& dpm_fields,
    const BoundaryConditions& boundary,
    const SourceTermFields& sources) {
    const auto geometry = GeometryCache::for_mesh(mesh);
    return advance_one_step_cpu(mesh, state, config, dpm_fields, boundary, sources, geometry);
}

StepDiagnostics advance_one_step_cpu(
    const mesh::Mesh& mesh,
    SurfaceState& state,
    const StepConfig& config,
    const DpmFields& dpm_fields,
    const BoundaryConditions& boundary,
    const SourceTermFields& sources,
    const GeometryCache& geometry) {
    validate_step_inputs(mesh, state, config);
    validate_dpm_fields_match_mesh(dpm_fields, mesh);
    validate_boundary_conditions_match_mesh(boundary, mesh);
    validate_source_term_fields_match_mesh(sources, mesh);
    validate_geometry_cache_matches_mesh(geometry, mesh);
    auto diagnostics = run_flux_core(mesh, state, config, dpm_fields, boundary, geometry);
    if (diagnostics.rollback_required) {
        return diagnostics;
    }
    apply_source_terms(state, config, dpm_fields, sources, geometry, diagnostics);
    return diagnostics;
}

StepDiagnostics advance_one_step_cpu(
    const mesh::Mesh& mesh,
    SurfaceState& state,
    const StepConfig& config,
    const DpmFields& dpm_fields,
    const BoundaryConditions& boundary,
    const SourceTermFields& sources,
    const GeometryCache& geometry,
    const RunoffStepInputs& runoff_inputs,
    RunoffState& runoff_state) {
    validate_step_inputs(mesh, state, config);
    validate_dpm_fields_match_mesh(dpm_fields, mesh);
    validate_boundary_conditions_match_mesh(boundary, mesh);
    validate_source_term_fields_match_mesh(sources, mesh);
    validate_geometry_cache_matches_mesh(geometry, mesh);
    validate_runoff_step_inputs_match_mesh(runoff_inputs, mesh);
    validate_runoff_state_match_mesh(runoff_state, mesh);

    auto diagnostics = run_flux_core(mesh, state, config, dpm_fields, boundary, geometry);
    if (diagnostics.rollback_required) {
        return diagnostics;  // runoff_state untouched
    }
    apply_ground_runoff_stage(state, config, dpm_fields, runoff_inputs, runoff_state, geometry, diagnostics);
    apply_coupling_and_friction(state, config, dpm_fields, sources, geometry, diagnostics);
    return diagnostics;
}

}  // namespace scau::surface2d

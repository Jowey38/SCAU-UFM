#pragma once

#include <cstddef>
#include <vector>

#include "core/types.hpp"
#include "mesh/mesh.hpp"
#include "surface2d/boundary/conditions.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/geometry/cache.hpp"
#include "surface2d/source_terms/fields.hpp"
#include "surface2d/source_terms/runoff/step_inputs.hpp"
#include "surface2d/state/state.hpp"

namespace scau::surface2d {

struct StepConfig {
    core::Real dt{0.0};
    core::Real cfl_safety{0.45};
    core::Real c_rollback{1.0};
    core::Real h_min{1.0e-8};
    core::Real gravity{9.81};
};

struct EdgeStepDiagnostics {
    core::Real mass_flux{0.0};
    core::Real momentum_flux_n{0.0};
    // Per-edge Cartesian momentum flux (along the edge normal, pre-sign/length).
    // Lets callers inspect a single edge's vector momentum contribution without
    // reading the wall-contaminated cell-total residual.
    core::Real momentum_x{0.0};
    core::Real momentum_y{0.0};
    core::Real pressure_pairing{0.0};
    core::Real s_phi_t{0.0};
    core::Real residual{0.0};
};

struct MomentumResidual {
    core::Real x{0.0};
    core::Real y{0.0};
};

struct CellStepDiagnostics {
    core::Real mass_residual{0.0};
    MomentumResidual momentum_residual{};
};

struct StepDiagnostics {
    std::size_t cell_count{0U};
    std::size_t edge_count{0U};
    core::Real max_cell_cfl{0.0};
    bool rollback_required{false};
    // Plan volumes (m^3) actually applied this step, for system mass audit.
    // All zero when the step rolls back. Ground runoff closure (M247-C):
    // rainfall_volume == surface_added_volume + infiltration_volume
    //                    + abstraction_volume + depression_storage_delta_volume.
    core::Real rainfall_volume{0.0};            // gross rain on ground area
    core::Real surface_added_volume{0.0};       // net ground runoff added to h
    core::Real infiltration_volume{0.0};
    core::Real abstraction_volume{0.0};
    core::Real depression_storage_delta_volume{0.0};
    core::Real roof_to_swmm_requested_volume{0.0};
    core::Real roof_to_swmm_accepted_volume{0.0};
    core::Real roof_to_swmm_rejected_volume{0.0};
    core::Real roof_pending_delta_volume{0.0};
    core::Real roof_overflow_to_surface_volume{0.0};
    bool missing_roof_overflow_target{false};
    core::Real exchange_volume{0.0};
    std::vector<CellStepDiagnostics> cells;
    std::vector<EdgeStepDiagnostics> edges;
};

// Ground runoff source stage (M247-C): per cell, gather the ground portion of
// runoff_state, run evaluate_ground_runoff using inputs.rainfall_rate and the
// per-cell fields/soil, add the net ground runoff depth to h (zero momentum),
// scatter state back, and accumulate the ground volume audit into diagnostics.
// Roof fields of runoff_state are not touched (M247-D owns the roof chain).
void apply_ground_runoff_stage(
    SurfaceState& state,
    const StepConfig& config,
    const DpmFields& dpm_fields,
    const RunoffStepInputs& inputs,
    RunoffState& runoff_state,
    const GeometryCache& geometry,
    StepDiagnostics& diagnostics);

[[nodiscard]] StepDiagnostics advance_one_step_cpu(
    const mesh::Mesh& mesh,
    SurfaceState& state,
    const StepConfig& config);

[[nodiscard]] StepDiagnostics advance_one_step_cpu(
    const mesh::Mesh& mesh,
    SurfaceState& state,
    const StepConfig& config,
    const DpmFields& dpm_fields);

[[nodiscard]] StepDiagnostics advance_one_step_cpu(
    const mesh::Mesh& mesh,
    SurfaceState& state,
    const StepConfig& config,
    const DpmFields& dpm_fields,
    const BoundaryConditions& boundary);

[[nodiscard]] StepDiagnostics advance_one_step_cpu(
    const mesh::Mesh& mesh,
    SurfaceState& state,
    const StepConfig& config,
    const DpmFields& dpm_fields,
    const BoundaryConditions& boundary,
    const SourceTermFields& sources);

// Hot-path overload: callers stepping the same mesh repeatedly should build
// the GeometryCache once and pass it here to avoid per-step rebuilds.
[[nodiscard]] StepDiagnostics advance_one_step_cpu(
    const mesh::Mesh& mesh,
    SurfaceState& state,
    const StepConfig& config,
    const DpmFields& dpm_fields,
    const BoundaryConditions& boundary,
    const SourceTermFields& sources,
    const GeometryCache& geometry);

// Runoff-aware hot-path overload (M247-C). Same flux/CFL/rollback path as the
// SourceTermFields overload, but the source order is
//   flux -> ground runoff -> coupling exchange -> friction
// (supersedes M241's rainfall/infiltration). runoff_state is mutated only on an
// accepted step (untouched on rollback). sources supplies manning_n and
// exchange_volume; its rainfall/infiltration are NOT read on this path.
[[nodiscard]] StepDiagnostics advance_one_step_cpu(
    const mesh::Mesh& mesh,
    SurfaceState& state,
    const StepConfig& config,
    const DpmFields& dpm_fields,
    const BoundaryConditions& boundary,
    const SourceTermFields& sources,
    const GeometryCache& geometry,
    const RunoffStepInputs& runoff_inputs,
    RunoffState& runoff_state);

}  // namespace scau::surface2d

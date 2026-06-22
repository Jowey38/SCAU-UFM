#pragma once

#include <cstddef>
#include <vector>

#include "core/types.hpp"
#include "mesh/mesh.hpp"
#include "surface2d/boundary/conditions.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/geometry/cache.hpp"
#include "surface2d/source_terms/fields.hpp"
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
    core::Real wb_pressure{0.0};  // F_p,e: phi_t-scaled WB pressure flux magnitude
    core::Real s_topo{0.0};       // S_topo^(left) diagnostic for the edge
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
    // Plan volumes (phi_t * dh * area) actually applied this step, for
    // system mass audit. All zero when the step rolls back.
    core::Real rainfall_volume{0.0};
    core::Real infiltration_volume{0.0};
    core::Real exchange_volume{0.0};
    std::vector<CellStepDiagnostics> cells;
    std::vector<EdgeStepDiagnostics> edges;
};

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

}  // namespace scau::surface2d

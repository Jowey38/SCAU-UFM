#pragma once

#include <cstddef>
#include <vector>

#include "core/types.hpp"
#include "mesh/mesh.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/state/state.hpp"

namespace scau::surface2d {

struct StepConfig {
    core::Real dt{0.0};
    core::Real cfl_safety{0.45};
    core::Real c_rollback{1.0};
};

struct EdgeStepDiagnostics {
    core::Real mass_flux{0.0};
    core::Real momentum_flux_n{0.0};
    core::Real pressure_pairing{0.0};
    core::Real s_phi_t{0.0};
    core::Real residual{0.0};
};

struct StepDiagnostics {
    std::size_t cell_count{0U};
    std::size_t edge_count{0U};
    core::Real max_cell_cfl{0.0};
    bool rollback_required{false};
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

}  // namespace scau::surface2d

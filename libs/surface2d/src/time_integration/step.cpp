#include "surface2d/time_integration/step.hpp"

#include <stdexcept>

namespace scau::surface2d {

StepDiagnostics advance_one_step_cpu(const mesh::Mesh& mesh, SurfaceState& state, const StepConfig& config) {
    if (config.dt <= 0.0) {
        throw std::invalid_argument("step dt must be positive");
    }
    if (config.cfl_safety <= 0.0) {
        throw std::invalid_argument("CFL_safety must be positive");
    }
    if (config.c_rollback <= 0.0) {
        throw std::invalid_argument("C_rollback must be positive");
    }
    validate_state_matches_mesh(state, mesh);

    return StepDiagnostics{
        .cell_count = mesh.cells.size(),
        .edge_count = mesh.edges.size(),
        .max_cell_cfl = 0.0,
        .rollback_required = false,
    };
}

}  // namespace scau::surface2d

#include "surface2d/backend.hpp"

#include <stdexcept>

namespace scau::surface2d {

BackendCapabilities query_backend_capabilities(BackendKind kind) noexcept {
    switch (kind) {
        case BackendKind::cpu_reference:
            return {
                .kind = kind,
                .available = true,
                .deterministic = true,
                .double_precision = true,
                .supports_snapshot_restore = true,
            };
        case BackendKind::cuda_deterministic:
        case BackendKind::cuda_performance:
            return {
                .kind = kind,
                .available = false,
                .deterministic = false,
                .double_precision = false,
                .supports_snapshot_restore = false,
            };
    }
    return {.kind = kind};
}

StepDiagnostics advance_one_step(
    BackendKind kind,
    const mesh::Mesh& mesh,
    SurfaceState& state,
    const StepConfig& config) {
    if (kind == BackendKind::cpu_reference) {
        return advance_one_step_cpu(mesh, state, config);
    }
    throw std::runtime_error(
        "requested Surface2D backend is unavailable; G9 remains pending");
}

}  // namespace scau::surface2d

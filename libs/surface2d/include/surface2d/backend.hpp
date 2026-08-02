#pragma once

#include "mesh/mesh.hpp"
#include "surface2d/state/state.hpp"
#include "surface2d/time_integration/step.hpp"

namespace scau::surface2d {

enum class BackendKind {
    cpu_reference,
    cuda_deterministic,
    cuda_performance,
};

struct BackendCapabilities {
    BackendKind kind{BackendKind::cpu_reference};
    bool available{false};
    bool deterministic{false};
    bool double_precision{false};
    bool supports_snapshot_restore{false};
};

[[nodiscard]] BackendCapabilities query_backend_capabilities(BackendKind kind) noexcept;

// Minimal backend seam. The CPU path delegates to the existing reference step.
// CUDA kinds fail closed until a real implementation and G9 evidence exist.
[[nodiscard]] StepDiagnostics advance_one_step(
    BackendKind kind,
    const mesh::Mesh& mesh,
    SurfaceState& state,
    const StepConfig& config);

}  // namespace scau::surface2d

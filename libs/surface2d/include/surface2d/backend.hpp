#pragma once

#include "mesh/mesh.hpp"
#include "surface2d/boundary/conditions.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/geometry/cache.hpp"
#include "surface2d/source_terms/fields.hpp"
#include "surface2d/source_terms/runoff/roof_step.hpp"
#include "surface2d/source_terms/runoff/step_inputs.hpp"
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

// cpu_reference is always available. cuda_deterministic reports available
// only when the backend was compiled in (SCAU_ENABLE_CUDA) AND a CUDA device
// is present at runtime (M284/G9); it is deterministic, double precision and
// snapshot-capable by construction (M280 pattern). cuda_performance stays
// fail-closed until deterministic CUDA is the reference (M266).
[[nodiscard]] BackendCapabilities query_backend_capabilities(BackendKind kind) noexcept;

// Backend seam. The CPU path delegates to the existing reference step; the
// deterministic CUDA path delegates to advance_one_step_cuda. Requesting an
// unavailable backend throws before any state mutation (M266).
[[nodiscard]] StepDiagnostics advance_one_step(
    BackendKind kind,
    const mesh::Mesh& mesh,
    SurfaceState& state,
    const StepConfig& config);

[[nodiscard]] StepDiagnostics advance_one_step(
    BackendKind kind,
    const mesh::Mesh& mesh,
    SurfaceState& state,
    const StepConfig& config,
    const DpmFields& dpm_fields,
    const BoundaryConditions& boundary,
    const SourceTermFields& sources,
    const GeometryCache& geometry);

[[nodiscard]] StepDiagnostics advance_one_step(
    BackendKind kind,
    const mesh::Mesh& mesh,
    SurfaceState& state,
    const StepConfig& config,
    const DpmFields& dpm_fields,
    const BoundaryConditions& boundary,
    const SourceTermFields& sources,
    const GeometryCache& geometry,
    const RunoffStepInputs& runoff_inputs,
    RunoffState& runoff_state);

[[nodiscard]] StepDiagnostics advance_one_step(
    BackendKind kind,
    const mesh::Mesh& mesh,
    SurfaceState& state,
    const StepConfig& config,
    const DpmFields& dpm_fields,
    const BoundaryConditions& boundary,
    const SourceTermFields& sources,
    const GeometryCache& geometry,
    const RunoffStepInputs& runoff_inputs,
    const RoofStepInputs& roof_inputs,
    RunoffState& runoff_state);

}  // namespace scau::surface2d

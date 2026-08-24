#include "surface2d/backend.hpp"

#include <stdexcept>

#if defined(SCAU_SURFACE2D_HAS_CUDA)
#include "surface2d/backends/cuda_backend.hpp"
#endif

namespace scau::surface2d {
namespace {

[[noreturn]] void throw_unavailable() {
    throw std::runtime_error(
        "requested Surface2D backend is unavailable; G9 remains pending");
}

bool cuda_deterministic_available() noexcept {
#if defined(SCAU_SURFACE2D_HAS_CUDA)
    return cuda_backend_device_available();
#else
    return false;
#endif
}

}  // namespace

BackendCapabilities query_backend_capabilities(BackendKind kind) noexcept {
    if (kind == BackendKind::cpu_reference) {
        return BackendCapabilities{
            .kind = kind,
            .available = true,
            .deterministic = true,
            .double_precision = true,
            .supports_snapshot_restore = true,
        };
    }
    if (kind == BackendKind::cuda_deterministic) {
        const bool available = cuda_deterministic_available();
        return BackendCapabilities{
            .kind = kind,
            .available = available,
            .deterministic = available,
            .double_precision = available,
            .supports_snapshot_restore = available,
        };
    }
    // cuda_performance stays fail-closed until deterministic CUDA is the
    // reference (M266): atomics/CUDA Graphs can never be the sole
    // correctness path.
    return BackendCapabilities{
        .kind = kind,
        .available = false,
        .deterministic = false,
        .double_precision = false,
        .supports_snapshot_restore = false,
    };
}

StepDiagnostics advance_one_step(
    BackendKind kind,
    const mesh::Mesh& mesh,
    SurfaceState& state,
    const StepConfig& config) {
    if (kind == BackendKind::cpu_reference) {
        return advance_one_step_cpu(mesh, state, config);
    }
#if defined(SCAU_SURFACE2D_HAS_CUDA)
    if (kind == BackendKind::cuda_deterministic && cuda_deterministic_available()) {
        // The 3-arg CPU overload only validates and reports base CFL
        // diagnostics (no state mutation); delegate so both backends stay
        // behaviorally equal on this entry point.
        return advance_one_step_cpu(mesh, state, config);
    }
#endif
    throw_unavailable();
}

StepDiagnostics advance_one_step(
    BackendKind kind,
    const mesh::Mesh& mesh,
    SurfaceState& state,
    const StepConfig& config,
    const DpmFields& dpm_fields,
    const BoundaryConditions& boundary,
    const SourceTermFields& sources,
    const GeometryCache& geometry) {
    if (kind == BackendKind::cpu_reference) {
        return advance_one_step_cpu(mesh, state, config, dpm_fields, boundary, sources, geometry);
    }
#if defined(SCAU_SURFACE2D_HAS_CUDA)
    if (kind == BackendKind::cuda_deterministic && cuda_deterministic_available()) {
        return advance_one_step_cuda(mesh, state, config, dpm_fields, boundary, sources, geometry);
    }
#endif
    throw_unavailable();
}

StepDiagnostics advance_one_step(
    BackendKind kind,
    const mesh::Mesh& mesh,
    SurfaceState& state,
    const StepConfig& config,
    const DpmFields& dpm_fields,
    const BoundaryConditions& boundary,
    const SourceTermFields& sources,
    const GeometryCache& geometry,
    const RunoffStepInputs& runoff_inputs,
    RunoffState& runoff_state) {
    if (kind == BackendKind::cpu_reference) {
        return advance_one_step_cpu(
            mesh, state, config, dpm_fields, boundary, sources, geometry, runoff_inputs, runoff_state);
    }
#if defined(SCAU_SURFACE2D_HAS_CUDA)
    if (kind == BackendKind::cuda_deterministic && cuda_deterministic_available()) {
        return advance_one_step_cuda(
            mesh, state, config, dpm_fields, boundary, sources, geometry, runoff_inputs, runoff_state);
    }
#endif
    throw_unavailable();
}

StepDiagnostics advance_one_step(
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
    RunoffState& runoff_state) {
    if (kind == BackendKind::cpu_reference) {
        return advance_one_step_cpu(
            mesh, state, config, dpm_fields, boundary, sources, geometry,
            runoff_inputs, roof_inputs, runoff_state);
    }
#if defined(SCAU_SURFACE2D_HAS_CUDA)
    if (kind == BackendKind::cuda_deterministic && cuda_deterministic_available()) {
        return advance_one_step_cuda(
            mesh, state, config, dpm_fields, boundary, sources, geometry,
            runoff_inputs, roof_inputs, runoff_state);
    }
#endif
    throw_unavailable();
}

}  // namespace scau::surface2d

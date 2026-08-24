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

// Deterministic CUDA backend for the Surface2D step (M284/G9). Only declared
// here; the implementation lives in the .cu translation unit and is compiled
// only when SCAU_ENABLE_CUDA is ON. Callers must go through
// surface2d::advance_one_step (backend.cpp), which keeps the M266 fail-closed
// contract when the backend is not compiled or no device is present.
//
// Determinism contract (M280 pattern):
//   - per-edge kernel computes fluxes/WB/CVC once per edge (identical shared
//     SCAU_HD numerics as the CPU reference);
//   - per-cell gather in ascending global edge index order (bitwise-identical
//     association to the CPU edge loop; no atomics anywhere);
//   - scalar volume diagnostics via fixed-order 256-wide block-tree reduction
//     with sequential host combination;
//   - double precision throughout; compiled with --fmad=false so device
//     contraction matches the /fp:precise host.

namespace scau::surface2d {

// True when a CUDA device is present and usable (never throws).
[[nodiscard]] bool cuda_backend_device_available() noexcept;

// Records for the G9 evidence log. Zero/empty when no device is available.
struct CudaBackendDeviceInfo {
    int compute_capability_major{0};
    int compute_capability_minor{0};
    int driver_version{0};
    int runtime_version{0};
    char device_name[256]{};
};
[[nodiscard]] CudaBackendDeviceInfo cuda_backend_device_info() noexcept;

// Device snapshot/restore evidence hook (M266 exit condition): uploads the
// state, snapshots it device-to-device, perturbs every value on the device,
// restores from the snapshot and downloads. Returns true iff the roundtrip
// is bitwise identical to the input. Throws when no device is available.
[[nodiscard]] bool cuda_snapshot_restore_roundtrip(const SurfaceState& state);

[[nodiscard]] StepDiagnostics advance_one_step_cuda(
    const mesh::Mesh& mesh,
    SurfaceState& state,
    const StepConfig& config,
    const DpmFields& dpm_fields,
    const BoundaryConditions& boundary,
    const SourceTermFields& sources,
    const GeometryCache& geometry);

[[nodiscard]] StepDiagnostics advance_one_step_cuda(
    const mesh::Mesh& mesh,
    SurfaceState& state,
    const StepConfig& config,
    const DpmFields& dpm_fields,
    const BoundaryConditions& boundary,
    const SourceTermFields& sources,
    const GeometryCache& geometry,
    const RunoffStepInputs& runoff_inputs,
    RunoffState& runoff_state);

[[nodiscard]] StepDiagnostics advance_one_step_cuda(
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

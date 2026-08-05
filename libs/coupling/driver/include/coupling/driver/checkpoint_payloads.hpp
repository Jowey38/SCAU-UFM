#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "coupling/core/state.hpp"
#include "coupling/driver/checkpoint_coordinator.hpp"
#include "coupling/driver/dflowfm_checkpoint.hpp"
#include "surface2d/state/state.hpp"

namespace scau::coupling::driver {

// Deterministic checkpoint payload hashing and per-module
// PreparedModuleCheckpoint builders for the SimDriver epoch commit protocol.
//
// Hashing is FNV-1a 64-bit over the exact bit patterns of every double
// (std::bit_cast) plus container sizes and enum tags — no dependency, fully
// deterministic across runs on the same platform, and sensitive to one-ULP
// perturbations. The hash string format is "fnv1a64:<16 hex digits>".

[[nodiscard]] std::string hash_surface_state(const surface2d::SurfaceState& state);

// Covers cells (volume, aggregate and endpoint-owned deficits, phi_t, h,
// area), runtime counters, and the pending event queue.
[[nodiscard]] std::string hash_coupling_snapshot(const core::CouplingSnapshot& snapshot);

// In-memory payloads: payload_reference is "memory://<module>/<epoch>".
[[nodiscard]] PreparedModuleCheckpoint prepare_surface2d_checkpoint(
    const surface2d::SurfaceState& state,
    std::uint64_t epoch,
    double logical_time);

[[nodiscard]] PreparedModuleCheckpoint prepare_coupling_checkpoint(
    const core::CouplingSnapshot& snapshot,
    std::uint64_t epoch,
    double logical_time);

[[nodiscard]] PreparedModuleCheckpoint prepare_sim_driver_checkpoint(
    std::size_t completed_coupling_steps,
    std::uint64_t epoch,
    double logical_time);

// SWMM exposes no hotstart/state-reload API: the record is metadata-only
// (elapsed engine time) under schema "swmm-noreload-v1" and exists so the
// commit verdict can attest that the engine boundary was observed.
[[nodiscard]] PreparedModuleCheckpoint prepare_swmm_checkpoint(
    double swmm_elapsed_time,
    std::uint64_t epoch,
    double logical_time);

// D-Flow FM record: metadata-only ("dflowfm-elapsed-v1") unless a validated
// restart-file checkpoint is supplied, in which case the record carries the
// file reference under schema "dflowfm-restart-file-v1".
[[nodiscard]] PreparedModuleCheckpoint prepare_dflowfm_checkpoint_record(
    double dflowfm_elapsed_time,
    const DFlowFMCheckpoint* file_checkpoint,
    std::uint64_t epoch,
    double logical_time);

}  // namespace scau::coupling::driver

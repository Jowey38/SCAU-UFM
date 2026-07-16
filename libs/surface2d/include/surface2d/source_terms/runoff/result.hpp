#pragma once

#include "core/types.hpp"

namespace scau::surface2d {

// Why CouplingLib rejected (part of) a roof drainage request.
enum class RoofDrainageRejectionReason {
    None,
    InvalidTargetNode,
    EngineUnavailable,
    NodeSurcharged,
    CapacityLimited,
    BackwaterBlocked,
    HealthBlocked,
};

// Per-cell, per-substep diagnostic flags. Warnings and fail-closed markers.
struct RunoffDiagnosticFlags {
    bool roof_drain_capacity_limited{false};
    bool roof_node_rejected{false};
    bool roof_pending_saturated{false};
    bool roof_overflow_to_surface{false};
    bool surface_storage_amplified_by_low_phi_t{false};
    bool green_ampt_ponding_started{false};
    bool missing_roof_overflow_target{false};
    bool mass_balance_violation{false};
};

// Audited volumes (m^3) for one cell over one substep. surface_added_volume is
// GROUND-ONLY and excludes roof overflow; the two are disjoint in the closure.
struct RunoffGenerationResult {
    core::Real rainfall_volume{0.0};
    core::Real surface_added_volume{0.0};
    core::Real infiltration_volume{0.0};
    core::Real abstraction_volume{0.0};
    core::Real depression_storage_delta_volume{0.0};
    core::Real roof_to_swmm_requested_volume{0.0};
    core::Real roof_to_swmm_accepted_volume{0.0};
    core::Real roof_to_swmm_rejected_volume{0.0};
    core::Real roof_pending_delta_volume{0.0};
    core::Real roof_overflow_to_surface_volume{0.0};
    core::Real rejected_fail_closed_volume{0.0};
    RunoffDiagnosticFlags flags{};
};

// Roof direct-drain request emitted to CouplingLib (M247-D consumes this).
struct RoofDrainageIntent {
    int source_cell_index{-1};
    int target_swmm_node_index{-1};
    core::Real requested_volume{0.0};
    core::Real source_roof_area{0.0};
};

// CouplingLib's response to a RoofDrainageIntent.
struct RoofDrainageAcceptance {
    core::Real requested_volume{0.0};
    core::Real accepted_volume{0.0};
    core::Real rejected_volume{0.0};
    RoofDrainageRejectionReason rejection_reason{RoofDrainageRejectionReason::None};
};

// Signed mass-balance error (m^3): rainfall_volume minus the sum of all sinks.
// Zero (within tolerance) means the substep is conservative.
[[nodiscard]] core::Real runoff_mass_balance_error(const RunoffGenerationResult& result);

// Sets result.flags.mass_balance_violation when the absolute balance error
// exceeds epsilon_mass_abs + epsilon_mass_rel * max(1, rainfall_volume).
// Returns true when closed (no violation).
bool check_runoff_mass_closure(
    RunoffGenerationResult& result,
    core::Real epsilon_mass_abs,
    core::Real epsilon_mass_rel);

}  // namespace scau::surface2d

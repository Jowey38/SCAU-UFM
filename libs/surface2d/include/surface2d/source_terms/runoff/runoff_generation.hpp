#pragma once

#include "core/types.hpp"
#include "surface2d/source_terms/runoff/green_ampt.hpp"
#include "surface2d/source_terms/runoff/result.hpp"
#include "surface2d/source_terms/runoff/soil_params.hpp"

namespace scau::surface2d {

// Per-cell meteorological / geometric inputs for one substep.
struct RunoffCellInputs {
    core::Real rainfall_rate{0.0};  // m/s
    core::Real phi_t{1.0};
    core::Real cell_area{0.0};      // m^2 (plan area)
    core::Real surface_depth{0.0};  // m (existing ponded surface water depth h)
};

// Per-cell static parameters. Area fractions are dimensionless; abstraction and
// depression capacities are depths (m). roof_drain_capacity is m^3/s.
// roof_storage_capacity is the max pending buffer expressed as a depth over the
// roof area (max pending volume = roof_storage_capacity * roof_area).
struct RunoffCellParams {
    core::Real pervious_fraction{1.0};
    core::Real impervious_fraction{0.0};
    core::Real roof_fraction{0.0};
    core::Real initial_abstraction_capacity{0.0};
    core::Real depression_storage_capacity{0.0};
    core::Real roof_abstraction_capacity{0.0};
    core::Real roof_storage_capacity{0.0};
    core::Real roof_drain_capacity{0.0};
    SoilParams soil{};
};

// Per-cell mutable runoff state (scalar view; the SoA RunoffState is gathered
// into this by the M247-C step seam). roof_storage_filled from the SoA state is
// intentionally not used in the single-buffer roof model.
struct RunoffCellState {
    core::Real cumulative_infiltration{0.0};
    core::Real ponding_time{-1.0};
    core::Real abstraction_filled{0.0};
    core::Real depression_storage_filled{0.0};
    core::Real roof_abstraction_filled{0.0};
    core::Real roof_pending_volume{0.0};
};

// Ground (pervious + impervious) outcome for one substep, in m^3.
struct GroundRunoffResult {
    core::Real surface_added_volume{0.0};
    core::Real infiltration_volume{0.0};          // pure-liquid, rain-derived
    core::Real ponded_infiltration_volume{0.0};   // pure-liquid, ponded-h-derived
    core::Real abstraction_volume{0.0};
    core::Real depression_storage_delta_volume{0.0};
    bool ponding_started{false};
};

// Pervious+impervious chain: abstraction -> depression -> Green-Ampt (pervious
// only) -> remaining is runoff. Mutates the ground portion of state. Fail-closed
// on non-finite/out-of-domain inputs and on fractions outside [0,1].
[[nodiscard]] GroundRunoffResult evaluate_ground_runoff(
    const RunoffCellInputs& inputs,
    const RunoffCellParams& params,
    RunoffCellState& state,
    core::Real dt,
    core::Real f_inf_floor);

// Roof emit outcome for one substep (m^3). roof_input_volume is the water added
// to the pending buffer this substep; requested_volume is the drain request.
struct RoofEmitResult {
    core::Real roof_abstraction_volume{0.0};
    core::Real roof_input_volume{0.0};
    core::Real requested_volume{0.0};
    bool drain_capacity_limited{false};
};

// Roof chain part A: initial abstraction (one-way loss) then add the remainder
// to roof_pending_volume; compute the drain request capped by roof_drain_capacity.
// Mutates the roof portion of state. Does NOT subtract accepted volume or compute
// overflow -- that is apply_roof_drainage_acceptance. Fail-closed on bad inputs.
[[nodiscard]] RoofEmitResult evaluate_roof_emit(
    const RunoffCellInputs& inputs,
    const RunoffCellParams& params,
    RunoffCellState& state,
    core::Real dt);

// Outcome of applying a CouplingLib acceptance to the pending buffer (m^3).
struct RoofAcceptanceResult {
    core::Real accepted_volume{0.0};
    core::Real rejected_volume{0.0};
    core::Real overflow_to_surface_volume{0.0};
    core::Real pending_delta_volume{0.0};  // pending_after - pending_before
    bool node_rejected{false};
    bool pending_saturated{false};
    bool overflowed{false};
    bool missing_overflow_target{false};
};

// Roof chain part B: subtract accepted volume from pending; the rejected portion
// stays in pending; if pending then exceeds roof_storage_capacity * roof_area the
// excess overflows to the mapped surface cell (when overflow_target_valid) or
// flags missing_overflow_target and is retained (never silently dropped).
// Mutates roof_pending_volume.
[[nodiscard]] RoofAcceptanceResult apply_roof_drainage_acceptance(
    const RunoffCellInputs& inputs,
    const RunoffCellParams& params,
    RunoffCellState& state,
    const RoofDrainageAcceptance& acceptance,
    bool overflow_target_valid);

// Bundles the assembled per-substep result with the roof drainage intent to
// hand to CouplingLib. surface_added_volume in the result is ground-only; the
// roof intent is satisfied later via apply_roof_drainage_acceptance and the
// result's roof_to_swmm_accepted / overflow / pending_delta fields updated by
// the caller (M247-C/D).
struct RunoffGenerationOutput {
    RunoffGenerationResult result;
    RoofDrainageIntent intent;
};

// Top-level per-cell runoff generation for one substep (pre-acceptance):
// runs the ground chain and the roof emit chain, assembles the audited result
// (roof abstraction folded into abstraction_volume; roof_pending_delta = roof
// input volume since nothing is drained yet; accepted/overflow = 0), and emits
// the roof drainage intent. cell_index/target_node populate the intent.
[[nodiscard]] RunoffGenerationOutput evaluate_runoff_generation(
    const RunoffCellInputs& inputs,
    const RunoffCellParams& params,
    RunoffCellState& state,
    core::Real dt,
    core::Real f_inf_floor,
    int cell_index,
    int target_swmm_node_index);

}  // namespace scau::surface2d

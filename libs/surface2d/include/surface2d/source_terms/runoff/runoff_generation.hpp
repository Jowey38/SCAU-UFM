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
    core::Real infiltration_volume{0.0};
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

}  // namespace scau::surface2d

#pragma once

#include <vector>

#include "core/types.hpp"
#include "mesh/mesh.hpp"
#include "surface2d/source_terms/runoff/fields.hpp"
#include "surface2d/source_terms/runoff/soil_params.hpp"

namespace scau::surface2d {

// Per-step runoff configuration handed to the runoff-aware advance_one_step_cpu.
// rainfall_rate is per-cell (m/s); fields/lut/f_inf_floor parameterise the
// ground chain. RunoffState (the mutable per-cell state) is passed separately.
struct RunoffStepInputs {
    RunoffFields fields;
    SoilParamsLUT lut;
    std::vector<core::Real> rainfall_rate;
    core::Real f_inf_floor{1.0e-4};

    // Neutral default: fully pervious fields, single valid soil entry, zero rain.
    [[nodiscard]] static RunoffStepInputs for_mesh(const mesh::Mesh& mesh);
};

void validate_runoff_step_inputs_match_mesh(const RunoffStepInputs& inputs, const mesh::Mesh& mesh);

}  // namespace scau::surface2d

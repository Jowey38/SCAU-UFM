#include "surface2d/source_terms/runoff/step_inputs.hpp"

#include <stdexcept>

namespace scau::surface2d {

RunoffStepInputs RunoffStepInputs::for_mesh(const mesh::Mesh& mesh) {
    RunoffStepInputs inputs;
    inputs.fields = RunoffFields::for_mesh(mesh);
    inputs.rainfall_rate.assign(mesh.cells.size(), 0.0);
    inputs.f_inf_floor = 1.0e-4;
    // One neutral soil entry so soil_type 0 resolves; loam-ish defaults.
    inputs.lut.entries.push_back(SoilParams{.k_s = 1.0e-5, .psi_f = 0.1, .theta_s = 0.4, .theta_i = 0.1});
    return inputs;
}

void validate_runoff_step_inputs_match_mesh(const RunoffStepInputs& inputs, const mesh::Mesh& mesh) {
    if (inputs.rainfall_rate.size() != mesh.cells.size()) {
        throw std::invalid_argument("runoff step rainfall_rate size must match mesh cell count");
    }
    validate_runoff_fields_match_mesh(inputs.fields, mesh);
    validate_soil_params_lut(inputs.lut);
    if (!(inputs.f_inf_floor > 0.0)) {
        throw std::invalid_argument("runoff step f_inf_floor must be positive");
    }
}

}  // namespace scau::surface2d

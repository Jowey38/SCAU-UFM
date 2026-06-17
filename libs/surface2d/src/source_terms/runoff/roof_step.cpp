#include "surface2d/source_terms/runoff/roof_step.hpp"

#include <stdexcept>

namespace scau::surface2d {

RoofStepInputs RoofStepInputs::for_mesh(const mesh::Mesh& mesh) {
    RoofStepInputs inputs;
    inputs.map = RoofDrainageMap::for_mesh(mesh);
    // Accept-nothing default: every request is fully rejected (CapacityLimited).
    inputs.accept = [](const RoofDrainageIntent& intent) {
        return RoofDrainageAcceptance{
            .requested_volume = intent.requested_volume,
            .accepted_volume = 0.0,
            .rejected_volume = intent.requested_volume,
            .rejection_reason = RoofDrainageRejectionReason::CapacityLimited,
        };
    };
    return inputs;
}

void validate_roof_step_inputs_match_mesh(const RoofStepInputs& inputs, const mesh::Mesh& mesh) {
    validate_roof_drainage_map_match_mesh(inputs.map, mesh);
    if (!inputs.accept) {
        throw std::invalid_argument("roof step acceptance function must be set");
    }
}

// Task-1 temporary stub; replaced by the real implementation in M247-D Task 2.
void apply_roof_runoff_stage(
    SurfaceState&,
    const StepConfig&,
    const DpmFields&,
    const RunoffStepInputs&,
    const RoofStepInputs&,
    RunoffState&,
    const GeometryCache&,
    StepDiagnostics&) {
}

}  // namespace scau::surface2d

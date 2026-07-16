#pragma once

#include <functional>
#include <vector>

#include "core/types.hpp"
#include "mesh/mesh.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/geometry/cache.hpp"
#include "surface2d/source_terms/runoff/fields.hpp"
#include "surface2d/source_terms/runoff/result.hpp"
#include "surface2d/source_terms/runoff/step_inputs.hpp"
#include "surface2d/state/state.hpp"

namespace scau::surface2d {

struct StepConfig;
struct StepDiagnostics;

// CouplingLib acceptance port: given a roof drainage intent, return the volume
// CouplingLib accepted into the pipe network. Mocked in tests; a real SWMM
// adapter (q = accepted/dt_sub) plugs in here once M240 is on the base.
using RoofDrainageAcceptanceFn = std::function<RoofDrainageAcceptance(const RoofDrainageIntent&)>;

// Per-step roof routing + the acceptance port.
struct RoofStepInputs {
    RoofDrainageMap map;
    RoofDrainageAcceptanceFn accept;

    // Neutral default: no roof targets (-1), an accept-nothing policy.
    [[nodiscard]] static RoofStepInputs for_mesh(const mesh::Mesh& mesh);
};

void validate_roof_step_inputs_match_mesh(const RoofStepInputs& inputs, const mesh::Mesh& mesh);

// Roof runoff stage (M247-D): per cell with roof_fraction>0, run evaluate_roof_emit,
// emit an intent, get acceptance from inputs.accept, apply it, route overflow to the
// mapped surface cell's h, scatter roof state, and audit. (Defined in Task 2 -- declare
// it here now so the header is stable.)
void apply_roof_runoff_stage(
    SurfaceState& state,
    const StepConfig& config,
    const DpmFields& dpm_fields,
    const RunoffStepInputs& runoff,
    const RoofStepInputs& roof,
    RunoffState& runoff_state,
    const GeometryCache& geometry,
    StepDiagnostics& diagnostics);

}  // namespace scau::surface2d

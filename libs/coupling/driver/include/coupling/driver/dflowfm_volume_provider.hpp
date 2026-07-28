#pragma once

#include <cstddef>
#include <string>

#include "coupling/river/dflowfm_boundary.hpp"

namespace scau::coupling::driver {

// Case-owned D-Flow FM whole-domain storage observation. M258 real-runtime
// evidence confirms that, for the authored 1D case, each entry of BMI `vol1`
// is a flow-node control volume [m^3] and sum(vol1) is total engine storage.
// This DTO is an observation only; conservation/residual/gate decisions stay
// outside the provider.
struct DFlowFMVolumeObservation {
    double volume{0.0};
    std::size_t sample_count{0U};
    bool scope_complete{false};
    std::string variable_name{};
};

// Reads and immediately copies the confirmed rank-1 double `vol1` variable
// through IDFlowFMEngine, validates every control volume as finite/non-negative,
// and returns their compensated sum. The variable is intentionally fixed:
// M258 evidence does not authorize another variable to claim complete scope.
//
// Sampling point: call after IDFlowFMEngine::update() returns, matching the
// M258 contract evidence. A successful observation is scope_complete=true
// for D-Flow FM only; it does not imply the other engines' storage scopes are
// complete.
[[nodiscard]] DFlowFMVolumeObservation observe_dflowfm_volume(
    const river::IDFlowFMEngine& engine);

}  // namespace scau::coupling::driver

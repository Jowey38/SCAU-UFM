#include "coupling/driver/dflowfm_volume_provider.hpp"

#include <cmath>

#include "coupling/river/dflowfm_boundary.hpp"

namespace scau::coupling::driver {

DFlowFMVolumeObservation observe_dflowfm_volume(
    const river::IDFlowFMEngine& engine) {
    constexpr const char* kVolumeVariable = "vol1";
    const std::vector<double> volumes = engine.get_rank1_double_values(kVolumeVariable);
    if (volumes.empty()) {
        throw river::DFlowFMEngineError(
            "D-Flow FM volume variable must contain at least one control volume",
            "DFlowFM",
            "dflowfm_volume_variable_empty_shape");
    }

    // Neumaier compensated summation keeps the whole-domain storage stable
    // when control-volume magnitudes vary strongly across a real network.
    double sum = 0.0;
    double correction = 0.0;
    for (const double volume : volumes) {
        if (!std::isfinite(volume) || volume < 0.0) {
            throw river::DFlowFMEngineError(
                "D-Flow FM volume variable contains a non-finite or negative control volume",
                "DFlowFM",
                "dflowfm_volume_value_invalid");
        }
        const double candidate = sum + volume;
        if (std::abs(sum) >= std::abs(volume)) {
            correction += (sum - candidate) + volume;
        } else {
            correction += (volume - candidate) + sum;
        }
        sum = candidate;
    }

    const double total = sum + correction;
    if (!std::isfinite(total)) {
        throw river::DFlowFMEngineError(
            "D-Flow FM total volume is not finite",
            "DFlowFM",
            "dflowfm_total_volume_invalid");
    }

    return DFlowFMVolumeObservation{
        .volume = total,
        .sample_count = volumes.size(),
        .scope_complete = true,
        .variable_name = kVolumeVariable,
    };
}

}  // namespace scau::coupling::driver

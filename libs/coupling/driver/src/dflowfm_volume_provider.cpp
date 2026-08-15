#include "coupling/driver/dflowfm_volume_provider.hpp"

#include <cmath>
#include <utility>
#include <vector>

#include "coupling/river/dflowfm_boundary.hpp"

namespace scau::coupling::driver {

namespace {

DFlowFMVolumeObservation sum_control_volumes(const std::vector<double>& volumes,
                                             std::size_t count,
                                             std::string variable_name) {
    // Neumaier compensated summation keeps the whole-domain storage stable
    // when control-volume magnitudes vary strongly across a real network.
    double sum = 0.0;
    double correction = 0.0;
    for (std::size_t index = 0; index < count; ++index) {
        const double volume = volumes[index];
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
        .sample_count = count,
        .scope_complete = true,
        .variable_name = std::move(variable_name),
    };
}

std::vector<double> read_vol1(const river::IDFlowFMEngine& engine) {
    const std::vector<double> volumes = engine.get_rank1_double_values("vol1");
    if (volumes.empty()) {
        throw river::DFlowFMEngineError(
            "D-Flow FM volume variable must contain at least one control volume",
            "DFlowFM",
            "dflowfm_volume_variable_empty_shape");
    }
    return volumes;
}

}  // namespace

DFlowFMVolumeObservation observe_dflowfm_volume(
    const river::IDFlowFMEngine& engine) {
    const std::vector<double> volumes = read_vol1(engine);
    return sum_control_volumes(volumes, volumes.size(), "vol1");
}

DFlowFMVolumeObservation observe_dflowfm_internal_volume(
    const river::DFlowFMEngine& engine) {
    const std::vector<double> volumes = read_vol1(engine);
    const int internal_count = engine.internal_cell_count();
    if (static_cast<std::size_t>(internal_count) > volumes.size()) {
        throw river::DFlowFMEngineError(
            "D-Flow FM ndxi exceeds the vol1 shape",
            "DFlowFM",
            "dflowfm_ndxi_exceeds_vol1_shape");
    }
    return sum_control_volumes(volumes,
                               static_cast<std::size_t>(internal_count),
                               "vol1[0:ndxi]");
}

}  // namespace scau::coupling::driver

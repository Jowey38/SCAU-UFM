#include "coupling/driver/dflowfm_external_net_provider.hpp"

#include <cmath>

#include "coupling/river/dflowfm_boundary.hpp"

namespace scau::coupling::driver {

DFlowFMExternalNetObservation derive_dflowfm_external_net(
    const river::DFlowFMNativeWaterBalance& native) {
    if (!native.scope_complete) {
        throw river::DFlowFMEngineError(
            "D-Flow FM native water-balance snapshot is not scope-complete",
            "DFlowFM",
            "dflowfm_water_balance_observation_invalid");
    }

    const double guarded_classes[] = {
        native.source_in_m3,        native.source_out_m3,
        native.qext_1d_in_m3,       native.qext_1d_out_m3,
        native.qext_2d_in_m3,       native.qext_2d_out_m3,
        native.rain_in_m3,          native.evaporation_out_m3,
        native.groundwater_in_m3,   native.groundwater_out_m3,
    };
    for (const double value : guarded_classes) {
        if (value != 0.0) {
            throw river::DFlowFMEngineError(
                "D-Flow FM run exercises an unproven external forcing class "
                "(source/qext/rain/evaporation/groundwater must stay zero)",
                "DFlowFM",
                "dflowfm_external_unproven_class_nonzero");
        }
    }

    DFlowFMExternalNetObservation observation{};
    observation.storage_m3 = native.storage_m3;
    observation.volume_error_cumulative_m3 = native.volume_error_cumulative_m3;
    observation.boundary_in_m3 = native.boundary_in_m3;
    observation.boundary_out_m3 = native.boundary_out_m3;
    observation.api_lateral_in_m3 = native.lateral_1d_in_m3 + native.lateral_2d_in_m3;
    observation.api_lateral_out_m3 = native.lateral_1d_out_m3 + native.lateral_2d_out_m3;
    observation.api_lateral_net_volume_m3 =
        observation.api_lateral_in_m3 - observation.api_lateral_out_m3;
    observation.raw_external_net_volume_m3 =
        (native.boundary_in_m3 - native.boundary_out_m3) +
        observation.api_lateral_net_volume_m3;
    // CouplingLib-accepted lateral volume is already represented in
    // Surface2D/CouplingLib accounting; remove it exactly once.
    observation.external_net_volume_m3 =
        observation.raw_external_net_volume_m3 - observation.api_lateral_net_volume_m3;

    observation.scope_complete =
        std::isfinite(observation.raw_external_net_volume_m3) &&
        std::isfinite(observation.external_net_volume_m3);
    if (!observation.scope_complete) {
        throw river::DFlowFMEngineError(
            "D-Flow FM external-net derivation produced a non-finite value",
            "DFlowFM",
            "dflowfm_external_net_invalid");
    }
    return observation;
}

DFlowFMExternalNetObservation observe_dflowfm_external_net(
    const river::DFlowFMEngine& engine) {
    return derive_dflowfm_external_net(engine.observe_native_water_balance());
}

}  // namespace scau::coupling::driver

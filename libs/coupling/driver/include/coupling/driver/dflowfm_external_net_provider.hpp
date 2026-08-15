#pragma once

#include "coupling/river/dflowfm_engine.hpp"

namespace scau::coupling::driver {

// Driver-owned audit view over the engine-native cumulative water balance.
//
// Scope policy (M274 contract + M272 dedup precedent):
// - boundary_in/out are genuinely external river inflow/outflow and form the
//   audit external net.
// - ALL lateral volume is CouplingLib-mediated API exchange (SCAU-UFM writes
//   `laterals/<id>/water_discharge` through the driver mapping; authored
//   cases register laterals with discharge = 0). It is already represented in
//   Surface2D/CouplingLib accounting, so it is removed from the audit value
//   exactly once, mirroring the SWMM api-lateral deduplication. File-forced
//   laterals are outside this contract.
// - source/qext/rain/evaporation/groundwater classes are unproven forcing
//   scope (M273/M274: not drivable through BMI or authored cases); any
//   nonzero value there means the run exercises physics outside the proven
//   contract and the provider fails closed.
//
// All values are cumulative since the last engine initialize and reset on
// every initialize (including restart reload); the whole-system audit
// consumes current-minus-baseline deltas inside one initialize span.
struct DFlowFMExternalNetObservation {
    double storage_m3{0.0};
    double volume_error_cumulative_m3{0.0};
    double boundary_in_m3{0.0};
    double boundary_out_m3{0.0};
    double api_lateral_in_m3{0.0};
    double api_lateral_out_m3{0.0};
    double raw_external_net_volume_m3{0.0};
    double api_lateral_net_volume_m3{0.0};
    double external_net_volume_m3{0.0};
    bool scope_complete{false};
};

// Pure derivation from the raw native snapshot. Throws DFlowFMEngineError
// when the snapshot is invalid (scope_complete false) or an unproven forcing
// class is nonzero; otherwise returns scope_complete = true.
[[nodiscard]] DFlowFMExternalNetObservation derive_dflowfm_external_net(
    const river::DFlowFMNativeWaterBalance& native);

// Convenience provider over the concrete engine (native bridge ABI is not
// part of IDFlowFMEngine by design).
[[nodiscard]] DFlowFMExternalNetObservation observe_dflowfm_external_net(
    const river::DFlowFMEngine& engine);

}  // namespace scau::coupling::driver

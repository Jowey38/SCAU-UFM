#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "coupling/river/dflowfm_boundary.hpp"

namespace scau::coupling::driver {

// C3 D-Flow FM provider contract (machine-facing identity the driver, the
// CouplingLib ledger records, the result linker and the QGIS view all share).
//
// Scope: the ONLY exchange kind the runtime drives through the engine is the
// CouplingLib-mediated API lateral (`laterals/<id>/water_discharge`). Open
// river boundaries are observed only in aggregate (boundary_in/out of the
// native water balance); they carry no per-object identity here and must not
// be presented as if they did.
inline constexpr const char* kDFlowFMProviderId = "dflowfm";
inline constexpr const char* kDFlowFMCapabilityExternalNetV1 = "dflowfm.external_net.v1";
inline constexpr const char* kDFlowFMExchangeKindApiLateral = "api_lateral";

struct DFlowFMBoundaryIdentity {
    std::string boundary_id{};          // case-owned native lateral id
    int provider_object_id{0};          // engine-side location id
    std::size_t surface_cell{0U};
    std::string exchange_kind{kDFlowFMExchangeKindApiLateral};
};

struct DFlowFMProviderContractSnapshot {
    std::string provider_id{};
    std::string capability_schema_version{};
    // Case identity: bytes of the surface case and of the river model input.
    std::string source_stcf_hash{};
    std::string dflowfm_mdu_hash{};
    // True when the run binds the concrete native water-balance observation;
    // then the MDU bytes must be hashed (a mock run cannot claim native scope).
    bool native_observed{false};
    double start_time_seconds{0.0};
    double dt_couple_seconds{0.0};
    std::vector<DFlowFMBoundaryIdentity> boundaries{};
};

// One committed-epoch native observation (cumulative since engine initialize).
struct DFlowFMNativeEpochObservation {
    double logical_time{0.0};
    double storage_m3{0.0};
    double boundary_in_m3{0.0};
    double boundary_out_m3{0.0};
    double api_lateral_in_m3{0.0};
    double api_lateral_out_m3{0.0};
    double volume_error_cumulative_m3{0.0};
};

// Both validators throw DFlowFMEngineError with a stable reason code
// (dflowfm_contract_*) and never repair the input.
void validate_dflowfm_provider_contract(const DFlowFMProviderContractSnapshot& snapshot);

// Epoch i must sit at start + (i + 1) * dt_couple; cumulative gross classes are
// non-decreasing; every value is finite. `baseline` is the initialize-time
// observation the series is measured against.
void validate_dflowfm_native_series(
    const DFlowFMProviderContractSnapshot& snapshot,
    const DFlowFMNativeEpochObservation& baseline,
    const std::vector<DFlowFMNativeEpochObservation>& series);

}  // namespace scau::coupling::driver

#include "coupling/driver/dflowfm_provider_contract.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <string>

namespace scau::coupling::driver {
namespace {

[[noreturn]] void reject(const std::string& message, const char* reason) {
    throw river::DFlowFMEngineError(message, "DFlowFM", reason);
}

bool finite(double value) { return std::isfinite(value); }

}  // namespace

void validate_dflowfm_provider_contract(const DFlowFMProviderContractSnapshot& snapshot) {
    if (snapshot.provider_id != kDFlowFMProviderId) {
        reject("D-Flow FM provider id '" + snapshot.provider_id + "' is not the C3 provider",
               "dflowfm_contract_provider_mismatch");
    }
    if (snapshot.capability_schema_version != kDFlowFMCapabilityExternalNetV1) {
        reject("D-Flow FM capability '" + snapshot.capability_schema_version +
                   "' is not the frozen external-net v1 contract",
               "dflowfm_contract_capability_unsupported");
    }
    if (snapshot.source_stcf_hash.empty()) {
        reject("D-Flow FM contract lacks the source STCF byte hash",
               "dflowfm_contract_case_identity_missing");
    }
    if (snapshot.native_observed && snapshot.dflowfm_mdu_hash.empty()) {
        reject("D-Flow FM native observation is bound but the MDU bytes are not hashed",
               "dflowfm_contract_case_identity_missing");
    }
    if (!finite(snapshot.start_time_seconds) || !finite(snapshot.dt_couple_seconds) ||
        snapshot.dt_couple_seconds <= 0.0) {
        reject("D-Flow FM contract time base is not finite with positive dt_couple",
               "dflowfm_contract_time_base_invalid");
    }
    std::set<std::string> boundary_ids;
    std::set<int> object_ids;
    for (const auto& boundary : snapshot.boundaries) {
        if (boundary.boundary_id.empty()) {
            reject("D-Flow FM boundary lacks a case-owned boundary id",
                   "dflowfm_contract_boundary_identity_missing");
        }
        if (boundary.provider_object_id < 0) {
            reject("D-Flow FM boundary '" + boundary.boundary_id +
                       "' has a negative provider object id",
                   "dflowfm_contract_boundary_identity_invalid");
        }
        if (boundary.exchange_kind != kDFlowFMExchangeKindApiLateral) {
            reject("D-Flow FM boundary '" + boundary.boundary_id + "' exchange kind '" +
                       boundary.exchange_kind + "' is outside the proven contract",
                   "dflowfm_contract_exchange_kind_unsupported");
        }
        if (!boundary_ids.insert(boundary.boundary_id).second) {
            reject("D-Flow FM boundary id '" + boundary.boundary_id + "' is duplicated",
                   "dflowfm_contract_boundary_id_duplicate");
        }
        if (!object_ids.insert(boundary.provider_object_id).second) {
            reject("D-Flow FM provider object id " + std::to_string(boundary.provider_object_id) +
                       " is bound by more than one boundary",
                   "dflowfm_contract_provider_object_duplicate");
        }
    }
}

void validate_dflowfm_native_series(
    const DFlowFMProviderContractSnapshot& snapshot,
    const DFlowFMNativeEpochObservation& baseline,
    const std::vector<DFlowFMNativeEpochObservation>& series) {
    const auto check_finite = [](const DFlowFMNativeEpochObservation& o, const char* where) {
        if (!finite(o.logical_time) || !finite(o.storage_m3) || !finite(o.boundary_in_m3) ||
            !finite(o.boundary_out_m3) || !finite(o.api_lateral_in_m3) ||
            !finite(o.api_lateral_out_m3) || !finite(o.volume_error_cumulative_m3)) {
            reject(std::string("D-Flow FM native observation is non-finite at ") + where,
                   "dflowfm_contract_native_observation_invalid");
        }
        if (o.storage_m3 < 0.0 || o.boundary_in_m3 < 0.0 || o.boundary_out_m3 < 0.0 ||
            o.api_lateral_in_m3 < 0.0 || o.api_lateral_out_m3 < 0.0) {
            reject(std::string("D-Flow FM native gross class is negative at ") + where,
                   "dflowfm_contract_native_observation_invalid");
        }
    };
    check_finite(baseline, "baseline");
    if (baseline.logical_time != snapshot.start_time_seconds) {
        reject("D-Flow FM native baseline is not at the contract start time",
               "dflowfm_contract_native_time_invalid");
    }
    const DFlowFMNativeEpochObservation* previous = &baseline;
    for (std::size_t i = 0U; i < series.size(); ++i) {
        const auto& current = series[i];
        check_finite(current, "epoch");
        const double expected =
            snapshot.start_time_seconds + static_cast<double>(i + 1U) * snapshot.dt_couple_seconds;
        if (std::abs(current.logical_time - expected) > 1.0e-9 * std::max(1.0, std::abs(expected)) ||
            current.logical_time <= previous->logical_time) {
            reject("D-Flow FM native epoch " + std::to_string(i) + " time " +
                       std::to_string(current.logical_time) + " != start + (i+1)*dt_couple",
                   "dflowfm_contract_native_time_invalid");
        }
        if (current.boundary_in_m3 < previous->boundary_in_m3 ||
            current.boundary_out_m3 < previous->boundary_out_m3 ||
            current.api_lateral_in_m3 < previous->api_lateral_in_m3 ||
            current.api_lateral_out_m3 < previous->api_lateral_out_m3) {
            reject("D-Flow FM cumulative gross class decreased at epoch " + std::to_string(i),
                   "dflowfm_contract_native_monotonicity_violated");
        }
        previous = &current;
    }
}

}  // namespace scau::coupling::driver

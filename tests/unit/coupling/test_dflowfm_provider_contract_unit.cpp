#include <gtest/gtest.h>

#include <functional>
#include <limits>
#include <string>
#include <vector>

#include "coupling/driver/dflowfm_provider_contract.hpp"

namespace driver = scau::coupling::driver;
namespace river = scau::coupling::river;

namespace {

driver::DFlowFMProviderContractSnapshot valid_snapshot() {
    driver::DFlowFMProviderContractSnapshot s{};
    s.provider_id = driver::kDFlowFMProviderId;
    s.capability_schema_version = driver::kDFlowFMCapabilityExternalNetV1;
    s.source_stcf_hash = "fnv1a64:0123456789abcdef";
    s.dflowfm_mdu_hash = "fnv1a64:fedcba9876543210";
    s.native_observed = true;
    s.start_time_seconds = 0.0;
    s.dt_couple_seconds = 60.0;
    s.boundaries = {{"lat1", 0, 1U, driver::kDFlowFMExchangeKindApiLateral},
                    {"lat2", 1, 0U, driver::kDFlowFMExchangeKindApiLateral}};
    return s;
}

driver::DFlowFMNativeEpochObservation obs(double t, double lat_in, double b_in, double b_out) {
    driver::DFlowFMNativeEpochObservation o{};
    o.logical_time = t;
    o.storage_m3 = 5500.0;
    o.api_lateral_in_m3 = lat_in;
    o.boundary_in_m3 = b_in;
    o.boundary_out_m3 = b_out;
    return o;
}

std::string reason_of(const std::function<void()>& fn) {
    try {
        fn();
    } catch (const river::DFlowFMEngineError& error) {
        return error.error_code();
    }
    return "<no throw>";
}

TEST(DFlowFMProviderContract, AcceptsFrozenIdentityAndDistinctBoundaries) {
    EXPECT_NO_THROW(driver::validate_dflowfm_provider_contract(valid_snapshot()));
}

TEST(DFlowFMProviderContract, MockRunMayOmitMduHashOnlyWithoutNativeScope) {
    auto s = valid_snapshot();
    s.native_observed = false;
    s.dflowfm_mdu_hash.clear();
    EXPECT_NO_THROW(driver::validate_dflowfm_provider_contract(s));
    s.native_observed = true;
    EXPECT_EQ(reason_of([&] { driver::validate_dflowfm_provider_contract(s); }),
              "dflowfm_contract_case_identity_missing");
}

TEST(DFlowFMProviderContract, RejectsForeignProviderOrCapability) {
    auto s = valid_snapshot();
    s.provider_id = "swmm";
    EXPECT_EQ(reason_of([&] { driver::validate_dflowfm_provider_contract(s); }),
              "dflowfm_contract_provider_mismatch");
    s = valid_snapshot();
    s.capability_schema_version = "dflowfm.external_net.v2";
    EXPECT_EQ(reason_of([&] { driver::validate_dflowfm_provider_contract(s); }),
              "dflowfm_contract_capability_unsupported");
}

// C3-01: same name != same object, and one object cannot back two boundaries.
TEST(DFlowFMProviderContract, RejectsDuplicateBoundaryOrObjectIdentity) {
    auto s = valid_snapshot();
    s.boundaries[1].boundary_id = "lat1";
    EXPECT_EQ(reason_of([&] { driver::validate_dflowfm_provider_contract(s); }),
              "dflowfm_contract_boundary_id_duplicate");
    s = valid_snapshot();
    s.boundaries[1].provider_object_id = 0;
    EXPECT_EQ(reason_of([&] { driver::validate_dflowfm_provider_contract(s); }),
              "dflowfm_contract_provider_object_duplicate");
    s = valid_snapshot();
    s.boundaries[0].boundary_id.clear();
    EXPECT_EQ(reason_of([&] { driver::validate_dflowfm_provider_contract(s); }),
              "dflowfm_contract_boundary_identity_missing");
}

// C3-02/03: only the proven exchange kind; anything else is outside scope.
TEST(DFlowFMProviderContract, RejectsUnprovenExchangeKindAndBadTimeBase) {
    auto s = valid_snapshot();
    s.boundaries[0].exchange_kind = "open_boundary";
    EXPECT_EQ(reason_of([&] { driver::validate_dflowfm_provider_contract(s); }),
              "dflowfm_contract_exchange_kind_unsupported");
    s = valid_snapshot();
    s.dt_couple_seconds = 0.0;
    EXPECT_EQ(reason_of([&] { driver::validate_dflowfm_provider_contract(s); }),
              "dflowfm_contract_time_base_invalid");
}

TEST(DFlowFMProviderContract, AcceptsMonotoneNativeSeriesOnContractTimeBase) {
    const auto s = valid_snapshot();
    const auto baseline = obs(0.0, 0.0, 0.0, 0.0);
    const std::vector<driver::DFlowFMNativeEpochObservation> series{
        obs(60.0, 1.0, 10.0, 12.0), obs(120.0, 2.5, 20.0, 24.0), obs(180.0, 2.5, 30.0, 36.0)};
    EXPECT_NO_THROW(driver::validate_dflowfm_native_series(s, baseline, series));
}

// C3-04: 60, 120, 240 skips epoch 3; 60, 120, 120 duplicates; time must not run back.
TEST(DFlowFMProviderContract, RejectsGapDuplicateOrReversedEpochs) {
    const auto s = valid_snapshot();
    const auto baseline = obs(0.0, 0.0, 0.0, 0.0);
    for (const auto& bad : {std::vector<driver::DFlowFMNativeEpochObservation>{obs(60.0, 1, 1, 1), obs(120.0, 1, 1, 1), obs(240.0, 1, 1, 1)},
                            std::vector<driver::DFlowFMNativeEpochObservation>{obs(60.0, 1, 1, 1), obs(120.0, 1, 1, 1), obs(120.0, 1, 1, 1)},
                            std::vector<driver::DFlowFMNativeEpochObservation>{obs(120.0, 1, 1, 1)}}) {
        EXPECT_EQ(reason_of([&] { driver::validate_dflowfm_native_series(s, baseline, bad); }),
                  "dflowfm_contract_native_time_invalid");
    }
    auto shifted = s;
    shifted.start_time_seconds = 30.0;
    EXPECT_EQ(reason_of([&] { driver::validate_dflowfm_native_series(shifted, baseline, {}); }),
              "dflowfm_contract_native_time_invalid");
}

TEST(DFlowFMProviderContract, RejectsDecreasingCumulativeOrNonFiniteNative) {
    const auto s = valid_snapshot();
    const auto baseline = obs(0.0, 0.0, 0.0, 0.0);
    EXPECT_EQ(reason_of([&] {
                  driver::validate_dflowfm_native_series(
                      s, baseline, {obs(60.0, 2.0, 1.0, 1.0), obs(120.0, 1.5, 2.0, 2.0)});
              }),
              "dflowfm_contract_native_monotonicity_violated");
    EXPECT_EQ(reason_of([&] {
                  driver::validate_dflowfm_native_series(
                      s, baseline, {obs(60.0, std::numeric_limits<double>::quiet_NaN(), 1.0, 1.0)});
              }),
              "dflowfm_contract_native_observation_invalid");
    EXPECT_EQ(reason_of([&] {
                  driver::validate_dflowfm_native_series(s, baseline, {obs(60.0, -1.0, 1.0, 1.0)});
              }),
              "dflowfm_contract_native_observation_invalid");
}

}  // namespace

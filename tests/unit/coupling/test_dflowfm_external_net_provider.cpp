#include <gtest/gtest.h>

#include <limits>

#include "coupling/driver/dflowfm_external_net_provider.hpp"
#include "coupling/river/dflowfm_boundary.hpp"

namespace driver = scau::coupling::driver;
namespace river = scau::coupling::river;

namespace {

river::DFlowFMNativeWaterBalance make_valid_native() {
    river::DFlowFMNativeWaterBalance native{};
    native.current_time_seconds = 600.0;
    native.storage_m3 = 5535.80621847882;
    native.volume_error_cumulative_m3 = -5.45696821063757e-12;
    native.boundary_in_m3 = 75.0;
    native.boundary_out_m3 = 114.193781521175;
    native.lateral_1d_in_m3 = 75.0;
    native.lateral_1d_out_m3 = 0.0;
    native.scope_complete = true;
    return native;
}

TEST(DFlowFMExternalNetProvider, DerivesBoundaryOnlyAuditNetWithLateralDedup) {
    const auto observation = driver::derive_dflowfm_external_net(make_valid_native());

    EXPECT_TRUE(observation.scope_complete);
    EXPECT_DOUBLE_EQ(observation.boundary_in_m3, 75.0);
    EXPECT_DOUBLE_EQ(observation.boundary_out_m3, 114.193781521175);
    EXPECT_DOUBLE_EQ(observation.api_lateral_in_m3, 75.0);
    EXPECT_DOUBLE_EQ(observation.api_lateral_out_m3, 0.0);
    EXPECT_DOUBLE_EQ(observation.api_lateral_net_volume_m3, 75.0);
    // raw = boundary net + api lateral net; audit value removes the
    // CouplingLib-owned lateral exactly once.
    EXPECT_DOUBLE_EQ(observation.raw_external_net_volume_m3,
                     (75.0 - 114.193781521175) + 75.0);
    EXPECT_DOUBLE_EQ(observation.external_net_volume_m3, 75.0 - 114.193781521175);
}

TEST(DFlowFMExternalNetProvider, SumsBothLateralDimensionClasses) {
    auto native = make_valid_native();
    native.lateral_2d_in_m3 = 2.5;
    native.lateral_2d_out_m3 = 1.0;

    const auto observation = driver::derive_dflowfm_external_net(native);
    EXPECT_DOUBLE_EQ(observation.api_lateral_in_m3, 77.5);
    EXPECT_DOUBLE_EQ(observation.api_lateral_out_m3, 1.0);
    EXPECT_DOUBLE_EQ(observation.api_lateral_net_volume_m3, 76.5);
    EXPECT_DOUBLE_EQ(observation.external_net_volume_m3, 75.0 - 114.193781521175);
}

TEST(DFlowFMExternalNetProvider, RejectsIncompleteNativeSnapshot) {
    auto native = make_valid_native();
    native.scope_complete = false;
    EXPECT_THROW(static_cast<void>(driver::derive_dflowfm_external_net(native)),
                 river::DFlowFMEngineError);
}

TEST(DFlowFMExternalNetProvider, RejectsUnprovenForcingClasses) {
    const auto expect_rejects = [](auto mutate) {
        auto native = make_valid_native();
        mutate(native);
        EXPECT_THROW(static_cast<void>(driver::derive_dflowfm_external_net(native)),
                     river::DFlowFMEngineError);
    };
    expect_rejects([](auto& n) { n.source_in_m3 = 1.0; });
    expect_rejects([](auto& n) { n.source_out_m3 = 1.0; });
    expect_rejects([](auto& n) { n.qext_1d_in_m3 = 1.0; });
    expect_rejects([](auto& n) { n.qext_1d_out_m3 = 1.0; });
    expect_rejects([](auto& n) { n.qext_2d_in_m3 = 1.0; });
    expect_rejects([](auto& n) { n.qext_2d_out_m3 = 1.0; });
    expect_rejects([](auto& n) { n.rain_in_m3 = 1.0; });
    expect_rejects([](auto& n) { n.evaporation_out_m3 = 1.0; });
    expect_rejects([](auto& n) { n.groundwater_in_m3 = 1.0; });
    expect_rejects([](auto& n) { n.groundwater_out_m3 = 1.0; });
}

TEST(DFlowFMExternalNetProvider, RejectsNonFiniteDerivedNet) {
    auto native = make_valid_native();
    native.boundary_in_m3 = std::numeric_limits<double>::max();
    native.boundary_out_m3 = -std::numeric_limits<double>::max();
    // The native snapshot itself claims completeness; the derived raw net
    // overflows to +inf and the derivation must fail closed.
    EXPECT_THROW(static_cast<void>(driver::derive_dflowfm_external_net(native)),
                 river::DFlowFMEngineError);
}

TEST(DFlowFMExternalNetProvider, ZeroForcingDerivesZeroNet) {
    river::DFlowFMNativeWaterBalance native{};
    native.storage_m3 = 5500.0;
    native.scope_complete = true;

    const auto observation = driver::derive_dflowfm_external_net(native);
    EXPECT_TRUE(observation.scope_complete);
    EXPECT_DOUBLE_EQ(observation.external_net_volume_m3, 0.0);
    EXPECT_DOUBLE_EQ(observation.raw_external_net_volume_m3, 0.0);
    EXPECT_DOUBLE_EQ(observation.storage_m3, 5500.0);
}

}  // namespace

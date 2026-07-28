#include <limits>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "coupling/driver/dflowfm_volume_provider.hpp"
#include "coupling/river/dflowfm_boundary.hpp"

namespace {

using scau::coupling::driver::observe_dflowfm_volume;
using scau::coupling::river::DFlowFMEngineError;
using scau::coupling::river::MockDFlowFMEngine;

TEST(CouplingDFlowFMVolumeProvider, SumsVol1AndMarksDFlowScopeComplete) {
    MockDFlowFMEngine engine;
    engine.initialize("case.mdu");
    engine.set_rank1_double_values_fixture("vol1", {10.0, 20.0, 30.0});

    const auto observation = observe_dflowfm_volume(engine);

    EXPECT_DOUBLE_EQ(observation.volume, 60.0);
    EXPECT_EQ(observation.sample_count, 3U);
    EXPECT_TRUE(observation.scope_complete);
    EXPECT_EQ(observation.variable_name, "vol1");
}

TEST(CouplingDFlowFMVolumeProvider, UsesCompensatedSummation) {
    MockDFlowFMEngine engine;
    engine.initialize("case.mdu");
    engine.set_rank1_double_values_fixture("vol1", {1.0e16, 1.0, 1.0});

    const auto observation = observe_dflowfm_volume(engine);

    EXPECT_DOUBLE_EQ(observation.volume, 1.0e16 + 2.0);
}

TEST(CouplingDFlowFMVolumeProvider, RejectsMissingConfirmedVol1Contract) {
    MockDFlowFMEngine engine;
    engine.initialize("case.mdu");

    EXPECT_THROW(
        static_cast<void>(observe_dflowfm_volume(engine)),
        DFlowFMEngineError);
}

TEST(CouplingDFlowFMVolumeProvider, RejectsNegativeOrNonFiniteControlVolume) {
    MockDFlowFMEngine engine;
    engine.initialize("case.mdu");
    engine.set_rank1_double_values_fixture("vol1", {1.0, -0.01});
    EXPECT_THROW(
        static_cast<void>(observe_dflowfm_volume(engine)),
        DFlowFMEngineError);

    engine.set_rank1_double_values_fixture(
        "vol1", {1.0, std::numeric_limits<double>::quiet_NaN()});
    EXPECT_THROW(
        static_cast<void>(observe_dflowfm_volume(engine)),
        DFlowFMEngineError);

    engine.set_rank1_double_values_fixture(
        "vol1", {1.0, std::numeric_limits<double>::infinity()});
    EXPECT_THROW(
        static_cast<void>(observe_dflowfm_volume(engine)),
        DFlowFMEngineError);
}

TEST(CouplingDFlowFMVolumeProvider, RejectsUninitializedEngine) {
    const MockDFlowFMEngine engine;

    EXPECT_THROW(
        static_cast<void>(observe_dflowfm_volume(engine)),
        DFlowFMEngineError);
}

}  // namespace

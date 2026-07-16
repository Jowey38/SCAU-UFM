#include "coupling/drainage/swmm_boundary.hpp"

#include <gtest/gtest.h>

using scau::coupling::drainage::MockSwmmEngine;
using scau::coupling::drainage::SwmmEngineError;

namespace {

MockSwmmEngine make_initialized_engine() {
    MockSwmmEngine engine;
    engine.initialize("mock.inp");
    return engine;
}

}  // namespace

TEST(MockSwmmEngine, SetsAndReadsNodeLateralInflowWithOverrideSemantics) {
    auto engine = make_initialized_engine();

    engine.set_node_lateral_inflow(1, 0.25);
    EXPECT_NEAR(engine.get_node_lateral_inflow(1), 0.25, 1.0e-12);

    engine.set_node_lateral_inflow(1, 0.75);
    EXPECT_NEAR(engine.get_node_lateral_inflow(1), 0.75, 1.0e-12);
}

TEST(MockSwmmEngine, NegativeNodeIdThrowsAndUninitializedThrows) {
    auto engine = make_initialized_engine();

    EXPECT_THROW(static_cast<void>(engine.get_node_head(-1)), SwmmEngineError);
    EXPECT_THROW(static_cast<void>(engine.get_node_lateral_inflow(-1)), SwmmEngineError);
    EXPECT_THROW(engine.set_node_lateral_inflow(-1, 0.1), SwmmEngineError);

    MockSwmmEngine uninitialized;
    EXPECT_THROW(static_cast<void>(uninitialized.get_node_head(0)), SwmmEngineError);
    EXPECT_THROW(uninitialized.set_node_lateral_inflow(0, 0.1), SwmmEngineError);
}

TEST(MockSwmmEngine, SurchargeAndHeadCanBeConfigured) {
    auto engine = make_initialized_engine();

    engine.set_node_head_fixture(0, 1.25);
    engine.set_node_surcharge_fixture(0, true);

    EXPECT_NEAR(engine.get_node_head(0), 1.25, 1.0e-12);
    EXPECT_TRUE(engine.is_surcharged(0));
}

TEST(MockSwmmEngine, StepValidatesDtAndAdvancesElapsedTime) {
    auto engine = make_initialized_engine();

    engine.step(0.5);
    engine.step(0.25);

    EXPECT_NEAR(engine.elapsed_time(), 0.75, 1.0e-12);
    EXPECT_THROW(engine.step(0.0), SwmmEngineError);
    EXPECT_THROW(engine.step(-1.0), SwmmEngineError);
}

TEST(MockSwmmEngine, LinkFlowUsesFixturesAndRejectsNegativeLinkIds) {
    auto engine = make_initialized_engine();

    engine.set_link_flow_fixture(0, -0.125);

    EXPECT_NEAR(engine.get_link_flow(0), -0.125, 1.0e-12);
    EXPECT_THROW(static_cast<void>(engine.get_link_flow(-1)), SwmmEngineError);
}

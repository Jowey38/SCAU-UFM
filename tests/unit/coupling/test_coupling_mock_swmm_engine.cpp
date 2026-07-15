#include "coupling/drainage/mock_swmm_engine.hpp"

#include <stdexcept>

#include <gtest/gtest.h>

using scau::coupling::drainage::MockSwmmEngine;
using scau::coupling::drainage::SwmmEngineError;

TEST(MockSwmmEngine, SetsAndReadsNodeLateralInflowWithOverrideSemantics) {
    MockSwmmEngine engine(2U);

    engine.set_node_lateral_inflow(1U, 0.25);
    EXPECT_NEAR(engine.get_node_lateral_inflow(1U), 0.25, 1.0e-12);

    engine.set_node_lateral_inflow(1U, 0.75);
    EXPECT_NEAR(engine.get_node_lateral_inflow(1U), 0.75, 1.0e-12);
}

TEST(MockSwmmEngine, InvalidNodeThrows) {
    MockSwmmEngine engine(1U);

    EXPECT_THROW(static_cast<void>(engine.get_node_head(1U)), SwmmEngineError);
    EXPECT_THROW(static_cast<void>(engine.get_node_lateral_inflow(1U)), SwmmEngineError);
    EXPECT_THROW(engine.set_node_lateral_inflow(1U, 0.1), SwmmEngineError);
}

TEST(MockSwmmEngine, SurchargeAndHeadCanBeConfigured) {
    MockSwmmEngine engine(1U);

    engine.set_node_head(0U, 1.25);
    engine.set_surcharged(0U, true);

    EXPECT_NEAR(engine.get_node_head(0U), 1.25, 1.0e-12);
    EXPECT_TRUE(engine.is_surcharged(0U));
}

TEST(MockSwmmEngine, StepValidatesDtAndAdvancesElapsedTime) {
    MockSwmmEngine engine(1U);

    engine.step(0.5);
    engine.step(0.25);

    EXPECT_NEAR(engine.elapsed_time(), 0.75, 1.0e-12);
    EXPECT_THROW(engine.step(0.0), SwmmEngineError);
    EXPECT_THROW(engine.step(-1.0), SwmmEngineError);
}

TEST(MockSwmmEngine, LinkFlowUsesConfiguredLinksAndRejectsInvalidLinks) {
    MockSwmmEngine engine(1U, 1U);

    engine.set_link_flow(0U, -0.125);

    EXPECT_NEAR(engine.get_link_flow(0U), -0.125, 1.0e-12);
    EXPECT_THROW(static_cast<void>(engine.get_link_flow(1U)), SwmmEngineError);
}

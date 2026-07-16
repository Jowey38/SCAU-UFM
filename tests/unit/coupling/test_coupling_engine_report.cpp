#include <gtest/gtest.h>

#include "coupling/drainage/swmm_boundary.hpp"
#include "coupling/river/dflowfm_boundary.hpp"

TEST(CouplingEngineReport, SwmmMockReportReflectsInitializedState) {
    scau::coupling::drainage::MockSwmmEngine engine;

    const auto before_init = scau::coupling::drainage::make_swmm_engine_report(engine);
    EXPECT_FALSE(before_init.healthy);
    EXPECT_EQ(before_init.engine_id, "SWMM");
    EXPECT_EQ(before_init.error_code, "swmm_mock_not_initialized");
    EXPECT_DOUBLE_EQ(before_init.elapsed_time, 0.0);

    engine.initialize("mock.inp");
    engine.step(2.0);
    const auto after_step = scau::coupling::drainage::make_swmm_engine_report(engine);
    EXPECT_TRUE(after_step.healthy);
    EXPECT_EQ(after_step.engine_id, "SWMM");
    EXPECT_TRUE(after_step.error_code.empty());
    EXPECT_DOUBLE_EQ(after_step.elapsed_time, 2.0);
}

TEST(CouplingEngineReport, DFlowFMMockReportReflectsInitializedState) {
    scau::coupling::river::MockDFlowFMEngine engine;

    const auto before_init = scau::coupling::river::make_dflowfm_engine_report(engine);
    EXPECT_FALSE(before_init.healthy);
    EXPECT_EQ(before_init.engine_id, "DFlowFM");
    EXPECT_EQ(before_init.error_code, "dflowfm_mock_not_initialized");
    EXPECT_DOUBLE_EQ(before_init.elapsed_time, 0.0);

    engine.initialize("mock.mdu");
    engine.update(3.0);
    const auto after_update = scau::coupling::river::make_dflowfm_engine_report(engine);
    EXPECT_TRUE(after_update.healthy);
    EXPECT_EQ(after_update.engine_id, "DFlowFM");
    EXPECT_TRUE(after_update.error_code.empty());
    EXPECT_DOUBLE_EQ(after_update.elapsed_time, 3.0);
}

TEST(CouplingEngineReport, SwmmErrorConvertsToUnhealthyReport) {
    const scau::coupling::drainage::SwmmEngineError error{
        "mock failure",
        "SWMM",
        "swmm_mock_failure",
    };

    const auto report = scau::coupling::drainage::make_swmm_engine_report(error);

    EXPECT_FALSE(report.healthy);
    EXPECT_EQ(report.engine_id, "SWMM");
    EXPECT_EQ(report.error_code, "swmm_mock_failure");
    EXPECT_DOUBLE_EQ(report.elapsed_time, 0.0);
}

TEST(CouplingEngineReport, DFlowFMErrorConvertsToUnhealthyReport) {
    const scau::coupling::river::DFlowFMEngineError error{
        "mock failure",
        "DFlowFM",
        "dflowfm_mock_failure",
    };

    const auto report = scau::coupling::river::make_dflowfm_engine_report(error);

    EXPECT_FALSE(report.healthy);
    EXPECT_EQ(report.engine_id, "DFlowFM");
    EXPECT_EQ(report.error_code, "dflowfm_mock_failure");
    EXPECT_DOUBLE_EQ(report.elapsed_time, 0.0);
}

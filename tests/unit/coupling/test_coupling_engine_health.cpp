#include <limits>
#include <stdexcept>

#include <gtest/gtest.h>

#include "coupling/core/state.hpp"

TEST(CouplingEngineHealth, ClassifiesHealthyReport) {
    const scau::coupling::core::EngineReport report{
        .healthy = true,
        .engine_id = "SWMM",
        .error_code = "",
        .elapsed_time = 2.0,
    };

    const auto diagnostic = scau::coupling::core::classify_engine_health(report);

    EXPECT_EQ(diagnostic.status, scau::coupling::core::EngineHealthStatus::healthy);
    EXPECT_EQ(diagnostic.engine_id, "SWMM");
    EXPECT_TRUE(diagnostic.error_code.empty());
    EXPECT_DOUBLE_EQ(diagnostic.elapsed_time, 2.0);
}

TEST(CouplingEngineHealth, ClassifiesUnhealthyReport) {
    const scau::coupling::core::EngineReport report{
        .healthy = false,
        .engine_id = "DFlowFM",
        .error_code = "dflowfm_mock_failure",
        .elapsed_time = 3.0,
    };

    const auto diagnostic = scau::coupling::core::classify_engine_health(report);

    EXPECT_EQ(diagnostic.status, scau::coupling::core::EngineHealthStatus::unhealthy);
    EXPECT_EQ(diagnostic.engine_id, "DFlowFM");
    EXPECT_EQ(diagnostic.error_code, "dflowfm_mock_failure");
    EXPECT_DOUBLE_EQ(diagnostic.elapsed_time, 3.0);
}

TEST(CouplingEngineHealth, RejectsInvalidElapsedTime) {
    const scau::coupling::core::EngineReport nonfinite{
        .healthy = true,
        .engine_id = "SWMM",
        .elapsed_time = std::numeric_limits<double>::quiet_NaN(),
    };
    const scau::coupling::core::EngineReport negative{
        .healthy = true,
        .engine_id = "SWMM",
        .elapsed_time = -1.0,
    };

    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::classify_engine_health(nonfinite)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::classify_engine_health(negative)),
        std::invalid_argument);
}

TEST(CouplingEngineHealth, AggregatesAllHealthyDiagnostics) {
    const std::vector<scau::coupling::core::EngineHealthDiagnostic> diagnostics{
        {.status = scau::coupling::core::EngineHealthStatus::healthy, .engine_id = "SWMM"},
        {.status = scau::coupling::core::EngineHealthStatus::healthy, .engine_id = "DFlowFM"},
    };

    const auto aggregate = scau::coupling::core::aggregate_engine_health(diagnostics);

    EXPECT_EQ(aggregate.status, scau::coupling::core::EngineHealthAggregateStatus::all_healthy);
    EXPECT_EQ(aggregate.report_count, 2U);
    EXPECT_EQ(aggregate.unhealthy_count, 0U);
}

TEST(CouplingEngineHealth, AggregatesAnyUnhealthyDiagnostics) {
    const std::vector<scau::coupling::core::EngineHealthDiagnostic> diagnostics{
        {.status = scau::coupling::core::EngineHealthStatus::healthy, .engine_id = "SWMM"},
        {.status = scau::coupling::core::EngineHealthStatus::unhealthy, .engine_id = "DFlowFM"},
        {.status = scau::coupling::core::EngineHealthStatus::unhealthy, .engine_id = "SWMM"},
    };

    const auto aggregate = scau::coupling::core::aggregate_engine_health(diagnostics);

    EXPECT_EQ(aggregate.status, scau::coupling::core::EngineHealthAggregateStatus::any_unhealthy);
    EXPECT_EQ(aggregate.report_count, 3U);
    EXPECT_EQ(aggregate.unhealthy_count, 2U);
}

TEST(CouplingEngineHealth, EmptyAggregateIsAllHealthyWithZeroReports) {
    const auto aggregate = scau::coupling::core::aggregate_engine_health({});

    EXPECT_EQ(aggregate.status, scau::coupling::core::EngineHealthAggregateStatus::all_healthy);
    EXPECT_EQ(aggregate.report_count, 0U);
    EXPECT_EQ(aggregate.unhealthy_count, 0U);
}

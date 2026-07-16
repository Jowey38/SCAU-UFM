#include <gtest/gtest.h>

#include "coupling/core/state.hpp"

TEST(CouplingFaultControllerDiagnostic, HealthyAggregateProducesNominalDiagnostic) {
    const scau::coupling::core::EngineHealthAggregate health{
        .status = scau::coupling::core::EngineHealthAggregateStatus::all_healthy,
        .report_count = 2U,
        .unhealthy_count = 0U,
    };

    const auto diagnostic = scau::coupling::core::make_fault_controller_diagnostic(health);

    EXPECT_EQ(diagnostic.status, scau::coupling::core::FaultControllerDiagnosticStatus::nominal);
    EXPECT_EQ(diagnostic.health.status, scau::coupling::core::EngineHealthAggregateStatus::all_healthy);
    EXPECT_EQ(diagnostic.health.report_count, 2U);
    EXPECT_EQ(diagnostic.health.unhealthy_count, 0U);
    EXPECT_FALSE(diagnostic.should_isolate);
    EXPECT_FALSE(diagnostic.should_reconnect);
    EXPECT_FALSE(diagnostic.should_abort);
}

TEST(CouplingFaultControllerDiagnostic, UnhealthyAggregateProducesFaultDetectedDiagnostic) {
    const scau::coupling::core::EngineHealthAggregate health{
        .status = scau::coupling::core::EngineHealthAggregateStatus::any_unhealthy,
        .report_count = 2U,
        .unhealthy_count = 1U,
    };

    const auto diagnostic = scau::coupling::core::make_fault_controller_diagnostic(health);

    EXPECT_EQ(diagnostic.status, scau::coupling::core::FaultControllerDiagnosticStatus::fault_detected);
    EXPECT_EQ(diagnostic.health.status, scau::coupling::core::EngineHealthAggregateStatus::any_unhealthy);
    EXPECT_EQ(diagnostic.health.report_count, 2U);
    EXPECT_EQ(diagnostic.health.unhealthy_count, 1U);
    EXPECT_FALSE(diagnostic.should_isolate);
    EXPECT_FALSE(diagnostic.should_reconnect);
    EXPECT_FALSE(diagnostic.should_abort);
}

TEST(CouplingFaultControllerDiagnostic, EmptyHealthyAggregateRemainsPassive) {
    const scau::coupling::core::EngineHealthAggregate health{
        .status = scau::coupling::core::EngineHealthAggregateStatus::all_healthy,
        .report_count = 0U,
        .unhealthy_count = 0U,
    };

    const auto diagnostic = scau::coupling::core::make_fault_controller_diagnostic(health);

    EXPECT_EQ(diagnostic.status, scau::coupling::core::FaultControllerDiagnosticStatus::nominal);
    EXPECT_EQ(diagnostic.health.report_count, 0U);
    EXPECT_EQ(diagnostic.health.unhealthy_count, 0U);
    EXPECT_FALSE(diagnostic.should_isolate);
    EXPECT_FALSE(diagnostic.should_reconnect);
    EXPECT_FALSE(diagnostic.should_abort);
}

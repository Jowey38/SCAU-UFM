#include <gtest/gtest.h>

#include "coupling/core/state.hpp"

TEST(CouplingFaultControllerPolicy, NominalDiagnosticProposesContinueRun) {
    const scau::coupling::core::FaultControllerDiagnostic diagnostic{
        .status = scau::coupling::core::FaultControllerDiagnosticStatus::nominal,
        .health = {
            .status = scau::coupling::core::EngineHealthAggregateStatus::all_healthy,
            .report_count = 2U,
            .unhealthy_count = 0U,
        },
    };

    const auto action = scau::coupling::core::propose_fault_controller_action(diagnostic);

    EXPECT_EQ(action.state, scau::coupling::core::FaultControllerProposedActionState::continue_run);
    EXPECT_EQ(action.diagnostic.status, scau::coupling::core::FaultControllerDiagnosticStatus::nominal);
    EXPECT_EQ(action.diagnostic.health.report_count, 2U);
    EXPECT_EQ(action.diagnostic.health.unhealthy_count, 0U);
    EXPECT_FALSE(action.execute_isolation);
    EXPECT_FALSE(action.execute_reconnect);
    EXPECT_FALSE(action.execute_abort);
}

TEST(CouplingFaultControllerPolicy, FaultDetectedDiagnosticProposesReviewRequired) {
    const scau::coupling::core::FaultControllerDiagnostic diagnostic{
        .status = scau::coupling::core::FaultControllerDiagnosticStatus::fault_detected,
        .health = {
            .status = scau::coupling::core::EngineHealthAggregateStatus::any_unhealthy,
            .report_count = 2U,
            .unhealthy_count = 1U,
        },
    };

    const auto action = scau::coupling::core::propose_fault_controller_action(diagnostic);

    EXPECT_EQ(action.state, scau::coupling::core::FaultControllerProposedActionState::review_required);
    EXPECT_EQ(action.diagnostic.status, scau::coupling::core::FaultControllerDiagnosticStatus::fault_detected);
    EXPECT_EQ(action.diagnostic.health.report_count, 2U);
    EXPECT_EQ(action.diagnostic.health.unhealthy_count, 1U);
    EXPECT_FALSE(action.execute_isolation);
    EXPECT_FALSE(action.execute_reconnect);
    EXPECT_FALSE(action.execute_abort);
}

TEST(CouplingFaultControllerPolicy, ProposalDoesNotPromotePassiveDiagnosticFlagsToExecution) {
    const scau::coupling::core::FaultControllerDiagnostic diagnostic{
        .status = scau::coupling::core::FaultControllerDiagnosticStatus::fault_detected,
        .health = {
            .status = scau::coupling::core::EngineHealthAggregateStatus::any_unhealthy,
            .report_count = 1U,
            .unhealthy_count = 1U,
        },
        .should_isolate = true,
        .should_reconnect = true,
        .should_abort = true,
    };

    const auto action = scau::coupling::core::propose_fault_controller_action(diagnostic);

    EXPECT_EQ(action.state, scau::coupling::core::FaultControllerProposedActionState::review_required);
    EXPECT_TRUE(action.diagnostic.should_isolate);
    EXPECT_TRUE(action.diagnostic.should_reconnect);
    EXPECT_TRUE(action.diagnostic.should_abort);
    EXPECT_FALSE(action.execute_isolation);
    EXPECT_FALSE(action.execute_reconnect);
    EXPECT_FALSE(action.execute_abort);
}

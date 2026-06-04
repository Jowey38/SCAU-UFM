#include <vector>

#include <gtest/gtest.h>

#include "coupling/core/state.hpp"

namespace {

scau::coupling::core::ExchangeCellState make_ordering_cell() {
    return {
        .volume = 40.0,
        .mass_deficit_account = {.volume = 12.0},
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
    };
}

std::vector<scau::coupling::core::SharedExchangeIntent> make_ordering_intents() {
    return {
        {
            .endpoint = {
                .engine = scau::coupling::core::SharedExchangeEngine::drainage,
                .node_id = 10U,
            },
            .q_request = 10.0,
            .priority_weight = 1.0,
        },
        {
            .endpoint = {
                .engine = scau::coupling::core::SharedExchangeEngine::river,
                .node_id = 30U,
            },
            .q_request = 10.0,
            .priority_weight = 2.0,
        },
    };
}

scau::coupling::core::FaultControllerProposedAction make_review_required_action() {
    const scau::coupling::core::EngineHealthAggregate health{
        .status = scau::coupling::core::EngineHealthAggregateStatus::any_unhealthy,
        .report_count = 2U,
        .unhealthy_count = 1U,
    };
    return scau::coupling::core::propose_fault_controller_action(
        scau::coupling::core::make_fault_controller_diagnostic(health));
}

void expect_same_decisions(
    const std::vector<scau::coupling::core::SharedExchangeDecision>& lhs,
    const std::vector<scau::coupling::core::SharedExchangeDecision>& rhs) {
    ASSERT_EQ(lhs.size(), rhs.size());
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        EXPECT_EQ(lhs[i].endpoint.engine, rhs[i].endpoint.engine);
        EXPECT_EQ(lhs[i].endpoint.node_id, rhs[i].endpoint.node_id);
        EXPECT_DOUBLE_EQ(lhs[i].exchange.q_granted, rhs[i].exchange.q_granted);
        EXPECT_DOUBLE_EQ(lhs[i].exchange.v_granted, rhs[i].exchange.v_granted);
        EXPECT_DOUBLE_EQ(lhs[i].exchange.q_repay, rhs[i].exchange.q_repay);
        EXPECT_DOUBLE_EQ(lhs[i].exchange.v_repay, rhs[i].exchange.v_repay);
        EXPECT_DOUBLE_EQ(lhs[i].exchange.v_unmet, rhs[i].exchange.v_unmet);
        EXPECT_DOUBLE_EQ(lhs[i].allocated_limit.q_limit, rhs[i].allocated_limit.q_limit);
        EXPECT_DOUBLE_EQ(lhs[i].allocated_limit.v_limit, rhs[i].allocated_limit.v_limit);
        EXPECT_DOUBLE_EQ(lhs[i].priority_weight, rhs[i].priority_weight);
    }
}

}  // namespace

TEST(CouplingMockSchedulerFaultObservationOrdering, ObservationBeforeSchedulerDoesNotChangeSchedulingReplayOrAudit) {
    const std::vector<scau::coupling::core::ExchangeCellState> initial_cells{make_ordering_cell()};
    scau::coupling::core::CouplingState control{initial_cells};
    scau::coupling::core::CouplingState observed{initial_cells};

    const auto control_snapshot = control.snapshot();
    const auto observed_snapshot = observed.snapshot();

    const auto action = make_review_required_action();
    const auto observation = scau::coupling::core::observe_mock_scheduler_fault_action(action);

    const auto control_step = control.run_mock_coupling_scheduler_step(
        0U,
        make_ordering_intents(),
        4.0);
    const auto observed_step = observed.run_mock_coupling_scheduler_step(
        0U,
        make_ordering_intents(),
        4.0);

    EXPECT_TRUE(observation.observed_review_required);
    EXPECT_FALSE(observation.executed_isolation);
    EXPECT_FALSE(observation.executed_reconnect);
    EXPECT_FALSE(observation.executed_abort);
    expect_same_decisions(control_step.decisions, observed_step.decisions);

    ASSERT_EQ(control.cells().size(), observed.cells().size());
    EXPECT_DOUBLE_EQ(control.cells()[0].volume, observed.cells()[0].volume);
    EXPECT_DOUBLE_EQ(
        control.cells()[0].mass_deficit_account.volume,
        observed.cells()[0].mass_deficit_account.volume);
    EXPECT_DOUBLE_EQ(control.cells()[0].h, observed.cells()[0].h);

    const auto control_delta = control.audit_system_mass_against_snapshot(control_snapshot, 0.01);
    const auto observed_delta = observed.audit_system_mass_against_snapshot(observed_snapshot, 0.01);
    EXPECT_DOUBLE_EQ(control_delta.baseline.total_mass, observed_delta.baseline.total_mass);
    EXPECT_DOUBLE_EQ(control_delta.current.total_mass, observed_delta.current.total_mass);
    EXPECT_DOUBLE_EQ(control_delta.residual, observed_delta.residual);
    EXPECT_EQ(control_delta.conserved, observed_delta.conserved);
}

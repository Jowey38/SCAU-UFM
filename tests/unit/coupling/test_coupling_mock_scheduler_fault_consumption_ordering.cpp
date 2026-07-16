#include <vector>

#include <gtest/gtest.h>

#include "coupling/core/state.hpp"

namespace {

scau::coupling::core::ExchangeCellState make_consumption_ordering_cell() {
    return {
        .volume = 40.0,
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
        .shared_deficit_accounts = {
            {.endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::drainage, .node_id = 10U}, .mass_deficit_account = {.volume = 4.0}},
            {.endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::river, .node_id = 30U}, .mass_deficit_account = {.volume = 8.0}},
        },
    };
}

std::vector<scau::coupling::core::SharedExchangeIntent> make_consumption_ordering_intents() {
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

scau::coupling::core::MockCouplingSchedulerFaultConsumption make_review_required_consumption() {
    const scau::coupling::core::EngineHealthAggregate health{
        .status = scau::coupling::core::EngineHealthAggregateStatus::any_unhealthy,
        .report_count = 2U,
        .unhealthy_count = 1U,
    };
    const auto action = scau::coupling::core::propose_fault_controller_action(
        scau::coupling::core::make_fault_controller_diagnostic(health));
    return scau::coupling::core::consume_mock_scheduler_fault_action(
        scau::coupling::core::observe_mock_scheduler_fault_action(action));
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

void expect_same_shared_deficit_accounts(
    const std::vector<scau::coupling::core::SharedExchangeEndpointDeficit>& lhs,
    const std::vector<scau::coupling::core::SharedExchangeEndpointDeficit>& rhs) {
    ASSERT_EQ(lhs.size(), rhs.size());
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        EXPECT_EQ(lhs[i].endpoint.engine, rhs[i].endpoint.engine);
        EXPECT_EQ(lhs[i].endpoint.node_id, rhs[i].endpoint.node_id);
        EXPECT_DOUBLE_EQ(lhs[i].mass_deficit_account.volume, rhs[i].mass_deficit_account.volume);
    }
}

}  // namespace

TEST(CouplingMockSchedulerFaultConsumptionOrdering, ConsumptionBeforeSchedulerDoesNotChangeSchedulingReplayOrAudit) {
    const std::vector<scau::coupling::core::ExchangeCellState> initial_cells{make_consumption_ordering_cell()};
    scau::coupling::core::CouplingState control{initial_cells};
    scau::coupling::core::CouplingState consumed{initial_cells};

    const auto control_snapshot = control.snapshot();
    const auto consumed_snapshot = consumed.snapshot();

    const auto consumption = make_review_required_consumption();

    const auto control_step = control.run_mock_coupling_scheduler_step(
        0U,
        make_consumption_ordering_intents(),
        4.0);
    const auto consumed_step = consumed.run_mock_coupling_scheduler_step(
        0U,
        make_consumption_ordering_intents(),
        4.0);

    EXPECT_TRUE(consumption.review_required_consumed);
    EXPECT_TRUE(consumption.exchange_scheduling_allowed);
    EXPECT_TRUE(consumption.replay_allowed);
    EXPECT_TRUE(consumption.audit_allowed);
    EXPECT_FALSE(consumption.executed_isolation);
    EXPECT_FALSE(consumption.executed_reconnect);
    EXPECT_FALSE(consumption.executed_abort);
    expect_same_decisions(control_step.decisions, consumed_step.decisions);

    ASSERT_EQ(control.cells().size(), consumed.cells().size());
    EXPECT_DOUBLE_EQ(control.cells()[0].volume, consumed.cells()[0].volume);
    EXPECT_DOUBLE_EQ(
        control.cells()[0].mass_deficit_account.volume,
        consumed.cells()[0].mass_deficit_account.volume);
    expect_same_shared_deficit_accounts(
        control.cells()[0].shared_deficit_accounts,
        consumed.cells()[0].shared_deficit_accounts);
    EXPECT_DOUBLE_EQ(control.cells()[0].h, consumed.cells()[0].h);

    const auto control_delta = control.audit_system_mass_against_snapshot(control_snapshot, 0.01);
    const auto consumed_delta = consumed.audit_system_mass_against_snapshot(consumed_snapshot, 0.01);
    EXPECT_DOUBLE_EQ(control_delta.baseline.total_mass, consumed_delta.baseline.total_mass);
    EXPECT_DOUBLE_EQ(control_delta.current.total_mass, consumed_delta.current.total_mass);
    EXPECT_DOUBLE_EQ(control_delta.residual, consumed_delta.residual);
    EXPECT_EQ(control_delta.conserved, consumed_delta.conserved);
}

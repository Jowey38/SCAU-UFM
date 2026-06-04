#include <vector>

#include <gtest/gtest.h>

#include "coupling/core/state.hpp"

namespace {

scau::coupling::core::ExchangeCellState make_passive_state_ordering_cell() {
    return {
        .volume = 40.0,
        .mass_deficit_account = {.volume = 12.0},
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
    };
}

std::vector<scau::coupling::core::SharedExchangeIntent> make_passive_state_ordering_intents() {
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

scau::coupling::core::FaultControllerPassiveStateClassification make_review_required_classification() {
    const scau::coupling::core::EngineHealthAggregate health{
        .status = scau::coupling::core::EngineHealthAggregateStatus::any_unhealthy,
        .report_count = 2U,
        .unhealthy_count = 1U,
    };
    const auto action = scau::coupling::core::propose_fault_controller_action(
        scau::coupling::core::make_fault_controller_diagnostic(health));
    const auto observation = scau::coupling::core::observe_mock_scheduler_fault_action(action);
    return scau::coupling::core::classify_fault_controller_passive_state(
        scau::coupling::core::consume_mock_scheduler_fault_action(observation));
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

TEST(CouplingFaultControllerPassiveStateOrdering, ClassificationBeforeSchedulerDoesNotChangeSchedulingReplayOrAudit) {
    const std::vector<scau::coupling::core::ExchangeCellState> initial_cells{make_passive_state_ordering_cell()};
    scau::coupling::core::CouplingState control{initial_cells};
    scau::coupling::core::CouplingState classified{initial_cells};

    const auto control_snapshot = control.snapshot();
    const auto classified_snapshot = classified.snapshot();

    const auto classification = make_review_required_classification();

    const auto control_step = control.run_mock_coupling_scheduler_step(
        0U,
        make_passive_state_ordering_intents(),
        4.0);
    const auto classified_step = classified.run_mock_coupling_scheduler_step(
        0U,
        make_passive_state_ordering_intents(),
        4.0);

    EXPECT_EQ(
        classification.state,
        scau::coupling::core::FaultControllerPassiveStateLabel::review_required);
    EXPECT_FALSE(classification.scheduler_control_enabled);
    EXPECT_FALSE(classification.isolation_enabled);
    EXPECT_FALSE(classification.reconnect_enabled);
    EXPECT_FALSE(classification.abort_enabled);
    expect_same_decisions(control_step.decisions, classified_step.decisions);

    ASSERT_EQ(control.cells().size(), classified.cells().size());
    EXPECT_DOUBLE_EQ(control.cells()[0].volume, classified.cells()[0].volume);
    EXPECT_DOUBLE_EQ(
        control.cells()[0].mass_deficit_account.volume,
        classified.cells()[0].mass_deficit_account.volume);
    EXPECT_DOUBLE_EQ(control.cells()[0].h, classified.cells()[0].h);

    const auto control_delta = control.audit_system_mass_against_snapshot(control_snapshot, 0.01);
    const auto classified_delta = classified.audit_system_mass_against_snapshot(classified_snapshot, 0.01);
    EXPECT_DOUBLE_EQ(control_delta.baseline.total_mass, classified_delta.baseline.total_mass);
    EXPECT_DOUBLE_EQ(control_delta.current.total_mass, classified_delta.current.total_mass);
    EXPECT_DOUBLE_EQ(control_delta.residual, classified_delta.residual);
    EXPECT_EQ(control_delta.conserved, classified_delta.conserved);
}

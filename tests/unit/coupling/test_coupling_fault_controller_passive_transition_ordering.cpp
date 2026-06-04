#include <vector>

#include <gtest/gtest.h>

#include "coupling/core/state.hpp"

namespace {

scau::coupling::core::ExchangeCellState make_passive_transition_ordering_cell() {
    return {
        .volume = 40.0,
        .mass_deficit_account = {.volume = 12.0},
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
    };
}

std::vector<scau::coupling::core::SharedExchangeIntent> make_passive_transition_ordering_intents() {
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

scau::coupling::core::FaultControllerPassiveTransition make_review_required_transition() {
    const scau::coupling::core::EngineHealthAggregate health{
        .status = scau::coupling::core::EngineHealthAggregateStatus::any_unhealthy,
        .report_count = 2U,
        .unhealthy_count = 1U,
    };
    const auto action = scau::coupling::core::propose_fault_controller_action(
        scau::coupling::core::make_fault_controller_diagnostic(health));
    const auto observation = scau::coupling::core::observe_mock_scheduler_fault_action(action);
    const auto consumption = scau::coupling::core::consume_mock_scheduler_fault_action(observation);
    const auto classification = scau::coupling::core::classify_fault_controller_passive_state(consumption);
    return scau::coupling::core::make_fault_controller_passive_transition(
        scau::coupling::core::FaultControllerPassiveStateLabel::running,
        classification);
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

TEST(CouplingFaultControllerPassiveTransitionOrdering, TransitionBeforeSchedulerDoesNotChangeSchedulingReplayOrAudit) {
    const std::vector<scau::coupling::core::ExchangeCellState> initial_cells{make_passive_transition_ordering_cell()};
    scau::coupling::core::CouplingState control{initial_cells};
    scau::coupling::core::CouplingState transitioned{initial_cells};

    const auto control_snapshot = control.snapshot();
    const auto transitioned_snapshot = transitioned.snapshot();

    const auto transition = make_review_required_transition();

    const auto control_step = control.run_mock_coupling_scheduler_step(
        0U,
        make_passive_transition_ordering_intents(),
        4.0);
    const auto transitioned_step = transitioned.run_mock_coupling_scheduler_step(
        0U,
        make_passive_transition_ordering_intents(),
        4.0);

    EXPECT_EQ(
        transition.previous_state,
        scau::coupling::core::FaultControllerPassiveStateLabel::running);
    EXPECT_EQ(
        transition.next_state,
        scau::coupling::core::FaultControllerPassiveStateLabel::review_required);
    EXPECT_EQ(
        transition.reason,
        scau::coupling::core::FaultControllerPassiveTransitionReason::fault_detected_review_required);
    EXPECT_FALSE(transition.isolation_requested);
    EXPECT_FALSE(transition.reconnect_requested);
    EXPECT_FALSE(transition.abort_requested);
    EXPECT_FALSE(transition.runtime_action_executed);
    EXPECT_FALSE(transition.scheduler_control_enabled);
    expect_same_decisions(control_step.decisions, transitioned_step.decisions);

    ASSERT_EQ(control.cells().size(), transitioned.cells().size());
    EXPECT_DOUBLE_EQ(control.cells()[0].volume, transitioned.cells()[0].volume);
    EXPECT_DOUBLE_EQ(
        control.cells()[0].mass_deficit_account.volume,
        transitioned.cells()[0].mass_deficit_account.volume);
    EXPECT_DOUBLE_EQ(control.cells()[0].h, transitioned.cells()[0].h);

    const auto control_delta = control.audit_system_mass_against_snapshot(control_snapshot, 0.01);
    const auto transitioned_delta = transitioned.audit_system_mass_against_snapshot(transitioned_snapshot, 0.01);
    EXPECT_DOUBLE_EQ(control_delta.baseline.total_mass, transitioned_delta.baseline.total_mass);
    EXPECT_DOUBLE_EQ(control_delta.current.total_mass, transitioned_delta.current.total_mass);
    EXPECT_DOUBLE_EQ(control_delta.residual, transitioned_delta.residual);
    EXPECT_EQ(control_delta.conserved, transitioned_delta.conserved);
}

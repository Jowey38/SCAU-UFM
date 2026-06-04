#include <vector>

#include <gtest/gtest.h>

#include "coupling/core/state.hpp"

namespace {

scau::coupling::core::ExchangeCellState make_blocked_action_ordering_cell() {
    return {
        .volume = 40.0,
        .mass_deficit_account = {.volume = 12.0},
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
    };
}

std::vector<scau::coupling::core::SharedExchangeIntent> make_blocked_action_ordering_intents() {
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

scau::coupling::core::FaultControllerBlockedAction make_review_required_blocked_action() {
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
    const auto transition = scau::coupling::core::make_fault_controller_passive_transition(
        scau::coupling::core::FaultControllerPassiveStateLabel::running,
        classification);
    const auto audit = scau::coupling::core::make_fault_controller_passive_action_audit_record(transition);
    const auto outcome = scau::coupling::core::make_fault_controller_passive_action_outcome(audit);
    return scau::coupling::core::make_fault_controller_blocked_action(outcome);
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

TEST(CouplingFaultControllerBlockedActionOrdering, BlockedActionBeforeSchedulerDoesNotChangeSchedulingReplayOrAudit) {
    const std::vector<scau::coupling::core::ExchangeCellState> initial_cells{make_blocked_action_ordering_cell()};
    scau::coupling::core::CouplingState control{initial_cells};
    scau::coupling::core::CouplingState blocked_recorded{initial_cells};

    const auto control_snapshot = control.snapshot();
    const auto blocked_recorded_snapshot = blocked_recorded.snapshot();

    const auto blocked = make_review_required_blocked_action();

    const auto control_step = control.run_mock_coupling_scheduler_step(
        0U,
        make_blocked_action_ordering_intents(),
        4.0);
    const auto blocked_recorded_step = blocked_recorded.run_mock_coupling_scheduler_step(
        0U,
        make_blocked_action_ordering_intents(),
        4.0);

    EXPECT_EQ(
        blocked.reason,
        scau::coupling::core::FaultControllerBlockedActionReason::operator_review_required);
    EXPECT_FALSE(blocked.execution_allowed);
    EXPECT_FALSE(blocked.scheduler_control_allowed);
    EXPECT_FALSE(blocked.adapter_call_allowed);
    EXPECT_FALSE(blocked.isolation_allowed);
    EXPECT_FALSE(blocked.reconnect_allowed);
    EXPECT_FALSE(blocked.abort_allowed);
    EXPECT_FALSE(blocked.release_gate_action_allowed);
    expect_same_decisions(control_step.decisions, blocked_recorded_step.decisions);

    ASSERT_EQ(control.cells().size(), blocked_recorded.cells().size());
    EXPECT_DOUBLE_EQ(control.cells()[0].volume, blocked_recorded.cells()[0].volume);
    EXPECT_DOUBLE_EQ(
        control.cells()[0].mass_deficit_account.volume,
        blocked_recorded.cells()[0].mass_deficit_account.volume);
    EXPECT_DOUBLE_EQ(control.cells()[0].h, blocked_recorded.cells()[0].h);

    const auto control_delta = control.audit_system_mass_against_snapshot(control_snapshot, 0.01);
    const auto blocked_recorded_delta = blocked_recorded.audit_system_mass_against_snapshot(blocked_recorded_snapshot, 0.01);
    EXPECT_DOUBLE_EQ(control_delta.baseline.total_mass, blocked_recorded_delta.baseline.total_mass);
    EXPECT_DOUBLE_EQ(control_delta.current.total_mass, blocked_recorded_delta.current.total_mass);
    EXPECT_DOUBLE_EQ(control_delta.residual, blocked_recorded_delta.residual);
    EXPECT_EQ(control_delta.conserved, blocked_recorded_delta.conserved);
}

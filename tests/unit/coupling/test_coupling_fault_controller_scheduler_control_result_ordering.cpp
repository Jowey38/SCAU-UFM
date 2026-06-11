#include <vector>

#include <gtest/gtest.h>

#include "coupling/core/state.hpp"

namespace {

scau::coupling::core::ExchangeCellState make_scheduler_control_result_ordering_cell() {
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

std::vector<scau::coupling::core::SharedExchangeIntent> make_scheduler_control_result_ordering_intents() {
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

scau::coupling::core::FaultControllerSchedulerControlResult make_review_required_scheduler_control_result() {
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
    const auto blocked = scau::coupling::core::make_fault_controller_blocked_action(outcome);
    const auto request = scau::coupling::core::make_fault_controller_scheduler_control_request(blocked);
    return scau::coupling::core::make_fault_controller_scheduler_control_result(request);
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

TEST(CouplingFaultControllerSchedulerControlResultOrdering, ResultBeforeSchedulerDoesNotChangeSchedulingReplayOrAudit) {
    const std::vector<scau::coupling::core::ExchangeCellState> initial_cells{make_scheduler_control_result_ordering_cell()};
    scau::coupling::core::CouplingState control{initial_cells};
    scau::coupling::core::CouplingState result_recorded{initial_cells};

    const auto control_snapshot = control.snapshot();
    const auto result_recorded_snapshot = result_recorded.snapshot();

    const auto result = make_review_required_scheduler_control_result();

    const auto control_step = control.run_mock_coupling_scheduler_step(
        0U,
        make_scheduler_control_result_ordering_intents(),
        4.0);
    const auto result_recorded_step = result_recorded.run_mock_coupling_scheduler_step(
        0U,
        make_scheduler_control_result_ordering_intents(),
        4.0);

    EXPECT_EQ(
        result.attempted_kind,
        scau::coupling::core::FaultControllerSchedulerControlKind::observe_only);
    EXPECT_EQ(
        result.status,
        scau::coupling::core::FaultControllerSchedulerControlResultStatus::blocked_boundary_absent);
    EXPECT_EQ(
        result.reason,
        scau::coupling::core::FaultControllerSchedulerControlResultReason::scheduler_control_boundary_absent);
    EXPECT_FALSE(result.scheduler_control_used);
    EXPECT_FALSE(result.exchange_requests_paused);
    EXPECT_FALSE(result.target_engine_request_skipped);
    EXPECT_FALSE(result.replay_held);
    EXPECT_FALSE(result.mass_audit_forced);
    EXPECT_FALSE(result.scheduler_state_mutated);
    EXPECT_FALSE(result.adapter_call_attempted);
    EXPECT_FALSE(result.release_gate_action_executed);
    expect_same_decisions(control_step.decisions, result_recorded_step.decisions);

    ASSERT_EQ(control.cells().size(), result_recorded.cells().size());
    EXPECT_DOUBLE_EQ(control.cells()[0].volume, result_recorded.cells()[0].volume);
    EXPECT_DOUBLE_EQ(
        control.cells()[0].mass_deficit_account.volume,
        result_recorded.cells()[0].mass_deficit_account.volume);
    expect_same_shared_deficit_accounts(
        control.cells()[0].shared_deficit_accounts,
        result_recorded.cells()[0].shared_deficit_accounts);
    EXPECT_DOUBLE_EQ(control.cells()[0].h, result_recorded.cells()[0].h);

    const auto control_delta = control.audit_system_mass_against_snapshot(control_snapshot, 0.01);
    const auto result_recorded_delta = result_recorded.audit_system_mass_against_snapshot(result_recorded_snapshot, 0.01);
    EXPECT_DOUBLE_EQ(control_delta.baseline.total_mass, result_recorded_delta.baseline.total_mass);
    EXPECT_DOUBLE_EQ(control_delta.current.total_mass, result_recorded_delta.current.total_mass);
    EXPECT_DOUBLE_EQ(control_delta.residual, result_recorded_delta.residual);
    EXPECT_EQ(control_delta.conserved, result_recorded_delta.conserved);
}

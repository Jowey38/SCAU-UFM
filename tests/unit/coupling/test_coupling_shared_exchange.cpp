#include <limits>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "coupling/core/state.hpp"
#include "coupling/drainage/swmm_boundary.hpp"
#include "coupling/river/dflowfm_boundary.hpp"

namespace {

scau::coupling::core::ExchangeCellState make_shared_cell() {
    return {
        .volume = 40.0,
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
    };
}

}  // namespace

TEST(CouplingSharedExchange, SplitsFlowLimitByPriorityWeight) {
    const auto decisions = scau::coupling::core::evaluate_shared_exchange(
        make_shared_cell(),
        {
            {.endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::drainage, .node_id = 10U}, .q_request = 2.0, .priority_weight = 1.0},
            {.endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::river, .node_id = 30U}, .q_request = 2.0, .priority_weight = 2.0},
        },
        4.0);

    ASSERT_EQ(decisions.size(), 2U);
    EXPECT_DOUBLE_EQ(decisions[0].allocated_limit.v_limit, 12.0);
    EXPECT_DOUBLE_EQ(decisions[0].allocated_limit.q_limit, 3.0);
    EXPECT_DOUBLE_EQ(decisions[1].allocated_limit.v_limit, 24.0);
    EXPECT_DOUBLE_EQ(decisions[1].allocated_limit.q_limit, 6.0);
    EXPECT_DOUBLE_EQ(decisions[0].priority_weight, 1.0);
    EXPECT_DOUBLE_EQ(decisions[1].priority_weight, 2.0);
    EXPECT_EQ(decisions[0].endpoint.engine, scau::coupling::core::SharedExchangeEngine::drainage);
    EXPECT_EQ(decisions[0].endpoint.node_id, 10U);
    EXPECT_EQ(decisions[1].endpoint.engine, scau::coupling::core::SharedExchangeEngine::river);
    EXPECT_EQ(decisions[1].endpoint.node_id, 30U);
}

TEST(CouplingSharedExchange, ScalesCompetingRequestsByPriorityWeight) {
    const auto decisions = scau::coupling::core::evaluate_shared_exchange(
        make_shared_cell(),
        {
            {.endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::drainage, .node_id = 10U}, .q_request = 10.0, .priority_weight = 1.0},
            {.endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::river, .node_id = 30U}, .q_request = 10.0, .priority_weight = 2.0},
        },
        4.0);

    ASSERT_EQ(decisions.size(), 2U);
    EXPECT_DOUBLE_EQ(decisions[0].exchange.q_granted, 3.0);
    EXPECT_DOUBLE_EQ(decisions[0].exchange.v_granted, 12.0);
    EXPECT_DOUBLE_EQ(decisions[0].exchange.v_unmet, 28.0);
    EXPECT_DOUBLE_EQ(decisions[1].exchange.q_granted, 6.0);
    EXPECT_DOUBLE_EQ(decisions[1].exchange.v_granted, 24.0);
    EXPECT_DOUBLE_EQ(decisions[1].exchange.v_unmet, 16.0);

    const double total_granted = decisions[0].exchange.q_granted + decisions[1].exchange.q_granted;
    EXPECT_DOUBLE_EQ(total_granted, 9.0);
}

TEST(CouplingSharedExchange, PreservesZeroRequestAndReportsUnmetVolume) {
    const auto decisions = scau::coupling::core::evaluate_shared_exchange(
        make_shared_cell(),
        {
            {.endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::drainage, .node_id = 10U}, .q_request = 0.0, .priority_weight = 1.0},
            {.endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::river, .node_id = 30U}, .q_request = 10.0, .priority_weight = 2.0},
        },
        4.0);

    ASSERT_EQ(decisions.size(), 2U);
    EXPECT_DOUBLE_EQ(decisions[0].exchange.q_granted, 0.0);
    EXPECT_DOUBLE_EQ(decisions[0].exchange.v_granted, 0.0);
    EXPECT_DOUBLE_EQ(decisions[0].exchange.v_unmet, 0.0);
    EXPECT_DOUBLE_EQ(decisions[1].exchange.q_granted, 6.0);
    EXPECT_DOUBLE_EQ(decisions[1].exchange.v_granted, 24.0);
    EXPECT_DOUBLE_EQ(decisions[1].exchange.v_unmet, 16.0);
}

TEST(CouplingSharedExchange, DeficitRepaymentConsumesSharedLimitBeforeNewRequests) {
    auto cell = make_shared_cell();
    cell.shared_deficit_accounts = {
        {
            .endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::drainage, .node_id = 10U},
            .mass_deficit_account = {.volume = 12.0},
        },
        {
            .endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::river, .node_id = 30U},
            .mass_deficit_account = {.volume = 36.0},
        },
    };

    const auto decisions = scau::coupling::core::evaluate_shared_exchange(
        cell,
        {
            {.endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::drainage, .node_id = 10U}, .q_request = 10.0, .priority_weight = 1.0},
            {.endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::river, .node_id = 30U}, .q_request = 10.0, .priority_weight = 2.0},
        },
        4.0);

    ASSERT_EQ(decisions.size(), 2U);
    EXPECT_NEAR(decisions[0].exchange.q_granted, 0.0, 1.0e-12);
    EXPECT_NEAR(decisions[0].exchange.v_granted, 0.0, 1.0e-12);
    EXPECT_DOUBLE_EQ(decisions[0].exchange.q_repay, 3.0);
    EXPECT_DOUBLE_EQ(decisions[0].exchange.v_repay, 12.0);
    EXPECT_DOUBLE_EQ(decisions[0].exchange.v_unmet, 40.0);
    EXPECT_DOUBLE_EQ(decisions[1].exchange.q_granted, 0.0);
    EXPECT_DOUBLE_EQ(decisions[1].exchange.v_granted, 0.0);
    EXPECT_DOUBLE_EQ(decisions[1].exchange.q_repay, 6.0);
    EXPECT_DOUBLE_EQ(decisions[1].exchange.v_repay, 24.0);
    EXPECT_DOUBLE_EQ(decisions[1].exchange.v_unmet, 40.0);
}

TEST(CouplingSharedExchange, PartialDeficitRepaymentReducesSharedLimitForNewRequests) {
    auto cell = make_shared_cell();
    cell.shared_deficit_accounts = {
        {
            .endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::drainage, .node_id = 10U},
            .mass_deficit_account = {.volume = 4.0},
        },
        {
            .endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::river, .node_id = 30U},
            .mass_deficit_account = {.volume = 8.0},
        },
    };

    const auto decisions = scau::coupling::core::evaluate_shared_exchange(
        cell,
        {
            {.endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::drainage, .node_id = 10U}, .q_request = 10.0, .priority_weight = 1.0},
            {.endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::river, .node_id = 30U}, .q_request = 10.0, .priority_weight = 2.0},
        },
        4.0);

    ASSERT_EQ(decisions.size(), 2U);
    EXPECT_DOUBLE_EQ(decisions[0].exchange.q_repay, 1.0);
    EXPECT_DOUBLE_EQ(decisions[0].exchange.v_repay, 4.0);
    EXPECT_DOUBLE_EQ(decisions[0].exchange.q_granted, 2.0);
    EXPECT_DOUBLE_EQ(decisions[0].exchange.v_granted, 8.0);
    EXPECT_DOUBLE_EQ(decisions[1].exchange.q_repay, 2.0);
    EXPECT_DOUBLE_EQ(decisions[1].exchange.v_repay, 8.0);
    EXPECT_DOUBLE_EQ(decisions[1].exchange.q_granted, 4.0);
    EXPECT_DOUBLE_EQ(decisions[1].exchange.v_granted, 16.0);

    const double total_flow = decisions[0].exchange.q_repay + decisions[0].exchange.q_granted +
        decisions[1].exchange.q_repay + decisions[1].exchange.q_granted;
    EXPECT_DOUBLE_EQ(total_flow, 9.0);
}

TEST(CouplingSharedExchange, SharedDeficitRepaymentUsesEndpointOwnershipNotCurrentWeights) {
    auto cell = make_shared_cell();
    cell.shared_deficit_accounts = {
        {
            .endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::drainage, .node_id = 10U},
            .mass_deficit_account = {.volume = 12.0},
        },
    };

    const auto decisions = scau::coupling::core::evaluate_shared_exchange(
        cell,
        {
            {.endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::drainage, .node_id = 10U}, .q_request = 10.0, .priority_weight = 1.0},
            {.endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::river, .node_id = 30U}, .q_request = 10.0, .priority_weight = 2.0},
        },
        4.0);

    ASSERT_EQ(decisions.size(), 2U);
    EXPECT_DOUBLE_EQ(decisions[0].exchange.q_repay, 3.0);
    EXPECT_DOUBLE_EQ(decisions[0].exchange.v_repay, 12.0);
    EXPECT_DOUBLE_EQ(decisions[1].exchange.q_repay, 0.0);
    EXPECT_DOUBLE_EQ(decisions[1].exchange.v_repay, 0.0);
}

TEST(CouplingSharedExchange, SharedDeficitOwnershipDistinguishesSameEngineNodeIds) {
    auto cell = make_shared_cell();
    cell.shared_deficit_accounts = {
        {
            .endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::drainage, .node_id = 10U},
            .mass_deficit_account = {.volume = 12.0},
        },
    };

    const auto decisions = scau::coupling::core::evaluate_shared_exchange(
        cell,
        {
            {.endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::drainage, .node_id = 10U}, .q_request = 0.0, .priority_weight = 1.0},
            {.endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::drainage, .node_id = 11U}, .q_request = 0.0, .priority_weight = 1.0},
        },
        4.0);

    ASSERT_EQ(decisions.size(), 2U);
    EXPECT_DOUBLE_EQ(decisions[0].exchange.q_repay, 3.0);
    EXPECT_DOUBLE_EQ(decisions[0].exchange.v_repay, 12.0);
    EXPECT_DOUBLE_EQ(decisions[1].exchange.q_repay, 0.0);
    EXPECT_DOUBLE_EQ(decisions[1].exchange.v_repay, 0.0);
}

TEST(CouplingSharedExchange, RejectsDuplicateSharedIntentEndpoints) {
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::evaluate_shared_exchange(
            make_shared_cell(),
            {
                {.endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::drainage, .node_id = 10U}, .q_request = 1.0, .priority_weight = 1.0},
                {.endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::drainage, .node_id = 10U}, .q_request = 2.0, .priority_weight = 2.0},
            },
            4.0)),
        std::invalid_argument);
}

TEST(CouplingSharedExchange, RejectsDuplicateSharedDeficitAccountEndpoints) {
    auto cell = make_shared_cell();
    cell.shared_deficit_accounts = {
        {
            .endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::drainage, .node_id = 10U},
            .mass_deficit_account = {.volume = 4.0},
        },
        {
            .endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::drainage, .node_id = 10U},
            .mass_deficit_account = {.volume = 8.0},
        },
    };

    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::evaluate_shared_exchange(
            cell,
            {{.endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::drainage, .node_id = 10U}, .q_request = 1.0, .priority_weight = 1.0}},
            4.0)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::CouplingState{{cell}}),
        std::invalid_argument);
}

TEST(CouplingSharedExchange, SharedPipelineSplitsAggregateAppliedVolumeAboveThreshold) {
    const auto pipeline = scau::coupling::core::evaluate_shared_exchange_pipeline(
        make_shared_cell(),
        {
            {.endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::drainage, .node_id = 10U}, .q_request = 10.0, .priority_weight = 1.0},
            {.endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::river, .node_id = 30U}, .q_request = 10.0, .priority_weight = 2.0},
        },
        4.0);

    ASSERT_EQ(pipeline.decisions.size(), 2U);
    EXPECT_TRUE(pipeline.drain_split_engaged);
    EXPECT_EQ(pipeline.drain_split.micro_steps, 5);
    EXPECT_DOUBLE_EQ(pipeline.drain_split.dt_micro, 0.8);
    EXPECT_DOUBLE_EQ(pipeline.drain_split.v_per_micro_step, 7.2);
    EXPECT_FALSE(pipeline.negative_depth_fix_engaged);
}

TEST(CouplingSharedExchange, SharedPipelineIncludesEndpointRepaymentInAggregateSplit) {
    auto cell = make_shared_cell();
    cell.shared_deficit_accounts = {
        {
            .endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::drainage, .node_id = 10U},
            .mass_deficit_account = {.volume = 4.0},
        },
        {
            .endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::river, .node_id = 30U},
            .mass_deficit_account = {.volume = 8.0},
        },
    };

    const auto pipeline = scau::coupling::core::evaluate_shared_exchange_pipeline(
        cell,
        {
            {.endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::drainage, .node_id = 10U}, .q_request = 0.0, .priority_weight = 1.0},
            {.endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::river, .node_id = 30U}, .q_request = 0.0, .priority_weight = 2.0},
        },
        4.0);

    ASSERT_EQ(pipeline.decisions.size(), 2U);
    EXPECT_TRUE(pipeline.drain_split_engaged);
    EXPECT_EQ(pipeline.drain_split.micro_steps, 2);
    EXPECT_DOUBLE_EQ(pipeline.drain_split.dt_micro, 2.0);
    EXPECT_DOUBLE_EQ(pipeline.drain_split.v_per_micro_step, 6.0);
}

TEST(CouplingSharedExchange, AsymmetricRequestsNeverGrantAboveRequestOrGlobalLimit) {
    const auto decisions = scau::coupling::core::evaluate_shared_exchange(
        make_shared_cell(),
        {
            {.endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::drainage, .node_id = 10U}, .q_request = 1.0, .priority_weight = 10.0},
            {.endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::river, .node_id = 30U}, .q_request = 100.0, .priority_weight = 1.0},
        },
        4.0);

    ASSERT_EQ(decisions.size(), 2U);
    EXPECT_LE(decisions[0].exchange.q_granted, 1.0);
    EXPECT_LE(decisions[1].exchange.q_granted, 100.0);
    EXPECT_LE(decisions[0].exchange.q_granted + decisions[1].exchange.q_granted, 9.0);
}

TEST(CouplingSharedExchange, ZeroStorageSharedCellGrantsNoNewRequest) {
    auto cell = make_shared_cell();
    cell.h = 0.0;

    const auto decisions = scau::coupling::core::evaluate_shared_exchange(
        cell,
        {
            {.endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::drainage, .node_id = 10U}, .q_request = 1.0, .priority_weight = 1.0},
            {.endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::river, .node_id = 30U}, .q_request = 2.0, .priority_weight = 2.0},
        },
        4.0);

    ASSERT_EQ(decisions.size(), 2U);
    EXPECT_DOUBLE_EQ(decisions[0].exchange.q_granted, 0.0);
    EXPECT_DOUBLE_EQ(decisions[0].exchange.v_unmet, 4.0);
    EXPECT_DOUBLE_EQ(decisions[1].exchange.q_granted, 0.0);
    EXPECT_DOUBLE_EQ(decisions[1].exchange.v_unmet, 8.0);
}

TEST(CouplingSharedExchange, EmptyIntentListReturnsEmptyDecisionList) {
    const auto decisions = scau::coupling::core::evaluate_shared_exchange(
        make_shared_cell(),
        std::vector<scau::coupling::core::SharedExchangeIntent>{},
        4.0);

    EXPECT_TRUE(decisions.empty());
}

TEST(CouplingSharedExchange, EmptyIntentListStillValidatesCellInputs) {
    auto cell = make_shared_cell();
    cell.phi_t = std::numeric_limits<double>::quiet_NaN();

    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::evaluate_shared_exchange(
            cell,
            std::vector<scau::coupling::core::SharedExchangeIntent>{},
            4.0)),
        std::invalid_argument);
}

TEST(CouplingSharedExchange, RejectsMixedAggregateAndSharedEndpointDeficits) {
    auto cell = make_shared_cell();
    cell.mass_deficit_account.volume = 1.0;
    cell.shared_deficit_accounts = {
        {
            .endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::drainage, .node_id = 10U},
            .mass_deficit_account = {.volume = 4.0},
        },
    };

    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::evaluate_shared_exchange(
            cell,
            {{.endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::drainage, .node_id = 10U}, .q_request = 1.0, .priority_weight = 1.0}},
            4.0)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::CouplingState{{cell}}),
        std::invalid_argument);
}

TEST(CouplingSharedExchange, RejectsInvalidSharedIntentInputs) {
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::evaluate_shared_exchange(
            make_shared_cell(),
            {{.q_request = -1.0, .priority_weight = 1.0}},
            4.0)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::evaluate_shared_exchange(
            make_shared_cell(),
            {{.q_request = std::numeric_limits<double>::quiet_NaN(), .priority_weight = 1.0}},
            4.0)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::evaluate_shared_exchange(
            make_shared_cell(),
            {{.q_request = 1.0, .priority_weight = 0.0}},
            4.0)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::evaluate_shared_exchange(
            make_shared_cell(),
            {{.q_request = 1.0, .priority_weight = std::numeric_limits<double>::infinity()}},
            4.0)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::evaluate_shared_exchange(
            make_shared_cell(),
            {{.q_request = 1.0, .priority_weight = 1.0}},
            0.0)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::evaluate_shared_exchange(
            make_shared_cell(),
            {{.q_request = 1.0, .priority_weight = 1.0}},
            std::numeric_limits<double>::quiet_NaN())),
        std::invalid_argument);
}

TEST(CouplingSharedExchange, RejectsNonFiniteCellInputs) {
    auto nonfinite_phi_t = make_shared_cell();
    nonfinite_phi_t.phi_t = std::numeric_limits<double>::quiet_NaN();
    auto nonfinite_h = make_shared_cell();
    nonfinite_h.h = std::numeric_limits<double>::infinity();
    auto nonfinite_area = make_shared_cell();
    nonfinite_area.area = std::numeric_limits<double>::quiet_NaN();
    auto nonfinite_deficit = make_shared_cell();
    nonfinite_deficit.mass_deficit_account.volume = std::numeric_limits<double>::infinity();

    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::evaluate_shared_exchange(
            nonfinite_phi_t,
            {{.q_request = 1.0, .priority_weight = 1.0}},
            4.0)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::evaluate_shared_exchange(
            nonfinite_h,
            {{.q_request = 1.0, .priority_weight = 1.0}},
            4.0)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::evaluate_shared_exchange(
            nonfinite_area,
            {{.q_request = 1.0, .priority_weight = 1.0}},
            4.0)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::evaluate_shared_exchange(
            nonfinite_deficit,
            {{.q_request = 1.0, .priority_weight = 1.0}},
            4.0)),
        std::invalid_argument);
}

TEST(CouplingSharedExchange, AdapterHelpersCreateTypedSharedExchangeIntents) {
    const auto swmm_intent = scau::coupling::drainage::make_swmm_shared_exchange_intent(10, 4.0, 1.5);
    const auto dflowfm_intent = scau::coupling::river::make_dflowfm_shared_exchange_intent(30, 6.0, 2.5);

    EXPECT_EQ(swmm_intent.endpoint.engine, scau::coupling::core::SharedExchangeEngine::drainage);
    EXPECT_EQ(swmm_intent.endpoint.node_id, 10U);
    EXPECT_DOUBLE_EQ(swmm_intent.q_request, 4.0);
    EXPECT_DOUBLE_EQ(swmm_intent.priority_weight, 1.5);
    EXPECT_EQ(dflowfm_intent.endpoint.engine, scau::coupling::core::SharedExchangeEngine::river);
    EXPECT_EQ(dflowfm_intent.endpoint.node_id, 30U);
    EXPECT_DOUBLE_EQ(dflowfm_intent.q_request, 6.0);
    EXPECT_DOUBLE_EQ(dflowfm_intent.priority_weight, 2.5);
}

TEST(CouplingSharedExchange, AdapterHelpersRejectInvalidSharedExchangeIntentInputs) {
    EXPECT_THROW(
        static_cast<void>(scau::coupling::drainage::make_swmm_shared_exchange_intent(-1, 1.0, 1.0)),
        scau::coupling::drainage::SwmmEngineError);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::drainage::make_swmm_shared_exchange_intent(1, -1.0, 1.0)),
        scau::coupling::drainage::SwmmEngineError);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::drainage::make_swmm_shared_exchange_intent(1, 1.0, 0.0)),
        scau::coupling::drainage::SwmmEngineError);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::river::make_dflowfm_shared_exchange_intent(-1, 1.0, 1.0)),
        scau::coupling::river::DFlowFMEngineError);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::river::make_dflowfm_shared_exchange_intent(1, -1.0, 1.0)),
        scau::coupling::river::DFlowFMEngineError);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::river::make_dflowfm_shared_exchange_intent(1, 1.0, 0.0)),
        scau::coupling::river::DFlowFMEngineError);
}

TEST(CouplingSharedExchange, AdapterSharedAcceptHelpersRejectWrongEngineDecisions) {
    scau::coupling::drainage::MockSwmmEngine swmm;
    scau::coupling::river::MockDFlowFMEngine dflowfm;
    swmm.initialize("mock.inp");
    dflowfm.initialize("mock.mdu");

    const scau::coupling::core::SharedExchangeDecision drainage_decision{
        .endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::drainage, .node_id = 10U},
        .exchange = {.q_granted = 1.0, .v_granted = 4.0},
    };
    const scau::coupling::core::SharedExchangeDecision river_decision{
        .endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::river, .node_id = 30U},
        .exchange = {.q_granted = 1.0, .v_granted = 4.0},
    };

    EXPECT_THROW(
        scau::coupling::drainage::accept_swmm_shared_exchange_decision(swmm, river_decision, 4.0),
        scau::coupling::drainage::SwmmEngineError);
    EXPECT_THROW(
        scau::coupling::river::accept_dflowfm_shared_exchange_decision(dflowfm, drainage_decision, 4.0),
        scau::coupling::river::DFlowFMEngineError);
}

TEST(CouplingSharedExchange, AdapterSharedAcceptHelpersRejectOutOfRangeEndpointIds) {
    scau::coupling::drainage::MockSwmmEngine swmm;
    scau::coupling::river::MockDFlowFMEngine dflowfm;
    swmm.initialize("mock.inp");
    dflowfm.initialize("mock.mdu");

    const auto out_of_range_node_id = static_cast<std::size_t>(std::numeric_limits<int>::max()) + 1U;
    const scau::coupling::core::SharedExchangeDecision drainage_decision{
        .endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::drainage, .node_id = out_of_range_node_id},
        .exchange = {.q_granted = 1.0, .v_granted = 4.0},
    };
    const scau::coupling::core::SharedExchangeDecision river_decision{
        .endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::river, .node_id = out_of_range_node_id},
        .exchange = {.q_granted = 1.0, .v_granted = 4.0},
    };

    EXPECT_THROW(
        scau::coupling::drainage::accept_swmm_shared_exchange_decision(swmm, drainage_decision, 4.0),
        scau::coupling::drainage::SwmmEngineError);
    EXPECT_THROW(
        scau::coupling::river::accept_dflowfm_shared_exchange_decision(dflowfm, river_decision, 4.0),
        scau::coupling::river::DFlowFMEngineError);
}

TEST(CouplingSharedExchange, DualMockSharedCellScenarioAcceptsCoreArbitratedDecisions) {
    scau::coupling::drainage::MockSwmmEngine swmm;
    scau::coupling::river::MockDFlowFMEngine dflowfm;
    swmm.initialize("mock.inp");
    dflowfm.initialize("mock.mdu");

    swmm.set_node_head_fixture(10, 3.25);
    dflowfm.set_water_level_fixture(30, 2.75);

    const auto decisions = scau::coupling::core::evaluate_shared_exchange(
        make_shared_cell(),
        {
            scau::coupling::drainage::make_swmm_shared_exchange_intent(10, 10.0, 1.0),
            scau::coupling::river::make_dflowfm_shared_exchange_intent(30, 10.0, 2.0),
        },
        4.0);
    ASSERT_EQ(decisions.size(), 2U);
    EXPECT_EQ(decisions[0].endpoint.engine, scau::coupling::core::SharedExchangeEngine::drainage);
    EXPECT_EQ(decisions[0].endpoint.node_id, 10U);
    EXPECT_EQ(decisions[1].endpoint.engine, scau::coupling::core::SharedExchangeEngine::river);
    EXPECT_EQ(decisions[1].endpoint.node_id, 30U);

    scau::coupling::drainage::accept_swmm_shared_exchange_decision(swmm, decisions[0], 4.0);
    scau::coupling::river::accept_dflowfm_shared_exchange_decision(dflowfm, decisions[1], 4.0);

    EXPECT_DOUBLE_EQ(swmm.get_node_head(10), 3.25);
    EXPECT_DOUBLE_EQ(swmm.get_node_lateral_inflow(10), 3.0);
    EXPECT_DOUBLE_EQ(dflowfm.get_value("water_level", 30), 2.75);
    EXPECT_DOUBLE_EQ(dflowfm.get_value("lateral_discharge", 30), 6.0);
}

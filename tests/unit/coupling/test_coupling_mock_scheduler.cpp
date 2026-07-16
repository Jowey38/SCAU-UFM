#include <vector>

#include <gtest/gtest.h>

#include "coupling/core/state.hpp"
#include "coupling/drainage/swmm_boundary.hpp"
#include "coupling/river/dflowfm_boundary.hpp"

namespace {

scau::coupling::core::ExchangeCellState make_scheduler_cell() {
    return {
        .volume = 40.0,
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
        .shared_deficit_accounts = {
            {
                .endpoint = {
                    .engine = scau::coupling::core::SharedExchangeEngine::drainage,
                    .node_id = 10U,
                },
                .mass_deficit_account = {.volume = 4.0},
            },
            {
                .endpoint = {
                    .engine = scau::coupling::core::SharedExchangeEngine::river,
                    .node_id = 30U,
                },
                .mass_deficit_account = {.volume = 8.0},
            },
        },
    };
}

std::vector<scau::coupling::core::SharedExchangeIntent> make_scheduler_intents() {
    return {
        scau::coupling::drainage::make_swmm_shared_exchange_intent(10, 10.0, 1.0),
        scau::coupling::river::make_dflowfm_shared_exchange_intent(30, 10.0, 2.0),
    };
}

}  // namespace

TEST(CouplingMockScheduler, StepReturnsDecisionsAndReplaysStateDeterministically) {
    scau::coupling::core::CouplingState state{{make_scheduler_cell()}};

    const auto result = state.run_mock_coupling_scheduler_step(0U, make_scheduler_intents(), 4.0);

    ASSERT_EQ(result.decisions.size(), 2U);
    EXPECT_EQ(result.decisions[0].endpoint.engine, scau::coupling::core::SharedExchangeEngine::drainage);
    EXPECT_EQ(result.decisions[0].endpoint.node_id, 10U);
    EXPECT_EQ(result.decisions[1].endpoint.engine, scau::coupling::core::SharedExchangeEngine::river);
    EXPECT_EQ(result.decisions[1].endpoint.node_id, 30U);
    EXPECT_DOUBLE_EQ(result.decisions[0].exchange.q_granted, 2.0);
    EXPECT_DOUBLE_EQ(result.decisions[1].exchange.q_granted, 4.0);
    ASSERT_EQ(state.cells().size(), 1U);
    EXPECT_NEAR(state.cells()[0].volume, 4.0, 1.0e-12);
    EXPECT_NEAR(state.cells()[0].mass_deficit_account.volume, 0.0, 1.0e-12);
    ASSERT_EQ(state.cells()[0].shared_deficit_accounts.size(), 2U);
    EXPECT_NEAR(state.cells()[0].shared_deficit_accounts[0].mass_deficit_account.volume, 32.0, 1.0e-12);
    EXPECT_NEAR(state.cells()[0].shared_deficit_accounts[1].mass_deficit_account.volume, 24.0, 1.0e-12);
    EXPECT_EQ(state.runtime_counters().count_drain_split, 1U);
    EXPECT_TRUE(state.snapshot().pending_events().empty());
}

TEST(CouplingMockScheduler, ReturnedDecisionsCanBeAcceptedByTypedMockAdapters) {
    scau::coupling::core::CouplingState state{{make_scheduler_cell()}};
    scau::coupling::drainage::MockSwmmEngine swmm;
    scau::coupling::river::MockDFlowFMEngine dflowfm;
    swmm.initialize("mock.inp");
    dflowfm.initialize("mock.mdu");

    const auto result = state.run_mock_coupling_scheduler_step(0U, make_scheduler_intents(), 4.0);
    ASSERT_EQ(result.decisions.size(), 2U);

    scau::coupling::drainage::accept_swmm_shared_exchange_decision(swmm, result.decisions[0], 4.0);
    scau::coupling::river::accept_dflowfm_shared_exchange_decision(dflowfm, result.decisions[1], 4.0);

    EXPECT_DOUBLE_EQ(swmm.get_node_lateral_inflow(10), 3.0);
    EXPECT_DOUBLE_EQ(dflowfm.get_value("lateral_discharge", 30), 6.0);
}

TEST(CouplingMockScheduler, EmptyIntentStepReplaysNoOpAndReturnsNoDecisions) {
    scau::coupling::core::CouplingState state{{make_scheduler_cell()}};

    const auto result = state.run_mock_coupling_scheduler_step(
        0U,
        std::vector<scau::coupling::core::SharedExchangeIntent>{},
        4.0);

    EXPECT_TRUE(result.decisions.empty());
    EXPECT_DOUBLE_EQ(state.cells()[0].volume, 40.0);
    EXPECT_DOUBLE_EQ(state.cells()[0].mass_deficit_account.volume, 0.0);
    ASSERT_EQ(state.cells()[0].shared_deficit_accounts.size(), 2U);
    EXPECT_DOUBLE_EQ(state.cells()[0].shared_deficit_accounts[0].mass_deficit_account.volume, 4.0);
    EXPECT_DOUBLE_EQ(state.cells()[0].shared_deficit_accounts[1].mass_deficit_account.volume, 8.0);
    EXPECT_TRUE(state.snapshot().pending_events().empty());
}

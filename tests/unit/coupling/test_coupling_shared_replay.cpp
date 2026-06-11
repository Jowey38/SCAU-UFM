#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "coupling/core/state.hpp"

namespace {

scau::coupling::core::ExchangeCellState make_replay_cell() {
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

std::vector<scau::coupling::core::SharedExchangeIntent> make_shared_intents() {
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

}  // namespace

TEST(CouplingSharedReplay, ApplySharedExchangeEnqueuesAggregateEventForReplay) {
    scau::coupling::core::CouplingState state{{make_replay_cell()}};

    const auto decisions = state.apply_shared_exchange(0U, make_shared_intents(), 4.0);
    state.replay_pending();

    ASSERT_EQ(decisions.size(), 2U);
    EXPECT_EQ(decisions[0].endpoint.engine, scau::coupling::core::SharedExchangeEngine::drainage);
    EXPECT_EQ(decisions[0].endpoint.node_id, 10U);
    EXPECT_EQ(decisions[1].endpoint.engine, scau::coupling::core::SharedExchangeEngine::river);
    EXPECT_EQ(decisions[1].endpoint.node_id, 30U);
    EXPECT_DOUBLE_EQ(decisions[0].exchange.v_granted, 8.0);
    EXPECT_DOUBLE_EQ(decisions[1].exchange.v_granted, 16.0);
    EXPECT_DOUBLE_EQ(decisions[0].exchange.v_repay, 4.0);
    EXPECT_DOUBLE_EQ(decisions[1].exchange.v_repay, 8.0);
    ASSERT_EQ(state.cells().size(), 1U);
    EXPECT_NEAR(state.cells()[0].volume, 4.0, 1.0e-12);
    ASSERT_EQ(state.cells()[0].shared_deficit_accounts.size(), 2U);
    EXPECT_NEAR(state.cells()[0].shared_deficit_accounts[0].mass_deficit_account.volume, 32.0, 1.0e-12);
    EXPECT_NEAR(state.cells()[0].shared_deficit_accounts[1].mass_deficit_account.volume, 24.0, 1.0e-12);
    EXPECT_NEAR(state.cells()[0].mass_deficit_account.volume, 0.0, 1.0e-12);
}

TEST(CouplingSharedReplay, SharedExchangeReplayIsDeterministicAfterRollback) {
    const std::vector<scau::coupling::core::ExchangeCellState> initial_cells{make_replay_cell()};
    scau::coupling::core::CouplingState fresh{initial_cells};
    scau::coupling::core::CouplingState rolled_back{initial_cells};

    fresh.apply_shared_exchange(0U, make_shared_intents(), 4.0);
    fresh.replay_pending();

    const auto snapshot = rolled_back.snapshot();
    rolled_back.enqueue_event({.exchange_cell_index = 0U, .direction = scau::coupling::core::ExchangeDirection::engine_to_surface, .volume_delta = 999.0});
    rolled_back.replay_pending();
    rolled_back.rollback(snapshot);
    rolled_back.apply_shared_exchange(0U, make_shared_intents(), 4.0);
    rolled_back.replay_pending();

    ASSERT_EQ(fresh.cells().size(), rolled_back.cells().size());
    EXPECT_DOUBLE_EQ(fresh.cells()[0].volume, rolled_back.cells()[0].volume);
    EXPECT_DOUBLE_EQ(
        fresh.cells()[0].mass_deficit_account.volume,
        rolled_back.cells()[0].mass_deficit_account.volume);
    ASSERT_EQ(
        fresh.cells()[0].shared_deficit_accounts.size(),
        rolled_back.cells()[0].shared_deficit_accounts.size());
    for (std::size_t i = 0; i < fresh.cells()[0].shared_deficit_accounts.size(); ++i) {
        EXPECT_EQ(
            fresh.cells()[0].shared_deficit_accounts[i].endpoint.engine,
            rolled_back.cells()[0].shared_deficit_accounts[i].endpoint.engine);
        EXPECT_EQ(
            fresh.cells()[0].shared_deficit_accounts[i].endpoint.node_id,
            rolled_back.cells()[0].shared_deficit_accounts[i].endpoint.node_id);
        EXPECT_DOUBLE_EQ(
            fresh.cells()[0].shared_deficit_accounts[i].mass_deficit_account.volume,
            rolled_back.cells()[0].shared_deficit_accounts[i].mass_deficit_account.volume);
    }
}

TEST(CouplingSharedReplay, SnapshotCapturesPendingSharedExchangeEventForReplay) {
    scau::coupling::core::CouplingState state{{make_replay_cell()}};

    state.apply_shared_exchange(0U, make_shared_intents(), 4.0);
    const auto snapshot = state.snapshot();

    EXPECT_EQ(state.runtime_counters().count_drain_split, 1U);
    EXPECT_EQ(state.runtime_counters().count_negative_depth_fix, 0U);
    ASSERT_EQ(snapshot.pending_events().size(), 1U);
    EXPECT_DOUBLE_EQ(snapshot.pending_events()[0].volume_delta, 36.0);
    EXPECT_DOUBLE_EQ(snapshot.pending_events()[0].unmet_volume, 56.0);
    EXPECT_DOUBLE_EQ(snapshot.pending_events()[0].repayment_volume, 12.0);
    ASSERT_EQ(snapshot.pending_events()[0].shared_endpoint_events.size(), 2U);
    EXPECT_EQ(
        snapshot.pending_events()[0].shared_endpoint_events[0].endpoint.engine,
        scau::coupling::core::SharedExchangeEngine::drainage);
    EXPECT_EQ(snapshot.pending_events()[0].shared_endpoint_events[0].endpoint.node_id, 10U);
    EXPECT_DOUBLE_EQ(snapshot.pending_events()[0].shared_endpoint_events[0].unmet_volume, 32.0);
    EXPECT_DOUBLE_EQ(snapshot.pending_events()[0].shared_endpoint_events[0].repayment_volume, 4.0);
    EXPECT_EQ(
        snapshot.pending_events()[0].shared_endpoint_events[1].endpoint.engine,
        scau::coupling::core::SharedExchangeEngine::river);
    EXPECT_EQ(snapshot.pending_events()[0].shared_endpoint_events[1].endpoint.node_id, 30U);
    EXPECT_DOUBLE_EQ(snapshot.pending_events()[0].shared_endpoint_events[1].unmet_volume, 24.0);
    EXPECT_DOUBLE_EQ(snapshot.pending_events()[0].shared_endpoint_events[1].repayment_volume, 8.0);
}

TEST(CouplingSharedReplay, SameEngineDifferentNodeDeficitReplayUpdatesOwningEndpointOnly) {
    scau::coupling::core::CouplingState state{{{
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
                    .engine = scau::coupling::core::SharedExchangeEngine::drainage,
                    .node_id = 11U,
                },
                .mass_deficit_account = {.volume = 0.0},
            },
        },
    }}};

    state.apply_shared_exchange(
        0U,
        {
            {
                .endpoint = {
                    .engine = scau::coupling::core::SharedExchangeEngine::drainage,
                    .node_id = 10U,
                },
                .q_request = 0.0,
                .priority_weight = 1.0,
            },
            {
                .endpoint = {
                    .engine = scau::coupling::core::SharedExchangeEngine::drainage,
                    .node_id = 11U,
                },
                .q_request = 0.0,
                .priority_weight = 1.0,
            },
        },
        4.0);
    state.replay_pending();

    ASSERT_EQ(state.cells()[0].shared_deficit_accounts.size(), 2U);
    EXPECT_DOUBLE_EQ(state.cells()[0].shared_deficit_accounts[0].mass_deficit_account.volume, 0.0);
    EXPECT_DOUBLE_EQ(state.cells()[0].shared_deficit_accounts[1].mass_deficit_account.volume, 0.0);
}

TEST(CouplingSharedReplay, ZeroStorageSharedExchangeReplayPreservesMetadataAndDoesNotThrow) {
    scau::coupling::core::CouplingState state{{{
        .volume = 0.0,
        .phi_t = 0.4,
        .h = 0.0,
        .area = 50.0,
    }}};

    const auto decisions = state.apply_shared_exchange(
        0U,
        {
            {
                .endpoint = {
                    .engine = scau::coupling::core::SharedExchangeEngine::drainage,
                    .node_id = 10U,
                },
                .q_request = 1.0,
                .priority_weight = 1.0,
            },
        },
        4.0);
    const auto snapshot = state.snapshot();
    state.replay_pending();

    ASSERT_EQ(decisions.size(), 1U);
    EXPECT_DOUBLE_EQ(decisions[0].exchange.v_granted, 0.0);
    EXPECT_DOUBLE_EQ(decisions[0].exchange.v_repay, 0.0);
    EXPECT_DOUBLE_EQ(decisions[0].exchange.v_unmet, 4.0);
    ASSERT_EQ(snapshot.pending_events().size(), 1U);
    EXPECT_DOUBLE_EQ(snapshot.pending_events()[0].volume_delta, 0.0);
    EXPECT_DOUBLE_EQ(snapshot.pending_events()[0].unmet_volume, 4.0);
    EXPECT_DOUBLE_EQ(snapshot.pending_events()[0].repayment_volume, 0.0);
    ASSERT_EQ(snapshot.pending_events()[0].shared_endpoint_events.size(), 1U);
    EXPECT_EQ(
        snapshot.pending_events()[0].shared_endpoint_events[0].endpoint.engine,
        scau::coupling::core::SharedExchangeEngine::drainage);
    EXPECT_EQ(snapshot.pending_events()[0].shared_endpoint_events[0].endpoint.node_id, 10U);
    EXPECT_DOUBLE_EQ(snapshot.pending_events()[0].shared_endpoint_events[0].unmet_volume, 4.0);
    EXPECT_DOUBLE_EQ(snapshot.pending_events()[0].shared_endpoint_events[0].repayment_volume, 0.0);
    EXPECT_DOUBLE_EQ(state.cells()[0].volume, 0.0);
    EXPECT_DOUBLE_EQ(state.cells()[0].h, 0.0);
    ASSERT_EQ(state.cells()[0].shared_deficit_accounts.size(), 1U);
    EXPECT_DOUBLE_EQ(state.cells()[0].shared_deficit_accounts[0].mass_deficit_account.volume, 4.0);
}

TEST(CouplingSharedReplay, EmptySharedExchangeIntentsDoNotEnqueueNoOpEvent) {
    scau::coupling::core::CouplingState state{{make_replay_cell()}};

    const auto decisions = state.apply_shared_exchange(
        0U,
        std::vector<scau::coupling::core::SharedExchangeIntent>{},
        4.0);
    const auto snapshot = state.snapshot();
    state.replay_pending();

    EXPECT_TRUE(decisions.empty());
    EXPECT_TRUE(snapshot.pending_events().empty());
    EXPECT_DOUBLE_EQ(state.cells()[0].volume, 40.0);
    EXPECT_DOUBLE_EQ(state.cells()[0].mass_deficit_account.volume, 0.0);
    ASSERT_EQ(state.cells()[0].shared_deficit_accounts.size(), 2U);
    EXPECT_DOUBLE_EQ(state.cells()[0].shared_deficit_accounts[0].mass_deficit_account.volume, 4.0);
    EXPECT_DOUBLE_EQ(state.cells()[0].shared_deficit_accounts[1].mass_deficit_account.volume, 8.0);
}

TEST(CouplingSharedReplay, ApplySharedExchangeRejectsOutOfRangeCellIndex) {
    scau::coupling::core::CouplingState state{{make_replay_cell()}};

    EXPECT_THROW(
        static_cast<void>(state.apply_shared_exchange(1U, make_shared_intents(), 4.0)),
        std::out_of_range);
}

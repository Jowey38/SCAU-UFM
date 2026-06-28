#include <gtest/gtest.h>

#include "coupling/core/state.hpp"

namespace {
constexpr double kHWet = 1.0e-4;

scau::coupling::core::CouplingState make_aggregate_state() {
    return scau::coupling::core::CouplingState{{
        {.volume = 0.0, .mass_deficit_account = {.volume = 1.0}, .phi_t = 1.0, .h = 0.0, .area = 1.0},
        {.volume = 0.0, .mass_deficit_account = {.volume = 2.0}, .phi_t = 1.0, .h = 0.0, .area = 1.0},
    }};
}

void enqueue_aggregate_sequence(scau::coupling::core::CouplingState& state) {
    state.enqueue_event({
        .exchange_cell_index = 0U,
        .direction = scau::coupling::core::ExchangeDirection::engine_to_surface,
        .volume_delta = 3.0,
        .unmet_volume = 2.0,
    });
    state.enqueue_event({
        .exchange_cell_index = 1U,
        .direction = scau::coupling::core::ExchangeDirection::engine_to_surface,
        .volume_delta = 1.0,
        .repayment_volume = 1.0,
    });
}

scau::coupling::core::CouplingState make_shared_state() {
    return scau::coupling::core::CouplingState{{
        {
            .volume = 0.0,
            .phi_t = 1.0,
            .h = 0.0,
            .area = 1.0,
            .shared_deficit_accounts = {
                {
                    .endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::drainage, .node_id = 10U},
                    .mass_deficit_account = {.volume = 4.0},
                },
                {
                    .endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::river, .node_id = 20U},
                    .mass_deficit_account = {.volume = 1.5},
                },
            },
        },
    }};
}

void enqueue_shared_sequence(scau::coupling::core::CouplingState& state) {
    state.enqueue_event({
        .exchange_cell_index = 0U,
        .direction = scau::coupling::core::ExchangeDirection::engine_to_surface,
        .volume_delta = 2.0,
        .unmet_volume = 2.0,
        .repayment_volume = 1.0,
        .shared_endpoint_events = {
            {
                .endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::drainage, .node_id = 10U},
                .unmet_volume = 1.5,
            },
            {
                .endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::river, .node_id = 20U},
                .unmet_volume = 0.5,
                .repayment_volume = 1.0,
            },
        },
    });
}
}  // namespace

TEST(SnapshotReplayMassDeficitGolden, AggregateDeficitReplayConsistency) {
    auto fresh = make_aggregate_state();
    auto replayed = make_aggregate_state();

    const auto baseline = fresh.compute_system_mass(kHWet);
    const auto replay_snapshot = replayed.snapshot();

    enqueue_aggregate_sequence(fresh);
    fresh.replay_pending();

    replayed.enqueue_event({
        .exchange_cell_index = 0U,
        .direction = scau::coupling::core::ExchangeDirection::engine_to_surface,
        .volume_delta = 999.0,
        .unmet_volume = 999.0,
    });
    replayed.replay_pending();
    replayed.rollback(replay_snapshot);
    enqueue_aggregate_sequence(replayed);
    replayed.replay_pending();

    ASSERT_EQ(fresh.cells().size(), replayed.cells().size());
    for (std::size_t i = 0; i < fresh.cells().size(); ++i) {
        EXPECT_DOUBLE_EQ(fresh.cells()[i].volume, replayed.cells()[i].volume);
        EXPECT_DOUBLE_EQ(
            fresh.cells()[i].mass_deficit_account.volume,
            replayed.cells()[i].mass_deficit_account.volume);
    }

    ASSERT_EQ(fresh.cells().size(), 2U);
    EXPECT_DOUBLE_EQ(fresh.cells()[0].volume, 3.0);
    EXPECT_DOUBLE_EQ(fresh.cells()[0].mass_deficit_account.volume, 3.0);
    EXPECT_DOUBLE_EQ(fresh.cells()[1].volume, 1.0);
    EXPECT_DOUBLE_EQ(fresh.cells()[1].mass_deficit_account.volume, 1.0);
    EXPECT_DOUBLE_EQ(replayed.cells()[0].volume, 3.0);
    EXPECT_DOUBLE_EQ(replayed.cells()[0].mass_deficit_account.volume, 3.0);
    EXPECT_DOUBLE_EQ(replayed.cells()[1].volume, 1.0);
    EXPECT_DOUBLE_EQ(replayed.cells()[1].mass_deficit_account.volume, 1.0);

    const auto fresh_delta = fresh.audit_system_mass_against_reference(baseline, kHWet);
    const auto replayed_delta = replayed.audit_system_mass_against_reference(baseline, kHWet);
    EXPECT_DOUBLE_EQ(baseline.total_mass, 3.0);
    EXPECT_DOUBLE_EQ(fresh_delta.current.surface_mass, 4.0);
    EXPECT_DOUBLE_EQ(fresh_delta.current.deficit_mass, 4.0);
    EXPECT_DOUBLE_EQ(fresh_delta.current.total_mass, 8.0);
    EXPECT_DOUBLE_EQ(fresh_delta.residual, 5.0);
    EXPECT_FALSE(fresh_delta.conserved);
    EXPECT_DOUBLE_EQ(replayed_delta.current.surface_mass, 4.0);
    EXPECT_DOUBLE_EQ(replayed_delta.current.deficit_mass, 4.0);
    EXPECT_DOUBLE_EQ(replayed_delta.current.total_mass, 8.0);
    EXPECT_DOUBLE_EQ(replayed_delta.residual, 5.0);
    EXPECT_FALSE(replayed_delta.conserved);
    EXPECT_DOUBLE_EQ(fresh_delta.current.total_mass, replayed_delta.current.total_mass);
    EXPECT_DOUBLE_EQ(fresh_delta.residual, replayed_delta.residual);
}

TEST(SnapshotReplayMassDeficitGolden, SharedEndpointDeficitReplayConsistency) {
    auto fresh = make_shared_state();
    auto replayed = make_shared_state();

    const auto baseline = fresh.compute_system_mass(kHWet);
    const auto replay_snapshot = replayed.snapshot();

    enqueue_shared_sequence(fresh);
    fresh.replay_pending();

    replayed.enqueue_event({
        .exchange_cell_index = 0U,
        .direction = scau::coupling::core::ExchangeDirection::engine_to_surface,
        .volume_delta = 999.0,
        .unmet_volume = 999.0,
        .shared_endpoint_events = {
            {
                .endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::drainage, .node_id = 10U},
                .unmet_volume = 999.0,
            },
        },
    });
    replayed.replay_pending();
    replayed.rollback(replay_snapshot);
    enqueue_shared_sequence(replayed);
    replayed.replay_pending();

    ASSERT_EQ(fresh.cells().size(), replayed.cells().size());
    for (std::size_t i = 0; i < fresh.cells().size(); ++i) {
        EXPECT_DOUBLE_EQ(fresh.cells()[i].volume, replayed.cells()[i].volume);
        ASSERT_EQ(fresh.cells()[i].shared_deficit_accounts.size(), replayed.cells()[i].shared_deficit_accounts.size());
        for (std::size_t j = 0; j < fresh.cells()[i].shared_deficit_accounts.size(); ++j) {
            EXPECT_EQ(fresh.cells()[i].shared_deficit_accounts[j].endpoint.engine, replayed.cells()[i].shared_deficit_accounts[j].endpoint.engine);
            EXPECT_EQ(fresh.cells()[i].shared_deficit_accounts[j].endpoint.node_id, replayed.cells()[i].shared_deficit_accounts[j].endpoint.node_id);
            EXPECT_DOUBLE_EQ(
                fresh.cells()[i].shared_deficit_accounts[j].mass_deficit_account.volume,
                replayed.cells()[i].shared_deficit_accounts[j].mass_deficit_account.volume);
        }
    }

    ASSERT_EQ(fresh.cells().size(), 1U);
    ASSERT_EQ(fresh.cells()[0].shared_deficit_accounts.size(), 2U);
    EXPECT_DOUBLE_EQ(fresh.cells()[0].volume, 2.0);
    EXPECT_DOUBLE_EQ(fresh.cells()[0].shared_deficit_accounts[0].mass_deficit_account.volume, 5.5);
    EXPECT_DOUBLE_EQ(fresh.cells()[0].shared_deficit_accounts[1].mass_deficit_account.volume, 1.0);
    EXPECT_DOUBLE_EQ(replayed.cells()[0].volume, 2.0);
    EXPECT_DOUBLE_EQ(replayed.cells()[0].shared_deficit_accounts[0].mass_deficit_account.volume, 5.5);
    EXPECT_DOUBLE_EQ(replayed.cells()[0].shared_deficit_accounts[1].mass_deficit_account.volume, 1.0);

    const auto fresh_delta = fresh.audit_system_mass_against_reference(baseline, kHWet);
    const auto replayed_delta = replayed.audit_system_mass_against_reference(baseline, kHWet);
    EXPECT_DOUBLE_EQ(fresh_delta.current.surface_mass, 2.0);
    EXPECT_DOUBLE_EQ(fresh_delta.current.deficit_mass, 6.5);
    EXPECT_DOUBLE_EQ(fresh_delta.current.total_mass, 8.5);
    EXPECT_DOUBLE_EQ(fresh_delta.residual, 3.0);
    EXPECT_FALSE(fresh_delta.conserved);
    EXPECT_DOUBLE_EQ(replayed_delta.current.surface_mass, 2.0);
    EXPECT_DOUBLE_EQ(replayed_delta.current.deficit_mass, 6.5);
    EXPECT_DOUBLE_EQ(replayed_delta.current.total_mass, 8.5);
    EXPECT_DOUBLE_EQ(replayed_delta.residual, 3.0);
    EXPECT_FALSE(replayed_delta.conserved);
    EXPECT_DOUBLE_EQ(fresh_delta.current.total_mass, replayed_delta.current.total_mass);
    EXPECT_DOUBLE_EQ(fresh_delta.residual, replayed_delta.residual);
}

TEST(SnapshotReplayMassDeficitGolden, PendingEventSnapshotFidelity) {
    auto state = make_aggregate_state();
    state.enqueue_event({
        .exchange_cell_index = 0U,
        .direction = scau::coupling::core::ExchangeDirection::engine_to_surface,
        .volume_delta = 2.0,
        .unmet_volume = 1.0,
    });

    const auto snapshot = state.snapshot();
    state.enqueue_event({
        .exchange_cell_index = 1U,
        .direction = scau::coupling::core::ExchangeDirection::engine_to_surface,
        .volume_delta = 3.0,
        .repayment_volume = 0.5,
    });
    state.rollback(snapshot);

    ASSERT_EQ(snapshot.pending_events().size(), 1U);
    EXPECT_DOUBLE_EQ(snapshot.pending_events()[0].volume_delta, 2.0);
    EXPECT_DOUBLE_EQ(snapshot.pending_events()[0].unmet_volume, 1.0);
    EXPECT_DOUBLE_EQ(snapshot.pending_events()[0].repayment_volume, 0.0);

    const auto restored_snapshot = state.snapshot();
    ASSERT_EQ(restored_snapshot.pending_events().size(), 1U);
    EXPECT_DOUBLE_EQ(restored_snapshot.pending_events()[0].volume_delta, 2.0);
    EXPECT_DOUBLE_EQ(restored_snapshot.pending_events()[0].unmet_volume, 1.0);
    EXPECT_DOUBLE_EQ(restored_snapshot.pending_events()[0].repayment_volume, 0.0);

    state.replay_pending();
    const auto drained_snapshot = state.snapshot();
    EXPECT_TRUE(drained_snapshot.pending_events().empty());
    EXPECT_DOUBLE_EQ(state.cells()[0].volume, 2.0);
    EXPECT_DOUBLE_EQ(state.cells()[0].mass_deficit_account.volume, 2.0);
    EXPECT_DOUBLE_EQ(state.cells()[1].volume, 0.0);
    EXPECT_DOUBLE_EQ(state.cells()[1].mass_deficit_account.volume, 2.0);
}

TEST(SnapshotReplayMassDeficitGolden, RuntimeCountersMatchAcrossFreshAndReplayPaths) {
    auto fresh = make_aggregate_state();
    auto replayed = make_aggregate_state();
    const auto snapshot = replayed.snapshot();

    auto drive = [](scau::coupling::core::CouplingState& state) {
        enqueue_aggregate_sequence(state);
        state.replay_pending();
        state.record_pipeline_decision({.drain_split_engaged = true});
        state.record_pipeline_decision({.negative_depth_fix_engaged = true});
    };

    drive(fresh);
    replayed.record_pipeline_decision({.drain_split_engaged = true, .negative_depth_fix_engaged = true});
    replayed.rollback(snapshot);
    drive(replayed);

    EXPECT_TRUE(fresh.snapshot().pending_events().empty());
    EXPECT_TRUE(replayed.snapshot().pending_events().empty());
    EXPECT_EQ(fresh.runtime_counters().count_drain_split, 1U);
    EXPECT_EQ(fresh.runtime_counters().count_negative_depth_fix, 1U);
    EXPECT_EQ(replayed.runtime_counters().count_drain_split, 1U);
    EXPECT_EQ(replayed.runtime_counters().count_negative_depth_fix, 1U);
    EXPECT_EQ(fresh.runtime_counters().count_drain_split, replayed.runtime_counters().count_drain_split);
    EXPECT_EQ(
        fresh.runtime_counters().count_negative_depth_fix,
        replayed.runtime_counters().count_negative_depth_fix);
}

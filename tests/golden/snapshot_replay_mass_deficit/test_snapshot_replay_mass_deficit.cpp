#include <gtest/gtest.h>

#include <vector>

#include "coupling/core/state.hpp"

namespace {

struct SharedDeficitSignature {
    std::size_t cell_index;
    scau::coupling::core::SharedExchangeEngine engine;
    std::size_t node_id;
    double volume;
};

struct PendingEventSignature {
    std::size_t exchange_cell_index;
    scau::coupling::core::ExchangeDirection direction;
    double volume_delta;
    double unmet_volume;
    double repayment_volume;
    std::vector<scau::coupling::core::SharedExchangeEndpointEvent> shared_endpoint_events;
};

struct ReplayRoundSignature {
    std::vector<double> cell_volumes;
    std::vector<double> aggregate_deficits;
    std::vector<SharedDeficitSignature> shared_deficits;
    std::vector<PendingEventSignature> pending_events;
    std::size_t drain_split_count{0U};
    std::size_t negative_depth_fix_count{0U};
    double surface_mass{0.0};
    double deficit_mass{0.0};
    double total_mass{0.0};
    double residual{0.0};
    bool conserved{false};
    bool pending_empty{false};
};

ReplayRoundSignature capture_round_signature(
    const scau::coupling::core::CouplingState& state,
    const scau::coupling::core::SystemMassAudit& baseline,
    double h_wet) {
    ReplayRoundSignature signature;
    signature.cell_volumes.reserve(state.cells().size());
    signature.aggregate_deficits.reserve(state.cells().size());

    for (std::size_t cell_index = 0; cell_index < state.cells().size(); ++cell_index) {
        const auto& cell = state.cells()[cell_index];
        signature.cell_volumes.push_back(cell.volume);
        signature.aggregate_deficits.push_back(cell.mass_deficit_account.volume);
        for (const auto& shared : cell.shared_deficit_accounts) {
            signature.shared_deficits.push_back({
                .cell_index = cell_index,
                .engine = shared.endpoint.engine,
                .node_id = shared.endpoint.node_id,
                .volume = shared.mass_deficit_account.volume,
            });
        }
    }

    const auto snapshot = state.snapshot();
    for (const auto& event : snapshot.pending_events()) {
        signature.pending_events.push_back({
            .exchange_cell_index = event.exchange_cell_index,
            .direction = event.direction,
            .volume_delta = event.volume_delta,
            .unmet_volume = event.unmet_volume,
            .repayment_volume = event.repayment_volume,
            .shared_endpoint_events = event.shared_endpoint_events,
        });
    }

    signature.drain_split_count = state.runtime_counters().count_drain_split;
    signature.negative_depth_fix_count = state.runtime_counters().count_negative_depth_fix;

    const auto delta = state.audit_system_mass_against_reference(baseline, h_wet);
    signature.surface_mass = delta.current.surface_mass;
    signature.deficit_mass = delta.current.deficit_mass;
    signature.total_mass = delta.current.total_mass;
    signature.residual = delta.residual;
    signature.conserved = delta.conserved;
    signature.pending_empty = signature.pending_events.empty();
    return signature;
}

void expect_same_signature(
    const ReplayRoundSignature& actual,
    const ReplayRoundSignature& expected) {
    ASSERT_EQ(actual.cell_volumes.size(), expected.cell_volumes.size());
    for (std::size_t i = 0; i < actual.cell_volumes.size(); ++i) {
        EXPECT_DOUBLE_EQ(actual.cell_volumes[i], expected.cell_volumes[i]);
        EXPECT_DOUBLE_EQ(actual.aggregate_deficits[i], expected.aggregate_deficits[i]);
    }

    ASSERT_EQ(actual.shared_deficits.size(), expected.shared_deficits.size());
    for (std::size_t i = 0; i < actual.shared_deficits.size(); ++i) {
        EXPECT_EQ(actual.shared_deficits[i].cell_index, expected.shared_deficits[i].cell_index);
        EXPECT_EQ(actual.shared_deficits[i].engine, expected.shared_deficits[i].engine);
        EXPECT_EQ(actual.shared_deficits[i].node_id, expected.shared_deficits[i].node_id);
        EXPECT_DOUBLE_EQ(actual.shared_deficits[i].volume, expected.shared_deficits[i].volume);
    }

    ASSERT_EQ(actual.pending_events.size(), expected.pending_events.size());
    for (std::size_t i = 0; i < actual.pending_events.size(); ++i) {
        EXPECT_EQ(actual.pending_events[i].exchange_cell_index, expected.pending_events[i].exchange_cell_index);
        EXPECT_EQ(actual.pending_events[i].direction, expected.pending_events[i].direction);
        EXPECT_DOUBLE_EQ(actual.pending_events[i].volume_delta, expected.pending_events[i].volume_delta);
        EXPECT_DOUBLE_EQ(actual.pending_events[i].unmet_volume, expected.pending_events[i].unmet_volume);
        EXPECT_DOUBLE_EQ(actual.pending_events[i].repayment_volume, expected.pending_events[i].repayment_volume);
        ASSERT_EQ(actual.pending_events[i].shared_endpoint_events.size(), expected.pending_events[i].shared_endpoint_events.size());
        for (std::size_t j = 0; j < actual.pending_events[i].shared_endpoint_events.size(); ++j) {
            EXPECT_EQ(
                actual.pending_events[i].shared_endpoint_events[j].endpoint.engine,
                expected.pending_events[i].shared_endpoint_events[j].endpoint.engine);
            EXPECT_EQ(
                actual.pending_events[i].shared_endpoint_events[j].endpoint.node_id,
                expected.pending_events[i].shared_endpoint_events[j].endpoint.node_id);
            EXPECT_DOUBLE_EQ(
                actual.pending_events[i].shared_endpoint_events[j].unmet_volume,
                expected.pending_events[i].shared_endpoint_events[j].unmet_volume);
            EXPECT_DOUBLE_EQ(
                actual.pending_events[i].shared_endpoint_events[j].repayment_volume,
                expected.pending_events[i].shared_endpoint_events[j].repayment_volume);
        }
    }

    EXPECT_EQ(actual.drain_split_count, expected.drain_split_count);
    EXPECT_EQ(actual.negative_depth_fix_count, expected.negative_depth_fix_count);
    EXPECT_NEAR(actual.surface_mass, expected.surface_mass, 1.0e-9);
    EXPECT_NEAR(actual.deficit_mass, expected.deficit_mass, 1.0e-9);
    EXPECT_NEAR(actual.total_mass, expected.total_mass, 1.0e-9);
    EXPECT_NEAR(actual.residual, expected.residual, 1.0e-9);
    EXPECT_EQ(actual.conserved, expected.conserved);
    EXPECT_EQ(actual.pending_empty, expected.pending_empty);
}

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

    const auto fresh_signature = capture_round_signature(fresh, baseline, kHWet);
    const auto replayed_signature = capture_round_signature(replayed, baseline, kHWet);
    expect_same_signature(replayed_signature, fresh_signature);

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

    enqueue_shared_sequence(fresh);
    const auto baseline = fresh.compute_system_mass(kHWet);
    fresh.replay_pending();

    enqueue_shared_sequence(replayed);
    const auto replay_snapshot = replayed.snapshot();
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
    const auto restored_snapshot = replayed.snapshot();
    ASSERT_EQ(restored_snapshot.pending_events().size(), 1U);
    EXPECT_DOUBLE_EQ(restored_snapshot.pending_events()[0].volume_delta, 2.0);
    EXPECT_DOUBLE_EQ(restored_snapshot.pending_events()[0].unmet_volume, 2.0);
    EXPECT_DOUBLE_EQ(restored_snapshot.pending_events()[0].repayment_volume, 1.0);
    ASSERT_EQ(restored_snapshot.pending_events()[0].shared_endpoint_events.size(), 2U);
    EXPECT_EQ(restored_snapshot.pending_events()[0].shared_endpoint_events[0].endpoint.engine, scau::coupling::core::SharedExchangeEngine::drainage);
    EXPECT_EQ(restored_snapshot.pending_events()[0].shared_endpoint_events[0].endpoint.node_id, 10U);
    EXPECT_DOUBLE_EQ(restored_snapshot.pending_events()[0].shared_endpoint_events[0].unmet_volume, 1.5);
    EXPECT_DOUBLE_EQ(restored_snapshot.pending_events()[0].shared_endpoint_events[0].repayment_volume, 0.0);
    EXPECT_EQ(restored_snapshot.pending_events()[0].shared_endpoint_events[1].endpoint.engine, scau::coupling::core::SharedExchangeEngine::river);
    EXPECT_EQ(restored_snapshot.pending_events()[0].shared_endpoint_events[1].endpoint.node_id, 20U);
    EXPECT_DOUBLE_EQ(restored_snapshot.pending_events()[0].shared_endpoint_events[1].unmet_volume, 0.5);
    EXPECT_DOUBLE_EQ(restored_snapshot.pending_events()[0].shared_endpoint_events[1].repayment_volume, 1.0);
    replayed.replay_pending();

    const auto fresh_signature = capture_round_signature(fresh, baseline, kHWet);
    const auto replayed_signature = capture_round_signature(replayed, baseline, kHWet);
    expect_same_signature(replayed_signature, fresh_signature);

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

TEST(SnapshotReplayMassDeficitGolden, SharedReplayDriftRemainsZeroAcrossRounds) {
    auto state = make_shared_state();
    enqueue_shared_sequence(state);

    const auto baseline = state.compute_system_mass(kHWet);
    const auto snapshot_0 = state.snapshot();

    state.replay_pending();
    const auto round_1 = capture_round_signature(state, baseline, kHWet);
    EXPECT_EQ(round_1.cell_volumes.size(), 1U);
    EXPECT_DOUBLE_EQ(round_1.cell_volumes[0], 2.0);
    EXPECT_DOUBLE_EQ(round_1.aggregate_deficits[0], 0.0);
    EXPECT_EQ(round_1.shared_deficits.size(), 2U);
    EXPECT_EQ(round_1.shared_deficits[0].cell_index, 0U);
    EXPECT_EQ(round_1.shared_deficits[0].engine, scau::coupling::core::SharedExchangeEngine::drainage);
    EXPECT_EQ(round_1.shared_deficits[0].node_id, 10U);
    EXPECT_DOUBLE_EQ(round_1.shared_deficits[0].volume, 5.5);
    EXPECT_EQ(round_1.shared_deficits[1].cell_index, 0U);
    EXPECT_EQ(round_1.shared_deficits[1].engine, scau::coupling::core::SharedExchangeEngine::river);
    EXPECT_EQ(round_1.shared_deficits[1].node_id, 20U);
    EXPECT_DOUBLE_EQ(round_1.shared_deficits[1].volume, 1.0);
    EXPECT_EQ(round_1.pending_events.size(), 0U);
    EXPECT_EQ(round_1.drain_split_count, 0U);
    EXPECT_EQ(round_1.negative_depth_fix_count, 0U);
    EXPECT_DOUBLE_EQ(round_1.surface_mass, 2.0);
    EXPECT_DOUBLE_EQ(round_1.deficit_mass, 6.5);
    EXPECT_DOUBLE_EQ(round_1.total_mass, 8.5);
    EXPECT_DOUBLE_EQ(round_1.residual, 3.0);
    EXPECT_FALSE(round_1.conserved);
    EXPECT_TRUE(round_1.pending_empty);

    auto oracle = round_1;
    for (int round = 2; round <= 5; ++round) {
        state.rollback(snapshot_0);
        state.replay_pending();
        const auto round_signature = capture_round_signature(state, baseline, kHWet);
        expect_same_signature(round_signature, oracle);
    }

    const auto final_signature = capture_round_signature(state, baseline, kHWet);
    expect_same_signature(final_signature, oracle);
}

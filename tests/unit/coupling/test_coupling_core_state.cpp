#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "coupling/core/state.hpp"

TEST(CouplingCoreState, SnapshotCapturesCurrentCellsAndRemainsImmutable) {
    scau::coupling::core::CouplingState state{{
        {.volume = 10.0},
        {.volume = 20.0},
    }};

    const auto snapshot = state.snapshot();
    state.enqueue_event({.exchange_cell_index = 0U, .volume_delta = 5.0});
    state.replay_pending();

    ASSERT_EQ(snapshot.cells().size(), 2U);
    EXPECT_DOUBLE_EQ(snapshot.cells()[0].volume, 10.0);
    EXPECT_DOUBLE_EQ(snapshot.cells()[1].volume, 20.0);
    EXPECT_DOUBLE_EQ(state.cells()[0].volume, 15.0);
}

TEST(CouplingCoreState, RollbackRestoresSnapshotVolumes) {
    scau::coupling::core::CouplingState state{{
        {.volume = 10.0},
        {.volume = 20.0},
    }};

    const auto snapshot = state.snapshot();
    state.enqueue_event({.exchange_cell_index = 0U, .volume_delta = -3.0});
    state.enqueue_event({.exchange_cell_index = 1U, .volume_delta = 7.0});
    state.replay_pending();
    state.rollback(snapshot);

    ASSERT_EQ(state.cells().size(), 2U);
    EXPECT_DOUBLE_EQ(state.cells()[0].volume, 10.0);
    EXPECT_DOUBLE_EQ(state.cells()[1].volume, 20.0);
}

TEST(CouplingCoreState, RollbackPreservesPendingEventsForReplay) {
    scau::coupling::core::CouplingState state{{
        {.volume = 10.0},
        {.volume = 20.0},
    }};

    const auto snapshot = state.snapshot();
    state.enqueue_event({.exchange_cell_index = 0U, .volume_delta = 2.0});
    state.enqueue_event({.exchange_cell_index = 0U, .volume_delta = 3.0});
    state.enqueue_event({.exchange_cell_index = 1U, .volume_delta = -4.0});
    state.rollback(snapshot);
    state.replay_pending();

    ASSERT_EQ(state.cells().size(), 2U);
    EXPECT_DOUBLE_EQ(state.cells()[0].volume, 15.0);
    EXPECT_DOUBLE_EQ(state.cells()[1].volume, 16.0);
}

TEST(CouplingCoreState, EnqueueRejectsOutOfRangeExchangeCellIndex) {
    scau::coupling::core::CouplingState state{{
        {.volume = 10.0},
    }};

    EXPECT_THROW(
        state.enqueue_event({.exchange_cell_index = 1U, .volume_delta = 1.0}),
        std::out_of_range);
}

TEST(CouplingCoreState, SnapshotCapturesDeficitAccountsAndRemainsImmutable) {
    scau::coupling::core::CouplingState state{{
        {.volume = 10.0, .mass_deficit_account = {.volume = 1.0}},
        {.volume = 20.0, .mass_deficit_account = {.volume = 2.0}},
    }};

    const auto snapshot = state.snapshot();
    state.enqueue_event({.exchange_cell_index = 0U, .volume_delta = 5.0, .unmet_volume = 3.0});
    state.replay_pending();

    ASSERT_EQ(snapshot.cells().size(), 2U);
    EXPECT_DOUBLE_EQ(snapshot.cells()[0].mass_deficit_account.volume, 1.0);
    EXPECT_DOUBLE_EQ(snapshot.cells()[1].mass_deficit_account.volume, 2.0);
    EXPECT_DOUBLE_EQ(state.cells()[0].mass_deficit_account.volume, 4.0);
}

TEST(CouplingCoreState, RollbackRestoresSnapshotDeficitAccounts) {
    scau::coupling::core::CouplingState state{{
        {.volume = 10.0, .mass_deficit_account = {.volume = 1.0}},
        {.volume = 20.0, .mass_deficit_account = {.volume = 2.0}},
    }};

    const auto snapshot = state.snapshot();
    state.enqueue_event({.exchange_cell_index = 0U, .volume_delta = -3.0, .unmet_volume = 4.0});
    state.enqueue_event({.exchange_cell_index = 1U, .volume_delta = 7.0, .repayment_volume = 1.5});
    state.replay_pending();
    state.rollback(snapshot);

    ASSERT_EQ(state.cells().size(), 2U);
    EXPECT_DOUBLE_EQ(state.cells()[0].mass_deficit_account.volume, 1.0);
    EXPECT_DOUBLE_EQ(state.cells()[1].mass_deficit_account.volume, 2.0);
}

TEST(CouplingCoreState, RollbackPreservesPendingDeficitEventsForReplay) {
    scau::coupling::core::CouplingState state{{
        {.volume = 10.0, .mass_deficit_account = {.volume = 1.0}},
        {.volume = 20.0, .mass_deficit_account = {.volume = 5.0}},
    }};

    const auto snapshot = state.snapshot();
    state.enqueue_event({.exchange_cell_index = 0U, .volume_delta = 2.0, .unmet_volume = 3.0});
    state.enqueue_event({.exchange_cell_index = 1U, .volume_delta = -4.0, .repayment_volume = 2.5});
    state.rollback(snapshot);
    state.replay_pending();

    ASSERT_EQ(state.cells().size(), 2U);
    EXPECT_DOUBLE_EQ(state.cells()[0].volume, 12.0);
    EXPECT_DOUBLE_EQ(state.cells()[0].mass_deficit_account.volume, 4.0);
    EXPECT_DOUBLE_EQ(state.cells()[1].volume, 16.0);
    EXPECT_DOUBLE_EQ(state.cells()[1].mass_deficit_account.volume, 2.5);
}

TEST(CouplingCoreState, RuntimeCountersStartAtZero) {
    scau::coupling::core::CouplingState state{{{.volume = 10.0}}};

    EXPECT_EQ(state.runtime_counters().count_drain_split, 0U);
    EXPECT_EQ(state.runtime_counters().count_negative_depth_fix, 0U);
}

TEST(CouplingCoreState, RecordPipelineDecisionIncrementsDrainSplitCounter) {
    scau::coupling::core::CouplingState state{{{.volume = 10.0}}};

    state.record_pipeline_decision({
        .drain_split_engaged = true,
        .negative_depth_fix_engaged = false,
    });

    EXPECT_EQ(state.runtime_counters().count_drain_split, 1U);
    EXPECT_EQ(state.runtime_counters().count_negative_depth_fix, 0U);
}

TEST(CouplingCoreState, RecordPipelineDecisionIncrementsNegativeDepthFixCounter) {
    scau::coupling::core::CouplingState state{{{.volume = 10.0}}};

    state.record_pipeline_decision({
        .drain_split_engaged = false,
        .negative_depth_fix_engaged = true,
    });

    EXPECT_EQ(state.runtime_counters().count_drain_split, 0U);
    EXPECT_EQ(state.runtime_counters().count_negative_depth_fix, 1U);
}

TEST(CouplingCoreState, RecordPipelineDecisionIncrementsBothCountersWhenBothFlagsEngaged) {
    scau::coupling::core::CouplingState state{{{.volume = 10.0}}};

    state.record_pipeline_decision({
        .drain_split_engaged = true,
        .negative_depth_fix_engaged = true,
    });
    state.record_pipeline_decision({
        .drain_split_engaged = true,
        .negative_depth_fix_engaged = true,
    });

    EXPECT_EQ(state.runtime_counters().count_drain_split, 2U);
    EXPECT_EQ(state.runtime_counters().count_negative_depth_fix, 2U);
}

TEST(CouplingCoreState, RecordPipelineDecisionLeavesCountersUnchangedWhenNoFlagsEngaged) {
    scau::coupling::core::CouplingState state{{{.volume = 10.0}}};

    state.record_pipeline_decision({
        .drain_split_engaged = false,
        .negative_depth_fix_engaged = false,
    });

    EXPECT_EQ(state.runtime_counters().count_drain_split, 0U);
    EXPECT_EQ(state.runtime_counters().count_negative_depth_fix, 0U);
}

TEST(CouplingCoreState, SnapshotCapturesRuntimeCountersAndRemainsImmutable) {
    scau::coupling::core::CouplingState state{{{.volume = 10.0}}};
    state.record_pipeline_decision({.drain_split_engaged = true});
    state.record_pipeline_decision({.negative_depth_fix_engaged = true});

    const auto snapshot = state.snapshot();
    state.record_pipeline_decision({
        .drain_split_engaged = true,
        .negative_depth_fix_engaged = true,
    });

    EXPECT_EQ(snapshot.runtime_counters().count_drain_split, 1U);
    EXPECT_EQ(snapshot.runtime_counters().count_negative_depth_fix, 1U);
    EXPECT_EQ(state.runtime_counters().count_drain_split, 2U);
    EXPECT_EQ(state.runtime_counters().count_negative_depth_fix, 2U);
}

TEST(CouplingCoreState, RollbackRestoresSnapshotRuntimeCounters) {
    scau::coupling::core::CouplingState state{{{.volume = 10.0}}};
    state.record_pipeline_decision({.drain_split_engaged = true});

    const auto snapshot = state.snapshot();
    state.record_pipeline_decision({
        .drain_split_engaged = true,
        .negative_depth_fix_engaged = true,
    });
    state.rollback(snapshot);

    EXPECT_EQ(state.runtime_counters().count_drain_split, 1U);
    EXPECT_EQ(state.runtime_counters().count_negative_depth_fix, 0U);
}

TEST(CouplingCoreState, ReplayPendingDoesNotMutateRuntimeCounters) {
    scau::coupling::core::CouplingState state{{{.volume = 10.0}}};
    state.record_pipeline_decision({.drain_split_engaged = true});

    state.enqueue_event({.exchange_cell_index = 0U, .volume_delta = 2.0, .unmet_volume = 1.0});
    state.replay_pending();

    EXPECT_EQ(state.runtime_counters().count_drain_split, 1U);
    EXPECT_EQ(state.runtime_counters().count_negative_depth_fix, 0U);
    EXPECT_DOUBLE_EQ(state.cells()[0].volume, 12.0);
    EXPECT_DOUBLE_EQ(state.cells()[0].mass_deficit_account.volume, 1.0);
}

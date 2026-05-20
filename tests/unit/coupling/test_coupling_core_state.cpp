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

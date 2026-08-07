#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "coupling/core/state.hpp"

namespace {

using scau::coupling::core::CouplingState;
using scau::coupling::core::DeficitWriteoffConfig;
using scau::coupling::core::ExchangeCellState;
using scau::coupling::core::SharedExchangeEndpointDeficit;
using scau::coupling::core::SharedExchangeEngine;

CouplingState aggregate_state(double volume, std::size_t age = 0U) {
    ExchangeCellState cell{};
    cell.volume = 10.0;
    cell.phi_t = 1.0;
    cell.h = 1.0;
    cell.area = 10.0;
    cell.mass_deficit_account.volume = volume;
    cell.mass_deficit_account.age_steps = age;
    return CouplingState{{cell}};
}

CouplingState shared_state(double drainage_volume, double river_volume) {
    ExchangeCellState cell{};
    cell.volume = 10.0;
    cell.phi_t = 1.0;
    cell.h = 1.0;
    cell.area = 10.0;
    SharedExchangeEndpointDeficit drainage{};
    drainage.endpoint.engine = SharedExchangeEngine::drainage;
    drainage.endpoint.node_id = 11U;
    drainage.mass_deficit_account.volume = drainage_volume;
    SharedExchangeEndpointDeficit river{};
    river.endpoint.engine = SharedExchangeEngine::river;
    river.endpoint.node_id = 5U;
    river.mass_deficit_account.volume = river_volume;
    cell.shared_deficit_accounts = {drainage, river};
    return CouplingState{{cell}};
}

}  // namespace

TEST(DeficitWriteoff, WritesOffOnThirdConsecutiveEpoch) {
    auto state = aggregate_state(4.5);
    const auto first = state.apply_deficit_writeoff();
    EXPECT_TRUE(first.records.empty());
    EXPECT_EQ(state.cells()[0].mass_deficit_account.age_steps, 1U);
    EXPECT_DOUBLE_EQ(state.cells()[0].mass_deficit_account.volume, 4.5);

    const auto second = state.apply_deficit_writeoff();
    EXPECT_TRUE(second.records.empty());
    EXPECT_EQ(state.cells()[0].mass_deficit_account.age_steps, 2U);

    const auto third = state.apply_deficit_writeoff();
    ASSERT_EQ(third.records.size(), 1U);
    EXPECT_EQ(third.records[0].cell_index, 0U);
    EXPECT_FALSE(third.records[0].has_shared_endpoint);
    EXPECT_DOUBLE_EQ(third.records[0].volume_written_off, 4.5);
    EXPECT_EQ(third.records[0].age_steps_before_writeoff, 3U);
    EXPECT_DOUBLE_EQ(third.volume_written_off_total, 4.5);
    EXPECT_EQ(third.event_count, 1U);
    EXPECT_DOUBLE_EQ(state.cells()[0].mass_deficit_account.volume, 0.0);
    EXPECT_EQ(state.cells()[0].mass_deficit_account.age_steps, 0U);
    EXPECT_EQ(state.runtime_counters().count_writeoff_events, 1U);
    EXPECT_DOUBLE_EQ(state.runtime_counters().count_writeoff_volume_total, 4.5);
}

TEST(DeficitWriteoff, ZeroOrFullyRepaidAccountResetsAge) {
    auto state = aggregate_state(2.0, 2U);
    // A repayment event clears the account before epoch-end aging.
    scau::coupling::core::CouplingEvent repayment{};
    repayment.exchange_cell_index = 0U;
    repayment.volume_delta = 2.0;
    repayment.repayment_volume = 2.0;
    state.enqueue_event(repayment);
    state.replay_pending();
    ASSERT_DOUBLE_EQ(state.cells()[0].mass_deficit_account.volume, 0.0);
    ASSERT_EQ(state.cells()[0].mass_deficit_account.age_steps, 0U);

    const auto report = state.apply_deficit_writeoff();
    EXPECT_TRUE(report.records.empty());
    EXPECT_EQ(state.cells()[0].mass_deficit_account.age_steps, 0U);
}

TEST(DeficitWriteoff, SharedEndpointsAgeAndWriteOffIndependently) {
    auto state = shared_state(3.0, 0.0);
    state.apply_deficit_writeoff();
    state.apply_deficit_writeoff();

    // Add a river deficit after the drainage account has already aged twice.
    auto snapshot = state.snapshot();
    auto cells = snapshot.cells();
    cells[0].shared_deficit_accounts[1].mass_deficit_account.volume = 7.0;
    CouplingState staggered{cells};

    const auto third = staggered.apply_deficit_writeoff();
    ASSERT_EQ(third.records.size(), 1U);
    EXPECT_TRUE(third.records[0].has_shared_endpoint);
    EXPECT_EQ(third.records[0].endpoint.engine, SharedExchangeEngine::drainage);
    EXPECT_EQ(third.records[0].endpoint.node_id, 11U);
    EXPECT_DOUBLE_EQ(third.records[0].volume_written_off, 3.0);
    EXPECT_EQ(staggered.cells()[0].shared_deficit_accounts[1]
                  .mass_deficit_account.age_steps,
              1U);
    EXPECT_DOUBLE_EQ(staggered.cells()[0].shared_deficit_accounts[1]
                         .mass_deficit_account.volume,
                     7.0);
}

TEST(DeficitWriteoff, SnapshotRollbackReplaysBitExactly) {
    auto state = shared_state(3.0, 5.0);
    state.apply_deficit_writeoff();
    state.apply_deficit_writeoff();
    const auto before = state.snapshot();

    const auto first = state.apply_deficit_writeoff();
    const auto counters_first = state.runtime_counters();
    const auto cells_first = state.cells();

    state.rollback(before);
    const auto replayed = state.apply_deficit_writeoff();
    ASSERT_EQ(replayed.records.size(), first.records.size());
    ASSERT_EQ(replayed.records.size(), 2U);
    for (std::size_t index = 0U; index < first.records.size(); ++index) {
        EXPECT_EQ(replayed.records[index].cell_index, first.records[index].cell_index);
        EXPECT_EQ(replayed.records[index].endpoint.engine, first.records[index].endpoint.engine);
        EXPECT_EQ(replayed.records[index].endpoint.node_id, first.records[index].endpoint.node_id);
        EXPECT_DOUBLE_EQ(replayed.records[index].volume_written_off,
                         first.records[index].volume_written_off);
        EXPECT_EQ(replayed.records[index].age_steps_before_writeoff,
                  first.records[index].age_steps_before_writeoff);
    }
    EXPECT_DOUBLE_EQ(replayed.volume_written_off_total,
                     first.volume_written_off_total);
    EXPECT_EQ(state.runtime_counters().count_writeoff_events,
              counters_first.count_writeoff_events);
    EXPECT_DOUBLE_EQ(state.runtime_counters().count_writeoff_volume_total,
                     counters_first.count_writeoff_volume_total);
    ASSERT_EQ(state.cells().size(), cells_first.size());
    EXPECT_DOUBLE_EQ(state.cells()[0].shared_deficit_accounts[0]
                         .mass_deficit_account.volume,
                     cells_first[0].shared_deficit_accounts[0]
                         .mass_deficit_account.volume);
}

TEST(DeficitWriteoff, RejectsInvalidThresholdPendingEventsAndInvalidZeroAge) {
    auto state = aggregate_state(1.0);
    DeficitWriteoffConfig invalid{};
    invalid.writeoff_threshold_steps = 0U;
    EXPECT_THROW(static_cast<void>(state.apply_deficit_writeoff(invalid)),
                 std::invalid_argument);

    scau::coupling::core::CouplingEvent event{};
    event.exchange_cell_index = 0U;
    event.volume_delta = 1.0;
    state.enqueue_event(event);
    EXPECT_THROW(static_cast<void>(state.apply_deficit_writeoff()),
                 std::logic_error);

    ExchangeCellState bad{};
    bad.volume = 1.0;
    bad.phi_t = 1.0;
    bad.h = 1.0;
    bad.area = 1.0;
    bad.mass_deficit_account.age_steps = 1U;
    EXPECT_THROW(static_cast<void>(CouplingState{{bad}}), std::invalid_argument);
}

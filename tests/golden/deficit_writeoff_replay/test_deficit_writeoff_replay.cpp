#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "coupling/core/state.hpp"
#include "coupling/driver/checkpoint_payloads.hpp"

// G25 deficit_writeoff_replay: authoritative deterministic evidence for
// N_writeoff_steps=3, aggregate/shared endpoint independence, explicit
// count_writeoff_volume_total, and snapshot rollback/replay identity.

namespace {

using scau::coupling::core::CouplingState;
using scau::coupling::core::ExchangeCellState;
using scau::coupling::core::SharedExchangeEndpointDeficit;
using scau::coupling::core::SharedExchangeEngine;
using scau::coupling::driver::hash_coupling_snapshot;

CouplingState make_state() {
    ExchangeCellState aggregate{};
    aggregate.volume = 10.0;
    aggregate.phi_t = 1.0;
    aggregate.h = 1.0;
    aggregate.area = 10.0;
    aggregate.mass_deficit_account.volume = 4.0;

    ExchangeCellState shared{};
    shared.volume = 20.0;
    shared.phi_t = 0.5;
    shared.h = 2.0;
    shared.area = 20.0;
    SharedExchangeEndpointDeficit drainage{};
    drainage.endpoint.engine = SharedExchangeEngine::drainage;
    drainage.endpoint.node_id = 11U;
    drainage.mass_deficit_account.volume = 3.0;
    SharedExchangeEndpointDeficit river{};
    river.endpoint.engine = SharedExchangeEngine::river;
    river.endpoint.node_id = 5U;
    river.mass_deficit_account.volume = 7.0;
    shared.shared_deficit_accounts = {drainage, river};

    return CouplingState{{aggregate, shared}};
}

void expect_same_report(
    const scau::coupling::core::DeficitWriteoffReport& actual,
    const scau::coupling::core::DeficitWriteoffReport& expected) {
    ASSERT_EQ(actual.records.size(), expected.records.size());
    EXPECT_EQ(actual.event_count, expected.event_count);
    EXPECT_DOUBLE_EQ(actual.volume_written_off_total,
                     expected.volume_written_off_total);
    for (std::size_t index = 0U; index < expected.records.size(); ++index) {
        EXPECT_EQ(actual.records[index].cell_index,
                  expected.records[index].cell_index);
        EXPECT_EQ(actual.records[index].has_shared_endpoint,
                  expected.records[index].has_shared_endpoint);
        EXPECT_EQ(actual.records[index].endpoint.engine,
                  expected.records[index].endpoint.engine);
        EXPECT_EQ(actual.records[index].endpoint.node_id,
                  expected.records[index].endpoint.node_id);
        EXPECT_DOUBLE_EQ(actual.records[index].volume_written_off,
                         expected.records[index].volume_written_off);
        EXPECT_EQ(actual.records[index].age_steps_before_writeoff,
                  expected.records[index].age_steps_before_writeoff);
    }
}

}  // namespace

TEST(GoldenDeficitWriteoffReplay, ThirdEpochWriteoffIsExplicitAndReplayExact) {
    auto state = make_state();
    const auto initial_surface = state.compute_system_mass(1.0e-6).surface_mass;

    EXPECT_TRUE(state.apply_deficit_writeoff().records.empty());
    EXPECT_TRUE(state.apply_deficit_writeoff().records.empty());
    ASSERT_EQ(state.cells()[0].mass_deficit_account.age_steps, 2U);
    ASSERT_EQ(state.cells()[1].shared_deficit_accounts[0]
                  .mass_deficit_account.age_steps,
              2U);

    const auto pre_writeoff = state.snapshot();
    const std::string pre_hash = hash_coupling_snapshot(pre_writeoff);
    const auto report_first = state.apply_deficit_writeoff();
    const auto post_first = state.snapshot();
    const std::string post_hash = hash_coupling_snapshot(post_first);

    ASSERT_EQ(report_first.records.size(), 3U);
    EXPECT_DOUBLE_EQ(report_first.volume_written_off_total, 14.0);
    EXPECT_EQ(report_first.event_count, 3U);
    EXPECT_FALSE(report_first.records[0].has_shared_endpoint);
    EXPECT_EQ(report_first.records[1].endpoint.engine,
              SharedExchangeEngine::drainage);
    EXPECT_EQ(report_first.records[1].endpoint.node_id, 11U);
    EXPECT_EQ(report_first.records[2].endpoint.engine,
              SharedExchangeEngine::river);
    EXPECT_EQ(report_first.records[2].endpoint.node_id, 5U);
    EXPECT_EQ(state.runtime_counters().count_writeoff_events, 3U);
    EXPECT_DOUBLE_EQ(state.runtime_counters().count_writeoff_volume_total,
                     14.0);
    EXPECT_NE(post_hash, pre_hash);

    // Write-off is obligation-only: physical surface storage is unchanged.
    EXPECT_DOUBLE_EQ(state.compute_system_mass(1.0e-6).surface_mass,
                     initial_surface);
    EXPECT_DOUBLE_EQ(state.compute_system_mass(1.0e-6).deficit_mass, 0.0);

    // Rollback and replay the same epoch-end action; report and committed
    // snapshot hash are strictly identical.
    state.rollback(pre_writeoff);
    EXPECT_EQ(hash_coupling_snapshot(state.snapshot()), pre_hash);
    const auto report_replayed = state.apply_deficit_writeoff();
    expect_same_report(report_replayed, report_first);
    EXPECT_EQ(hash_coupling_snapshot(state.snapshot()), post_hash);
}

#include <optional>

#include <gtest/gtest.h>

#include "coupling/driver/coupling_state_endpoint_provider.hpp"
#include "coupling/driver/roof_exchange_gate.hpp"
#include "coupling/drainage/mock_swmm_engine.hpp"
#include "coupling/drainage/roof_drainage_adapter.hpp"

namespace {

using scau::coupling::core::CouplingEvent;
using scau::coupling::core::CouplingState;
using scau::coupling::core::ExchangeCellState;
using scau::coupling::core::MassDeficitAccount;
using scau::coupling::drainage::MockSwmmEngine;
using scau::coupling::drainage::SwmmRoofDrainageAcceptanceAdapter;
using scau::coupling::driver::CouplingStateEndpointProvider;
using scau::coupling::driver::RoofEndpointMap;
using scau::coupling::driver::RoofExchangeGate;
using scau::surface2d::RoofDrainageIntent;
using scau::surface2d::RoofDrainageRejectionReason;

constexpr double kDtSub = 10.0;

RoofDrainageIntent make_intent(int source_cell, double volume) {
    return RoofDrainageIntent{
        .source_cell_index = source_cell,
        .target_swmm_node_index = 0,
        .requested_volume = volume,
        .source_roof_area = 50.0,
    };
}

// Cell 0: Q_limit = 0.9*1.0*1.0*100/10 = 9 m3/s.
// Cell 1: dry -> Q_limit = 0.
CouplingState make_state() {
    return CouplingState{{
        ExchangeCellState{
            .volume = 100.0,
            .mass_deficit_account = MassDeficitAccount{},
            .phi_t = 1.0,
            .h = 1.0,
            .area = 100.0,
        },
        ExchangeCellState{
            .volume = 0.0,
            .mass_deficit_account = MassDeficitAccount{},
            .phi_t = 1.0,
            .h = 0.0,
            .area = 100.0,
        },
    }};
}

}  // namespace

TEST(CouplingStateEndpointProvider, ResolvesMappedLiveCell) {
    const auto state = make_state();
    const RoofEndpointMap map{{7, 0U}};
    const CouplingStateEndpointProvider provider(state, map);

    const auto endpoint = provider(make_intent(7, 30.0));

    ASSERT_TRUE(endpoint.has_value());
    EXPECT_NEAR(endpoint->h, 1.0, 1.0e-12);
    EXPECT_NEAR(endpoint->area, 100.0, 1.0e-12);
}

TEST(CouplingStateEndpointProvider, UnmappedSourceCellYieldsNullopt) {
    const auto state = make_state();
    const CouplingStateEndpointProvider provider(state, RoofEndpointMap{{7, 0U}});

    EXPECT_FALSE(provider(make_intent(8, 30.0)).has_value());
}

TEST(CouplingStateEndpointProvider, OutOfRangeExchangeCellYieldsNullopt) {
    const auto state = make_state();
    const CouplingStateEndpointProvider provider(state, RoofEndpointMap{{7, 99U}});

    EXPECT_FALSE(provider(make_intent(7, 30.0)).has_value());
}

TEST(CouplingStateEndpointProvider, SeesLiveStateMutationsAfterReplay) {
    auto state = make_state();
    const CouplingStateEndpointProvider provider(state, RoofEndpointMap{{7, 0U}});

    // Deficit ledger mutation through the canonical enqueue -> replay path
    // must be visible to arbitration without rebuilding the provider.
    state.enqueue_event(CouplingEvent{
        .exchange_cell_index = 0U,
        .volume_delta = 0.0,
        .unmet_volume = 20.0,
        .repayment_volume = 0.0,
    });
    state.replay_pending();

    const auto endpoint = provider(make_intent(7, 30.0));
    ASSERT_TRUE(endpoint.has_value());
    EXPECT_NEAR(endpoint->mass_deficit_account.volume, 20.0, 1.0e-12);
}

TEST(CouplingStateEndpointProvider, GateArbitratesAgainstLiveDeficit) {
    auto state = make_state();
    MockSwmmEngine engine(1U);
    SwmmRoofDrainageAcceptanceAdapter adapter(engine, kDtSub);
    RoofExchangeGate gate(
        CouplingStateEndpointProvider(state, RoofEndpointMap{{7, 0U}, {8, 1U}}),
        [&adapter](const RoofDrainageIntent& intent) { return adapter(intent); },
        kDtSub);

    // 20 m3 outstanding deficit reserves 2 m3/s of the 9 m3/s Q_limit.
    state.enqueue_event(CouplingEvent{
        .exchange_cell_index = 0U,
        .volume_delta = 0.0,
        .unmet_volume = 20.0,
        .repayment_volume = 0.0,
    });
    state.replay_pending();

    const auto clamped = gate(make_intent(7, 120.0));
    EXPECT_EQ(clamped.rejection_reason, RoofDrainageRejectionReason::CapacityLimited);
    EXPECT_NEAR(clamped.accepted_volume, 70.0, 1.0e-12);
    EXPECT_NEAR(engine.get_node_lateral_inflow(0U), 7.0, 1.0e-12);

    // Dry mapped cell: zero headroom.
    gate.begin_substep();
    adapter.begin_step();
    const auto dry = gate(make_intent(8, 10.0));
    EXPECT_EQ(dry.rejection_reason, RoofDrainageRejectionReason::CapacityLimited);
    EXPECT_NEAR(dry.accepted_volume, 0.0, 1.0e-12);

    // Unmapped roof cell: arbitration cannot vouch -> HealthBlocked.
    const auto unmapped = gate(make_intent(9, 10.0));
    EXPECT_EQ(unmapped.rejection_reason, RoofDrainageRejectionReason::HealthBlocked);
}

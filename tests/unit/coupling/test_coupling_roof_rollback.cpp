#include <optional>
#include <string>

#include <gtest/gtest.h>

#include "coupling/driver/coupling_state_endpoint_provider.hpp"
#include "coupling/driver/roof_swmm_step_driver.hpp"
#include "coupling/drainage/swmm_boundary.hpp"
#include "coupling/drainage/roof_drainage_adapter.hpp"

namespace {

using scau::coupling::core::CouplingState;
using scau::coupling::core::ExchangeCellState;
using scau::coupling::core::MassDeficitAccount;
using scau::coupling::drainage::MockSwmmEngine;
using scau::coupling::drainage::SwmmRoofDrainageAcceptanceAdapter;
using scau::coupling::driver::CouplingStateEndpointProvider;
using scau::coupling::driver::RoofEndpointMap;
using scau::coupling::driver::RoofSwmmStepDriver;
using scau::surface2d::RoofDrainageIntent;
using scau::surface2d::RoofDrainageRejectionReason;

constexpr double kDtSub = 10.0;

RoofDrainageIntent make_intent(int node, double volume) {
    return RoofDrainageIntent{
        .source_cell_index = 0,
        .target_swmm_node_index = node,
        .requested_volume = volume,
        .source_roof_area = 50.0,
    };
}

ExchangeCellState make_endpoint() {
    return ExchangeCellState{
        .volume = 100.0,
        .mass_deficit_account = MassDeficitAccount{},
        .phi_t = 1.0,
        .h = 1.0,
        .area = 100.0,
    };
}

}  // namespace

TEST(RoofRollback, AdapterRollbackZeroesWrittenNodesAndClearsLedger) {
    MockSwmmEngine engine;
    engine.initialize("mock.inp");
    SwmmRoofDrainageAcceptanceAdapter adapter(engine, kDtSub);

    (void)adapter(make_intent(0, 20.0));
    (void)adapter(make_intent(1, 10.0));
    EXPECT_NEAR(engine.get_node_lateral_inflow(0), 2.0, 1.0e-12);
    EXPECT_NEAR(engine.get_node_lateral_inflow(1), 1.0, 1.0e-12);

    adapter.rollback_step();

    // The engine-side API buffer is zeroed; nothing survives into routing.
    EXPECT_NEAR(engine.get_node_lateral_inflow(0), 0.0, 1.0e-12);
    EXPECT_NEAR(engine.get_node_lateral_inflow(1), 0.0, 1.0e-12);

    // The per-step ledger restarted: a fresh write is not accumulated onto
    // the rolled-back flows.
    (void)adapter(make_intent(0, 20.0));
    EXPECT_NEAR(engine.get_node_lateral_inflow(0), 2.0, 1.0e-12);
}

TEST(RoofRollback, DriverRollbackDiscardsSubstepWithoutAdvancingEngine) {
    MockSwmmEngine engine;
    engine.initialize("mock.inp");
    RoofSwmmStepDriver driver(
        engine,
        [](const RoofDrainageIntent&) { return std::make_optional(make_endpoint()); },
        kDtSub);

    driver.begin_substep();
    const auto accept = driver.acceptance_fn();
    const auto acceptance = accept(make_intent(0, 30.0));
    EXPECT_EQ(acceptance.rejection_reason, RoofDrainageRejectionReason::None);
    EXPECT_NEAR(engine.get_node_lateral_inflow(0), 3.0, 1.0e-12);

    driver.rollback_substep();

    // Rolled back: engine buffer zeroed, engine time untouched.
    EXPECT_NEAR(engine.get_node_lateral_inflow(0), 0.0, 1.0e-12);
    EXPECT_NEAR(engine.elapsed_time(), 0.0, 1.0e-12);
    EXPECT_EQ(driver.substep_count(), 1U);

    // The replayed substep gets FULL Q_limit headroom again (9 m3/s): the
    // rolled-back grant no longer counts against the gate ledger.
    driver.begin_substep();
    const auto replayed = accept(make_intent(0, 90.0));
    EXPECT_EQ(replayed.rejection_reason, RoofDrainageRejectionReason::None);
    EXPECT_NEAR(engine.get_node_lateral_inflow(0), 9.0, 1.0e-12);
}

TEST(RoofRollback, LiveStateProviderAndRollbackComposeOnRealisticFlow) {
    auto state = CouplingState{{make_endpoint()}};
    MockSwmmEngine engine;
    engine.initialize("mock.inp");
    RoofSwmmStepDriver driver(
        engine,
        CouplingStateEndpointProvider(state, RoofEndpointMap{{0, 0U}}),
        kDtSub);

    driver.begin_substep();
    const auto accept = driver.acceptance_fn();

    // Over-limit request against the LIVE cell: clamped to Q_limit = 9 m3/s.
    const auto clamped = accept(make_intent(0, 120.0));
    EXPECT_EQ(clamped.rejection_reason, RoofDrainageRejectionReason::CapacityLimited);
    EXPECT_NEAR(clamped.accepted_volume, 90.0, 1.0e-12);

    driver.rollback_substep();
    EXPECT_NEAR(engine.get_node_lateral_inflow(0), 0.0, 1.0e-12);

    driver.begin_substep();
    const auto retried = accept(make_intent(0, 45.0));
    EXPECT_EQ(retried.rejection_reason, RoofDrainageRejectionReason::None);
    EXPECT_NEAR(engine.get_node_lateral_inflow(0), 4.5, 1.0e-12);
}

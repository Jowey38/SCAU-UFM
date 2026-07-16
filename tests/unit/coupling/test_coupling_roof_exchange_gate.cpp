#include <optional>

#include <gtest/gtest.h>

#include "coupling/driver/roof_exchange_gate.hpp"
#include "coupling/drainage/mock_swmm_engine.hpp"
#include "coupling/drainage/roof_drainage_adapter.hpp"

namespace {

using scau::coupling::core::ExchangeCellState;
using scau::coupling::core::MassDeficitAccount;
using scau::coupling::drainage::MockSwmmEngine;
using scau::coupling::drainage::SwmmRoofDrainageAcceptanceAdapter;
using scau::coupling::driver::RoofExchangeGate;
using scau::surface2d::RoofDrainageIntent;
using scau::surface2d::RoofDrainageRejectionReason;

constexpr double kDtSub = 10.0;

RoofDrainageIntent make_intent(double volume) {
    return RoofDrainageIntent{
        .source_cell_index = 0,
        .target_swmm_node_index = 0,
        .requested_volume = volume,
        .source_roof_area = 50.0,
    };
}

// Endpoint with V_limit = 0.9 * 1.0 * 1.0 * 100 = 90 m3 -> Q_limit = 9 m3/s.
ExchangeCellState make_endpoint(double deficit_volume = 0.0) {
    return ExchangeCellState{
        .volume = 0.0,
        .mass_deficit_account = MassDeficitAccount{.volume = deficit_volume},
        .phi_t = 1.0,
        .h = 1.0,
        .area = 100.0,
    };
}

}  // namespace

TEST(RoofExchangeGate, PassesThroughUnderHeadroom) {
    MockSwmmEngine engine(1U);
    SwmmRoofDrainageAcceptanceAdapter adapter(engine, kDtSub);
    RoofExchangeGate gate(
        [](const RoofDrainageIntent&) { return std::make_optional(make_endpoint()); },
        [&adapter](const RoofDrainageIntent& intent) { return adapter(intent); },
        kDtSub);

    // 30 m3 / 10 s = 3 m3/s < Q_limit 9 m3/s: fully granted.
    const auto acceptance = gate(make_intent(30.0));

    EXPECT_EQ(acceptance.rejection_reason, RoofDrainageRejectionReason::None);
    EXPECT_NEAR(acceptance.requested_volume, 30.0, 1.0e-12);
    EXPECT_NEAR(acceptance.accepted_volume, 30.0, 1.0e-12);
    EXPECT_NEAR(acceptance.rejected_volume, 0.0, 1.0e-12);
    EXPECT_NEAR(engine.get_node_lateral_inflow(0U), 3.0, 1.0e-12);
}

TEST(RoofExchangeGate, ClampsToQLimitAndReportsCapacityLimited) {
    MockSwmmEngine engine(1U);
    SwmmRoofDrainageAcceptanceAdapter adapter(engine, kDtSub);
    RoofExchangeGate gate(
        [](const RoofDrainageIntent&) { return std::make_optional(make_endpoint()); },
        [&adapter](const RoofDrainageIntent& intent) { return adapter(intent); },
        kDtSub);

    // 120 m3 / 10 s = 12 m3/s > Q_limit 9 m3/s: clamp to V_limit = 90 m3.
    const auto acceptance = gate(make_intent(120.0));

    EXPECT_EQ(acceptance.rejection_reason, RoofDrainageRejectionReason::CapacityLimited);
    EXPECT_NEAR(acceptance.requested_volume, 120.0, 1.0e-12);
    EXPECT_NEAR(acceptance.accepted_volume, 90.0, 1.0e-12);
    EXPECT_NEAR(acceptance.rejected_volume, 30.0, 1.0e-12);
    // Downstream sees only the granted flow.
    EXPECT_NEAR(engine.get_node_lateral_inflow(0U), 9.0, 1.0e-12);
}

TEST(RoofExchangeGate, OutstandingDeficitReservesRepaymentHeadroom) {
    MockSwmmEngine engine(1U);
    SwmmRoofDrainageAcceptanceAdapter adapter(engine, kDtSub);
    // Endpoint owes 20 m3: q_repay = min(20/10, 9) = 2 m3/s -> roof headroom 7 m3/s.
    RoofExchangeGate gate(
        [](const RoofDrainageIntent&) { return std::make_optional(make_endpoint(20.0)); },
        [&adapter](const RoofDrainageIntent& intent) { return adapter(intent); },
        kDtSub);

    const auto acceptance = gate(make_intent(120.0));

    EXPECT_EQ(acceptance.rejection_reason, RoofDrainageRejectionReason::CapacityLimited);
    EXPECT_NEAR(acceptance.accepted_volume, 70.0, 1.0e-12);
    EXPECT_NEAR(acceptance.rejected_volume, 50.0, 1.0e-12);
    EXPECT_NEAR(engine.get_node_lateral_inflow(0U), 7.0, 1.0e-12);
}

TEST(RoofExchangeGate, MissingEndpointStateFailsClosedWithoutDownstreamWrite) {
    MockSwmmEngine engine(1U);
    SwmmRoofDrainageAcceptanceAdapter adapter(engine, kDtSub);
    RoofExchangeGate gate(
        [](const RoofDrainageIntent&) { return std::optional<ExchangeCellState>{}; },
        [&adapter](const RoofDrainageIntent& intent) { return adapter(intent); },
        kDtSub);

    const auto acceptance = gate(make_intent(30.0));

    EXPECT_EQ(acceptance.rejection_reason, RoofDrainageRejectionReason::HealthBlocked);
    EXPECT_NEAR(acceptance.accepted_volume, 0.0, 1.0e-12);
    EXPECT_NEAR(acceptance.rejected_volume, 30.0, 1.0e-12);
    EXPECT_NEAR(engine.get_node_lateral_inflow(0U), 0.0, 1.0e-12);
}

TEST(RoofExchangeGate, DownstreamRejectionReasonWinsWithOriginalAccounting) {
    MockSwmmEngine engine(1U);
    engine.set_surcharged(0U, true);
    SwmmRoofDrainageAcceptanceAdapter adapter(engine, kDtSub);
    RoofExchangeGate gate(
        [](const RoofDrainageIntent&) { return std::make_optional(make_endpoint()); },
        [&adapter](const RoofDrainageIntent& intent) { return adapter(intent); },
        kDtSub);

    // Clamped to 90 m3, but the node is surcharged: everything rejected and
    // the acceptance must account for the ORIGINAL 120 m3 request.
    const auto acceptance = gate(make_intent(120.0));

    EXPECT_EQ(acceptance.rejection_reason, RoofDrainageRejectionReason::NodeSurcharged);
    EXPECT_NEAR(acceptance.requested_volume, 120.0, 1.0e-12);
    EXPECT_NEAR(acceptance.accepted_volume, 0.0, 1.0e-12);
    EXPECT_NEAR(acceptance.rejected_volume, 120.0, 1.0e-12);
    EXPECT_NEAR(engine.get_node_lateral_inflow(0U), 0.0, 1.0e-12);
}

TEST(RoofExchangeGate, ZeroHeadroomSkipsDownstream) {
    MockSwmmEngine engine(1U);
    SwmmRoofDrainageAcceptanceAdapter adapter(engine, kDtSub);
    // Dry endpoint: V_limit = 0 -> Q_limit = 0 -> nothing can be granted.
    RoofExchangeGate gate(
        [](const RoofDrainageIntent&) {
            auto endpoint = make_endpoint();
            endpoint.h = 0.0;
            return std::make_optional(endpoint);
        },
        [&adapter](const RoofDrainageIntent& intent) { return adapter(intent); },
        kDtSub);

    const auto acceptance = gate(make_intent(30.0));

    EXPECT_EQ(acceptance.rejection_reason, RoofDrainageRejectionReason::CapacityLimited);
    EXPECT_NEAR(acceptance.accepted_volume, 0.0, 1.0e-12);
    EXPECT_NEAR(acceptance.rejected_volume, 30.0, 1.0e-12);
    EXPECT_NEAR(engine.get_node_lateral_inflow(0U), 0.0, 1.0e-12);
}

TEST(RoofExchangeGate, InvalidVolumeForwardsToDownstreamFailClosed) {
    MockSwmmEngine engine(1U);
    SwmmRoofDrainageAcceptanceAdapter adapter(engine, kDtSub);
    RoofExchangeGate gate(
        [](const RoofDrainageIntent&) { return std::make_optional(make_endpoint()); },
        [&adapter](const RoofDrainageIntent& intent) { return adapter(intent); },
        kDtSub);

    const auto acceptance = gate(make_intent(-5.0));

    EXPECT_EQ(acceptance.rejection_reason, RoofDrainageRejectionReason::EngineUnavailable);
    EXPECT_NEAR(acceptance.accepted_volume, 0.0, 1.0e-12);
    EXPECT_NEAR(acceptance.rejected_volume, 5.0, 1.0e-12);
    EXPECT_NEAR(engine.get_node_lateral_inflow(0U), 0.0, 1.0e-12);
}

TEST(RoofExchangeGate, GrantedFlowAccumulatesAgainstQLimitWithinSubstep) {
    MockSwmmEngine engine(1U);
    SwmmRoofDrainageAcceptanceAdapter adapter(engine, kDtSub);
    RoofExchangeGate gate(
        [](const RoofDrainageIntent&) { return std::make_optional(make_endpoint()); },
        [&adapter](const RoofDrainageIntent& intent) { return adapter(intent); },
        kDtSub);

    // First intent grants 3 m3/s of the 9 m3/s Q_limit.
    const auto first = gate(make_intent(30.0));
    EXPECT_EQ(first.rejection_reason, RoofDrainageRejectionReason::None);

    // Second intent on the SAME endpoint may only get the remaining 6 m3/s.
    const auto second = gate(make_intent(120.0));
    EXPECT_EQ(second.rejection_reason, RoofDrainageRejectionReason::CapacityLimited);
    EXPECT_NEAR(second.accepted_volume, 60.0, 1.0e-12);
    EXPECT_NEAR(second.rejected_volume, 60.0, 1.0e-12);
    EXPECT_NEAR(engine.get_node_lateral_inflow(0U), 9.0, 1.0e-12);

    // A new substep resets the per-endpoint grant ledger.
    gate.begin_substep();
    adapter.begin_step();
    const auto third = gate(make_intent(30.0));
    EXPECT_EQ(third.rejection_reason, RoofDrainageRejectionReason::None);
    EXPECT_NEAR(engine.get_node_lateral_inflow(0U), 3.0, 1.0e-12);
}

TEST(RoofExchangeGate, RejectsInvalidDtAtConstruction) {
    EXPECT_THROW(
        RoofExchangeGate(
            [](const RoofDrainageIntent&) { return std::make_optional(make_endpoint()); },
            [](const RoofDrainageIntent&) { return scau::surface2d::RoofDrainageAcceptance{}; },
            0.0),
        std::invalid_argument);
}

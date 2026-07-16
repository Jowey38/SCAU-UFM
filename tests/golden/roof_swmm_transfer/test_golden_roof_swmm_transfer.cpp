#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "coupling/driver/coupling_state_endpoint_provider.hpp"
#include "coupling/driver/roof_swmm_step_driver.hpp"
#include "coupling/drainage/swmm_engine.hpp"

// GoldenSuite M240-ROOF: roof -> SWMM transfer conservation on the real
// embedded SWMM 5.2.4 engine.
//
// Locks the full delivered chain: RoofSwmmStepDriver (dt_sub scheduling)
// -> RoofExchangeGate (Q_limit hard gate, live CouplingState endpoint)
// -> SwmmRoofDrainageAcceptanceAdapter (engine write) -> real routing.
//
// Conservation contract:
//   (1) per intent: accepted + rejected == requested (exact)
//   (2) per substep: accepted volume <= V_limit = 0.9 * phi_t * h * A (exact)
//   (3) routed lateral inflow after each advanced substep == accepted / dt_sub
//   (4) rolled-back substep delivers nothing into routing
//   (5) totals close: sum(requested) == sum(accepted) + sum(rejected)

namespace {

using scau::coupling::core::CouplingState;
using scau::coupling::core::ExchangeCellState;
using scau::coupling::core::MassDeficitAccount;
using scau::coupling::drainage::SwmmEngine;
using scau::coupling::driver::CouplingStateEndpointProvider;
using scau::coupling::driver::RoofEndpointMap;
using scau::coupling::driver::RoofSwmmStepDriver;
using scau::surface2d::RoofDrainageIntent;
using scau::surface2d::RoofDrainageRejectionReason;

constexpr double kDtSub = 60.0;
// Endpoint: V_limit = 0.9 * 1.0 * 0.5 * 100 = 45 m3 -> Q_limit = 0.75 m3/s.
constexpr double kVLimit = 45.0;

std::string minimal_case_path() {
    return std::string(SCAU_SWMM_TEST_CASE_DIR) + "/swmm_minimal.inp";
}

CouplingState make_state() {
    return CouplingState{{
        ExchangeCellState{
            .volume = 100.0,
            .mass_deficit_account = MassDeficitAccount{},
            .phi_t = 1.0,
            .h = 0.5,
            .area = 100.0,
        },
    }};
}

}  // namespace

TEST(GoldenRoofSwmmTransfer, TransferConservationUnderRealRouting) {
    SwmmEngine engine;
    engine.initialize(minimal_case_path());

    const std::size_t j1 = engine.node_index("J1");
    const std::size_t o1 = engine.node_index("O1");

    auto state = make_state();
    RoofSwmmStepDriver driver(
        engine,
        CouplingStateEndpointProvider(state, RoofEndpointMap{{0, 0U}}),
        kDtSub);
    const auto accept = driver.acceptance_fn();

    // Deterministic schedule: under-limit, over-limit, and zero requests.
    const std::vector<double> requested_volumes{
        12.0, 30.0, 60.0, 45.0, 0.0, 90.0, 18.0, 45.0, 6.0, 75.0,
    };
    const int rollback_substep = 5;  // the 90 m3 substep is rolled back

    double total_requested = 0.0;
    double total_accepted = 0.0;
    double total_rejected = 0.0;
    double total_delivered = 0.0;  // accepted volume that actually advanced

    for (std::size_t i = 0; i < requested_volumes.size(); ++i) {
        driver.begin_substep();
        const RoofDrainageIntent intent{
            .source_cell_index = 0,
            .target_swmm_node_index = static_cast<int>(j1),
            .requested_volume = requested_volumes[i],
            .source_roof_area = 200.0,
        };
        const auto acceptance = accept(intent);

        // (1) per-intent closure, exact.
        EXPECT_NEAR(
            acceptance.accepted_volume + acceptance.rejected_volume,
            acceptance.requested_volume,
            1.0e-12);
        EXPECT_NEAR(acceptance.requested_volume, requested_volumes[i], 1.0e-12);

        // (2) hard gate: never above V_limit, exact.
        EXPECT_LE(acceptance.accepted_volume, kVLimit + 1.0e-12);
        if (requested_volumes[i] > kVLimit) {
            EXPECT_EQ(acceptance.rejection_reason, RoofDrainageRejectionReason::CapacityLimited);
            EXPECT_NEAR(acceptance.accepted_volume, kVLimit, 1.0e-12);
        } else {
            EXPECT_EQ(acceptance.rejection_reason, RoofDrainageRejectionReason::None);
            EXPECT_NEAR(acceptance.accepted_volume, requested_volumes[i], 1.0e-12);
        }

        total_requested += acceptance.requested_volume;
        total_accepted += acceptance.accepted_volume;
        total_rejected += acceptance.rejected_volume;

        if (static_cast<int>(i) == rollback_substep) {
            // (4) rollback leg: discard before the engine advances.
            driver.rollback_substep();
            driver.advance_engine();
            EXPECT_NEAR(engine.get_node_lateral_inflow(j1), 0.0, 1.0e-9);
            continue;
        }

        driver.advance_engine();
        total_delivered += acceptance.accepted_volume;

        // (3) the real solver routed exactly the granted flow.
        EXPECT_NEAR(
            engine.get_node_lateral_inflow(j1),
            acceptance.accepted_volume / kDtSub,
            1.0e-6);
    }

    // (5) totals close exactly at the acceptance boundary.
    EXPECT_NEAR(total_accepted + total_rejected, total_requested, 1.0e-9);

    // Delivered water excludes exactly the rolled-back substep's grant.
    EXPECT_NEAR(total_delivered, total_accepted - kVLimit, 1.0e-9);

    // The drainage network genuinely received and routed the water.
    EXPECT_GT(engine.get_node_inflow(o1), 0.0);
    EXPECT_GT(engine.get_node_head(j1), 10.0);
    EXPECT_FALSE(engine.is_surcharged(j1));

    engine.finalize();
}

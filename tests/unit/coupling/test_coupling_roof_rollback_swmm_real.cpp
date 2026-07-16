#include <optional>
#include <string>

#include <gtest/gtest.h>

#include "coupling/driver/roof_swmm_step_driver.hpp"
#include "coupling/drainage/swmm_engine.hpp"

namespace {

using scau::coupling::core::ExchangeCellState;
using scau::coupling::core::MassDeficitAccount;
using scau::coupling::drainage::SwmmEngine;
using scau::coupling::driver::RoofSwmmStepDriver;
using scau::surface2d::RoofDrainageIntent;
using scau::surface2d::RoofDrainageRejectionReason;

std::string minimal_case_path() {
    return std::string(SCAU_SWMM_TEST_CASE_DIR) + "/swmm_minimal.inp";
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

// Rollback contract against the real solver: a rolled-back substep's roof
// write must never enter SWMM routing.
TEST(RoofRollbackRealSwmm, RolledBackWriteDoesNotReachRealRouting) {
    SwmmEngine engine;
    engine.initialize(minimal_case_path());

    const std::size_t j1 = engine.node_index("J1");

    constexpr double dt_sub = 600.0;
    RoofSwmmStepDriver driver(
        engine,
        [](const RoofDrainageIntent&) { return std::make_optional(make_endpoint()); },
        dt_sub);

    driver.begin_substep();
    const auto accept = driver.acceptance_fn();
    const auto acceptance = accept(RoofDrainageIntent{
        .source_cell_index = 0,
        .target_swmm_node_index = static_cast<int>(j1),
        .requested_volume = 30.0,
        .source_roof_area = 100.0,
    });
    EXPECT_EQ(acceptance.rejection_reason, RoofDrainageRejectionReason::None);

    // Surface substep rolled back before the engine advanced.
    driver.rollback_substep();
    driver.advance_engine();

    // Nothing entered routing: no routed lateral inflow at the node.
    EXPECT_NEAR(engine.get_node_lateral_inflow(j1), 0.0, 1.0e-9);

    // The replayed substep works normally afterwards.
    driver.begin_substep();
    const auto replayed = accept(RoofDrainageIntent{
        .source_cell_index = 0,
        .target_swmm_node_index = static_cast<int>(j1),
        .requested_volume = 30.0,
        .source_roof_area = 100.0,
    });
    EXPECT_EQ(replayed.rejection_reason, RoofDrainageRejectionReason::None);
    driver.advance_engine();
    EXPECT_NEAR(engine.get_node_lateral_inflow(j1), 0.05, 1.0e-6);

    engine.finalize();
}

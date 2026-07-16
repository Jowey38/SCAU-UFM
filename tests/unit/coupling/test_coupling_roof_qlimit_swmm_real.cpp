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

// Endpoint with V_limit = 0.9 * 1.0 * 0.5 * 100 = 45 m3; over dt_sub = 600 s
// the hard gate is Q_limit = 0.075 m3/s.
ExchangeCellState make_endpoint() {
    return ExchangeCellState{
        .volume = 0.0,
        .mass_deficit_account = MassDeficitAccount{},
        .phi_t = 1.0,
        .h = 0.5,
        .area = 100.0,
    };
}

}  // namespace

// End-to-end arbitration against the real solver: an over-limit roof intent
// is clamped to Q_limit by CouplingLib BEFORE the SWMM adapter writes it, and
// the real engine routes exactly the granted flow.
TEST(RoofQLimitToRealSwmm, ClampedIntentIsWrittenAndRoutedByRealEngine) {
    SwmmEngine engine;
    engine.initialize(minimal_case_path());

    const std::size_t j1 = engine.node_index("J1");
    const std::size_t o1 = engine.node_index("O1");

    constexpr double dt_sub = 600.0;
    RoofSwmmStepDriver driver(
        engine,
        [](const RoofDrainageIntent&) { return std::make_optional(make_endpoint()); },
        dt_sub);

    driver.begin_substep();
    const auto accept = driver.acceptance_fn();

    // 60 m3 over 600 s = 0.1 m3/s > Q_limit 0.075 m3/s: clamp to 45 m3.
    const auto acceptance = accept(RoofDrainageIntent{
        .source_cell_index = 0,
        .target_swmm_node_index = static_cast<int>(j1),
        .requested_volume = 60.0,
        .source_roof_area = 200.0,
    });

    EXPECT_EQ(acceptance.rejection_reason, RoofDrainageRejectionReason::CapacityLimited);
    EXPECT_NEAR(acceptance.accepted_volume, 45.0, 1.0e-12);
    EXPECT_NEAR(acceptance.rejected_volume, 15.0, 1.0e-12);

    driver.advance_engine();

    // The real solver consumed exactly the granted flow, not the request.
    EXPECT_NEAR(engine.get_node_lateral_inflow(j1), 0.075, 1.0e-6);
    EXPECT_GT(engine.get_node_head(j1), 10.0);
    EXPECT_GT(engine.get_node_inflow(o1), 0.0);

    engine.finalize();
}

TEST(RoofQLimitToRealSwmm, DriverSchedulesRealEngineOnDtSub) {
    SwmmEngine engine;
    engine.initialize(minimal_case_path());

    const std::size_t j1 = engine.node_index("J1");

    constexpr double dt_sub = 300.0;
    RoofSwmmStepDriver driver(
        engine,
        [](const RoofDrainageIntent&) { return std::make_optional(make_endpoint()); },
        dt_sub);

    const auto accept = driver.acceptance_fn();
    for (int substep = 0; substep < 3; ++substep) {
        driver.begin_substep();
        const auto acceptance = accept(RoofDrainageIntent{
            .source_cell_index = 0,
            .target_swmm_node_index = static_cast<int>(j1),
            .requested_volume = 6.0,  // 0.02 m3/s, far under the hard gate
            .source_roof_area = 50.0,
        });
        EXPECT_EQ(acceptance.rejection_reason, RoofDrainageRejectionReason::None);
        driver.advance_engine();
    }

    EXPECT_EQ(driver.substep_count(), 3U);
    // SWMM advanced on the same dt_sub schedule as the surface substeps.
    EXPECT_GE(engine.elapsed_time(), 3.0 * dt_sub - 5.0);
    EXPECT_NEAR(engine.get_node_lateral_inflow(j1), 0.02, 1.0e-6);

    engine.finalize();
}

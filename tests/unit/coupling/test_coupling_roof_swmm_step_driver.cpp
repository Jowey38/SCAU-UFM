#include <optional>

#include <gtest/gtest.h>

#include "coupling/driver/roof_swmm_step_driver.hpp"
#include "coupling/drainage/mock_swmm_engine.hpp"

namespace {

using scau::coupling::core::ExchangeCellState;
using scau::coupling::core::MassDeficitAccount;
using scau::coupling::drainage::MockSwmmEngine;
using scau::coupling::driver::RoofSwmmStepDriver;
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

ExchangeCellState make_endpoint() {
    return ExchangeCellState{
        .volume = 0.0,
        .mass_deficit_account = MassDeficitAccount{},
        .phi_t = 1.0,
        .h = 1.0,
        .area = 100.0,
    };
}

}  // namespace

TEST(RoofSwmmStepDriver, AcceptanceFnArbitratesThenWritesEngine) {
    MockSwmmEngine engine(1U);
    RoofSwmmStepDriver driver(
        engine,
        [](const RoofDrainageIntent&) { return std::make_optional(make_endpoint()); },
        kDtSub);

    driver.begin_substep();
    const auto acceptance_fn = driver.acceptance_fn();

    // Under headroom: fully granted and written as q = 30/10 = 3 m3/s.
    const auto full = acceptance_fn(make_intent(30.0));
    EXPECT_EQ(full.rejection_reason, RoofDrainageRejectionReason::None);
    EXPECT_NEAR(engine.get_node_lateral_inflow(0U), 3.0, 1.0e-12);

    // Over headroom: Q_limit = 9 m3/s; per-node accumulation makes the total
    // request 3 + 12 = 15 m3/s -> only 6 m3/s more can be granted.
    const auto clamped = acceptance_fn(make_intent(120.0));
    EXPECT_EQ(clamped.rejection_reason, RoofDrainageRejectionReason::CapacityLimited);
    EXPECT_NEAR(engine.get_node_lateral_inflow(0U), 9.0, 1.0e-12);
}

TEST(RoofSwmmStepDriver, BeginSubstepResetsAccumulationAndCounts) {
    MockSwmmEngine engine(1U);
    RoofSwmmStepDriver driver(
        engine,
        [](const RoofDrainageIntent&) { return std::make_optional(make_endpoint()); },
        kDtSub);

    EXPECT_EQ(driver.substep_count(), 0U);

    driver.begin_substep();
    const auto acceptance_fn = driver.acceptance_fn();
    (void)acceptance_fn(make_intent(30.0));
    EXPECT_NEAR(engine.get_node_lateral_inflow(0U), 3.0, 1.0e-12);

    driver.begin_substep();
    EXPECT_EQ(driver.substep_count(), 2U);
    (void)acceptance_fn(make_intent(10.0));
    // Accumulation restarted: 10/10 = 1 m3/s, not 3 + 1.
    EXPECT_NEAR(engine.get_node_lateral_inflow(0U), 1.0, 1.0e-12);
}

TEST(RoofSwmmStepDriver, AdvanceEngineStepsByDtSub) {
    MockSwmmEngine engine(1U);
    RoofSwmmStepDriver driver(
        engine,
        [](const RoofDrainageIntent&) { return std::make_optional(make_endpoint()); },
        kDtSub);

    driver.begin_substep();
    driver.advance_engine();
    driver.begin_substep();
    driver.advance_engine();

    EXPECT_NEAR(engine.elapsed_time(), 2.0 * kDtSub, 1.0e-12);
    EXPECT_NEAR(driver.dt_sub(), kDtSub, 1.0e-12);
}

TEST(RoofSwmmStepDriver, RejectsInvalidDtAtConstruction) {
    MockSwmmEngine engine(1U);
    // The owned acceptance adapter validates dt_sub first; its error type is
    // the drainage-layer contract.
    EXPECT_THROW(
        RoofSwmmStepDriver(
            engine,
            [](const RoofDrainageIntent&) { return std::make_optional(make_endpoint()); },
            -1.0),
        scau::coupling::drainage::SwmmEngineError);
}

#include <cmath>
#include <stdexcept>

#include <gtest/gtest.h>

#include "coupling/driver/tri_coupling.hpp"

// Tri-coupling step driver, exercised with both mock engines: surface<->pipe,
// surface<->river, and pipe<->river pairwise bidirectional exchange in one
// dt_sub, with all arbitration core-owned and the engines never seeing each
// other (DTO-only seam).

namespace {

using scau::coupling::core::CouplingState;
using scau::coupling::core::SharedExchangeEngine;
using scau::coupling::driver::DrainageRiverLink;
using scau::coupling::driver::SurfaceDrainageLink;
using scau::coupling::driver::SurfaceRiverLink;
using scau::coupling::driver::TriCouplingStepConfig;
using scau::coupling::driver::advance_tri_coupling_step;
using scau::coupling::drainage::MockSwmmEngine;
using scau::coupling::river::MockDFlowFMEngine;

constexpr double kDtSub = 4.0;

CouplingState make_state() {
    // V_limit = 0.9 * phi_t * h * area = 0.9 * 0.4 * 2.0 * 50.0 = 36
    // Q_limit = 36 / 4 = 9
    return CouplingState{{
        {.volume = 40.0, .mass_deficit_account = {.volume = 0.0},
         .phi_t = 0.4, .h = 2.0, .area = 50.0},
        {.volume = 40.0, .mass_deficit_account = {.volume = 0.0},
         .phi_t = 0.4, .h = 2.0, .area = 50.0},
    }};
}

}  // namespace

TEST(CouplingTriDriver, SharedCellArbitrationFeedsBothEngines) {
    auto state = make_state();
    MockSwmmEngine swmm;
    MockDFlowFMEngine dflowfm;
    swmm.initialize("mock.inp");
    dflowfm.initialize("mock.mdu");

    TriCouplingStepConfig config{
        .surface_drainage = {{.cell_index = 0U, .node_id = 11, .q_request = 2.0, .priority_weight = 1.0}},
        .surface_river = {{.cell_index = 0U, .location_id = 5, .q_request = 4.0, .priority_weight = 2.0}},
    };

    const auto report = advance_tri_coupling_step(state, swmm, dflowfm, config, kDtSub);

    ASSERT_EQ(report.surface_decisions.size(), 2U);
    double granted_total_volume = 0.0;
    for (const auto& decision : report.surface_decisions) {
        granted_total_volume += decision.exchange.v_granted;
        const double q_accepted = decision.exchange.q_granted + decision.exchange.q_repay;
        if (decision.endpoint.engine == SharedExchangeEngine::drainage) {
            EXPECT_EQ(decision.endpoint.node_id, 11U);
            EXPECT_DOUBLE_EQ(swmm.get_node_lateral_inflow(11), q_accepted);
        } else {
            EXPECT_EQ(decision.endpoint.node_id, 5U);
            EXPECT_DOUBLE_EQ(dflowfm.get_value("lateral_discharge", 5), q_accepted);
        }
    }
    EXPECT_LE(granted_total_volume, 36.0 + 1.0e-9);

    // Engines were stepped by exactly one dt_sub.
    EXPECT_DOUBLE_EQ(swmm.elapsed_time(), kDtSub);
    EXPECT_DOUBLE_EQ(dflowfm.elapsed_time(), kDtSub);

    // The granted surface volume left the 2D cells after replay.
    EXPECT_LT(report.surface_mass_after.surface_mass, report.surface_mass_before.surface_mass);
}

TEST(CouplingTriDriver, OutfallDischargeTransfersIntoRiverLateral) {
    auto state = make_state();
    MockSwmmEngine swmm;
    MockDFlowFMEngine dflowfm;
    swmm.initialize("mock.inp");
    dflowfm.initialize("mock.mdu");

    swmm.set_node_inflow_fixture(21, 0.3);  // discharge arriving at the outfall

    TriCouplingStepConfig config{
        .drainage_river = {{.outfall_node_id = 21, .river_location_id = 7,
                            .q_capacity = 1.0, .drive_outfall_stage = false}},
    };

    const auto report = advance_tri_coupling_step(state, swmm, dflowfm, config, kDtSub);

    ASSERT_EQ(report.interface_decisions.size(), 1U);
    const auto& decision = report.interface_decisions[0];
    EXPECT_EQ(decision.source.engine, SharedExchangeEngine::drainage);
    EXPECT_EQ(decision.target.engine, SharedExchangeEngine::river);
    EXPECT_DOUBLE_EQ(decision.q_granted, 0.3);
    EXPECT_DOUBLE_EQ(decision.v_unmet, 0.0);
    EXPECT_DOUBLE_EQ(dflowfm.get_value("lateral_discharge", 7), 0.3);
}

TEST(CouplingTriDriver, InterfaceExchangeClampsToRiverCapacity) {
    auto state = make_state();
    MockSwmmEngine swmm;
    MockDFlowFMEngine dflowfm;
    swmm.initialize("mock.inp");
    dflowfm.initialize("mock.mdu");

    swmm.set_node_inflow_fixture(21, 2.0);

    TriCouplingStepConfig config{
        .drainage_river = {{.outfall_node_id = 21, .river_location_id = 7,
                            .q_capacity = 0.5, .drive_outfall_stage = false}},
    };

    const auto report = advance_tri_coupling_step(state, swmm, dflowfm, config, kDtSub);

    ASSERT_EQ(report.interface_decisions.size(), 1U);
    EXPECT_DOUBLE_EQ(report.interface_decisions[0].q_granted, 0.5);
    EXPECT_DOUBLE_EQ(report.interface_decisions[0].v_unmet, (2.0 - 0.5) * kDtSub);
    EXPECT_DOUBLE_EQ(dflowfm.get_value("lateral_discharge", 7), 0.5);
}

TEST(CouplingTriDriver, RiverWaterLevelDrivesOutfallStage) {
    auto state = make_state();
    MockSwmmEngine swmm;
    MockDFlowFMEngine dflowfm;
    swmm.initialize("mock.inp");
    dflowfm.initialize("mock.mdu");

    dflowfm.set_water_level_fixture(7, 9.4);

    TriCouplingStepConfig config{
        .drainage_river = {{.outfall_node_id = 21, .river_location_id = 7,
                            .q_capacity = 1.0, .drive_outfall_stage = true}},
    };

    static_cast<void>(advance_tri_coupling_step(state, swmm, dflowfm, config, kDtSub));

    EXPECT_DOUBLE_EQ(swmm.outfall_stage(21), 9.4);
}

TEST(CouplingTriDriver, NodeOverflowReturnsToSurfaceCell) {
    auto state = make_state();
    MockSwmmEngine swmm;
    MockDFlowFMEngine dflowfm;
    swmm.initialize("mock.inp");
    dflowfm.initialize("mock.mdu");

    swmm.set_node_overflow_fixture(11, 0.25);

    TriCouplingStepConfig config{
        .surface_drainage = {{.cell_index = 0U, .node_id = 11, .q_request = 0.0, .priority_weight = 1.0}},
    };

    const auto report = advance_tri_coupling_step(state, swmm, dflowfm, config, kDtSub);

    ASSERT_EQ(report.return_decisions.size(), 1U);
    EXPECT_EQ(report.return_decisions[0].source.engine, SharedExchangeEngine::drainage);
    EXPECT_DOUBLE_EQ(report.return_decisions[0].v_returned, 0.25 * kDtSub);
    EXPECT_NEAR(
        report.surface_mass_after.surface_mass - report.surface_mass_before.surface_mass,
        0.25 * kDtSub,
        1.0e-9);
}

TEST(CouplingTriDriver, RiverSpillReturnsToSurfaceCell) {
    auto state = make_state();
    MockSwmmEngine swmm;
    MockDFlowFMEngine dflowfm;
    swmm.initialize("mock.inp");
    dflowfm.initialize("mock.mdu");

    TriCouplingStepConfig config{
        .surface_river = {{.cell_index = 1U, .location_id = 5, .q_request = 0.0,
                           .priority_weight = 1.0, .q_return = 0.1}},
    };

    const auto report = advance_tri_coupling_step(state, swmm, dflowfm, config, kDtSub);

    ASSERT_EQ(report.return_decisions.size(), 1U);
    EXPECT_EQ(report.return_decisions[0].source.engine, SharedExchangeEngine::river);
    EXPECT_DOUBLE_EQ(report.return_decisions[0].v_returned, 0.1 * kDtSub);
    EXPECT_DOUBLE_EQ(state.cells()[1].volume, 40.0 + 0.1 * kDtSub);
}

TEST(CouplingTriDriver, EnginesStayIsolatedFromEachOther) {
    auto state = make_state();
    MockSwmmEngine swmm;
    MockDFlowFMEngine dflowfm;
    swmm.initialize("mock.inp");
    dflowfm.initialize("mock.mdu");

    swmm.set_node_inflow_fixture(21, 0.3);
    dflowfm.set_water_level_fixture(7, 9.4);

    TriCouplingStepConfig config{
        .surface_drainage = {{.cell_index = 0U, .node_id = 11, .q_request = 1.0, .priority_weight = 1.0}},
        .surface_river = {{.cell_index = 0U, .location_id = 5, .q_request = 1.0, .priority_weight = 1.0}},
        .drainage_river = {{.outfall_node_id = 21, .river_location_id = 7,
                            .q_capacity = 1.0, .drive_outfall_stage = true}},
    };

    static_cast<void>(advance_tri_coupling_step(state, swmm, dflowfm, config, kDtSub));

    // Mutual isolation: each mock only carries its own engine's state; the
    // exchange happened exclusively through core DTO decisions.
    EXPECT_DOUBLE_EQ(swmm.get_node_lateral_inflow(21), 0.0);
    EXPECT_DOUBLE_EQ(dflowfm.get_value("lateral_discharge", 5), 1.0);
    EXPECT_DOUBLE_EQ(dflowfm.get_value("lateral_discharge", 7), 0.3);
    EXPECT_DOUBLE_EQ(swmm.outfall_stage(21), 9.4);
}

TEST(CouplingTriDriver, FailsClosedOnInvalidConfig) {
    auto state = make_state();
    MockSwmmEngine swmm;
    MockDFlowFMEngine dflowfm;
    swmm.initialize("mock.inp");
    dflowfm.initialize("mock.mdu");

    EXPECT_THROW(
        static_cast<void>(advance_tri_coupling_step(state, swmm, dflowfm, {}, 0.0)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(advance_tri_coupling_step(state, swmm, dflowfm, {}, std::nan(""))),
        std::invalid_argument);

    TriCouplingStepConfig duplicate_nodes{
        .surface_drainage = {
            {.cell_index = 0U, .node_id = 11, .q_request = 1.0, .priority_weight = 1.0},
            {.cell_index = 1U, .node_id = 11, .q_request = 1.0, .priority_weight = 1.0},
        },
    };
    EXPECT_THROW(
        static_cast<void>(advance_tri_coupling_step(state, swmm, dflowfm, duplicate_nodes, kDtSub)),
        std::invalid_argument);

    TriCouplingStepConfig negative_request{
        .surface_river = {{.cell_index = 0U, .location_id = 5, .q_request = -1.0, .priority_weight = 1.0}},
    };
    EXPECT_THROW(
        static_cast<void>(advance_tri_coupling_step(state, swmm, dflowfm, negative_request, kDtSub)),
        std::invalid_argument);

    TriCouplingStepConfig out_of_range_cell{
        .surface_drainage = {{.cell_index = 99U, .node_id = 11, .q_request = 1.0, .priority_weight = 1.0}},
    };
    EXPECT_THROW(
        static_cast<void>(advance_tri_coupling_step(state, swmm, dflowfm, out_of_range_cell, kDtSub)),
        std::invalid_argument);

    TriCouplingStepConfig duplicate_river_interface_target{
        .drainage_river = {
            {.outfall_node_id = 21, .river_location_id = 7, .q_capacity = 1.0},
            {.outfall_node_id = 22, .river_location_id = 7, .q_capacity = 1.0},
        },
    };
    EXPECT_THROW(
        static_cast<void>(advance_tri_coupling_step(
            state, swmm, dflowfm, duplicate_river_interface_target, kDtSub)),
        std::invalid_argument);

    // No engine state was touched by the rejected configs.
    EXPECT_DOUBLE_EQ(swmm.elapsed_time(), 0.0);
    EXPECT_DOUBLE_EQ(dflowfm.elapsed_time(), 0.0);
}

TEST(CouplingTriDriver, EmptyConfigIsAConservativeNoop) {
    auto state = make_state();
    MockSwmmEngine swmm;
    MockDFlowFMEngine dflowfm;
    swmm.initialize("mock.inp");
    dflowfm.initialize("mock.mdu");

    const auto report = advance_tri_coupling_step(state, swmm, dflowfm, {}, kDtSub);

    EXPECT_TRUE(report.surface_decisions.empty());
    EXPECT_TRUE(report.interface_decisions.empty());
    EXPECT_TRUE(report.return_decisions.empty());
    EXPECT_DOUBLE_EQ(
        report.surface_mass_after.surface_mass, report.surface_mass_before.surface_mass);
    EXPECT_DOUBLE_EQ(swmm.elapsed_time(), kDtSub);
    EXPECT_DOUBLE_EQ(dflowfm.elapsed_time(), kDtSub);
}

TEST(CouplingTriDriver, HeadDrivenDrainageIntentUsesWaterLevels) {
    auto state = make_state();
    MockSwmmEngine swmm;
    MockDFlowFMEngine dflowfm;
    swmm.initialize("mock.inp");
    dflowfm.initialize("mock.mdu");

    // Surface 0.4 m over the inlet crest, node head below the crest:
    // free weir intent Q = 1.7 * 2.0 * 0.4^1.5.
    swmm.set_node_head_fixture(11, 9.0);

    TriCouplingStepConfig config{
        .surface_drainage = {{
            .cell_index = 0U,
            .node_id = 11,
            .priority_weight = 1.0,
            .geometry = scau::coupling::core::ExchangeFlowGeometry{
                .crest_level = 10.0,
                .exchange_width = 2.0,
            },
            .surface_water_level = 10.4,
        }},
    };

    const auto report = advance_tri_coupling_step(state, swmm, dflowfm, config, kDtSub);

    const double expected_intent = 1.7 * 2.0 * std::pow(0.4, 1.5);
    ASSERT_EQ(report.surface_decisions.size(), 1U);
    EXPECT_NEAR(report.surface_decisions[0].exchange.q_granted, expected_intent, 1.0e-9);
    EXPECT_TRUE(report.return_decisions.empty());
    EXPECT_GT(swmm.get_node_lateral_inflow(11), 0.0);
}

TEST(CouplingTriDriver, HeadDrivenDrainageReverseFlowReturnsToSurface) {
    auto state = make_state();
    MockSwmmEngine swmm;
    MockDFlowFMEngine dflowfm;
    swmm.initialize("mock.inp");
    dflowfm.initialize("mock.mdu");

    // Node head 0.4 m over the crest, surface below: reverse (surcharge)
    // weir flow back onto the surface; no drain intent.
    swmm.set_node_head_fixture(11, 10.4);

    TriCouplingStepConfig config{
        .surface_drainage = {{
            .cell_index = 0U,
            .node_id = 11,
            .priority_weight = 1.0,
            .geometry = scau::coupling::core::ExchangeFlowGeometry{
                .crest_level = 10.0,
                .exchange_width = 2.0,
            },
            .surface_water_level = 9.0,
        }},
    };

    const auto report = advance_tri_coupling_step(state, swmm, dflowfm, config, kDtSub);

    const double expected_return = 1.7 * 2.0 * std::pow(0.4, 1.5);
    ASSERT_EQ(report.return_decisions.size(), 1U);
    EXPECT_NEAR(report.return_decisions[0].q_returned, expected_return, 1.0e-9);
    ASSERT_EQ(report.surface_decisions.size(), 1U);
    EXPECT_DOUBLE_EQ(report.surface_decisions[0].exchange.q_granted, 0.0);
    EXPECT_DOUBLE_EQ(swmm.get_node_lateral_inflow(11), 0.0);
}

TEST(CouplingTriDriver, HeadDrivenRiverSpillUsesVillemonteSubmergedWeir) {
    auto state = make_state();
    MockSwmmEngine swmm;
    MockDFlowFMEngine dflowfm;
    swmm.initialize("mock.inp");
    dflowfm.initialize("mock.mdu");

    // River 0.4 m over the bank crest, surface 0.2 m over: submerged weir
    // spill into the 2D cell with Villemonte reduction.
    dflowfm.set_water_level_fixture(5, 10.4);

    TriCouplingStepConfig config{
        .surface_river = {{
            .cell_index = 1U,
            .location_id = 5,
            .priority_weight = 1.0,
            .geometry = scau::coupling::core::ExchangeFlowGeometry{
                .crest_level = 10.0,
                .exchange_width = 2.0,
            },
            .surface_water_level = 10.2,
        }},
    };

    const auto report = advance_tri_coupling_step(state, swmm, dflowfm, config, kDtSub);

    const double q_free = 1.7 * 2.0 * std::pow(0.4, 1.5);
    const double expected =
        q_free * std::pow(1.0 - std::pow(0.2 / 0.4, 1.5), 0.385);
    ASSERT_EQ(report.return_decisions.size(), 1U);
    EXPECT_NEAR(report.return_decisions[0].q_returned, expected, 1.0e-9);
    EXPECT_EQ(report.return_decisions[0].source.engine, SharedExchangeEngine::river);
}

TEST(CouplingTriDriver, HeadDrivenModeRejectsNonFiniteSurfaceLevel) {
    auto state = make_state();
    MockSwmmEngine swmm;
    MockDFlowFMEngine dflowfm;
    swmm.initialize("mock.inp");
    dflowfm.initialize("mock.mdu");

    TriCouplingStepConfig config{
        .surface_drainage = {{
            .cell_index = 0U,
            .node_id = 11,
            .priority_weight = 1.0,
            .geometry = scau::coupling::core::ExchangeFlowGeometry{
                .crest_level = 10.0,
                .exchange_width = 2.0,
            },
            .surface_water_level = std::nan(""),
        }},
    };

    EXPECT_THROW(
        static_cast<void>(advance_tri_coupling_step(state, swmm, dflowfm, config, kDtSub)),
        std::invalid_argument);
    EXPECT_DOUBLE_EQ(swmm.elapsed_time(), 0.0);
}

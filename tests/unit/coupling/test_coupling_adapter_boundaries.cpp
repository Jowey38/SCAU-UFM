#include <limits>
#include <type_traits>

#include <gtest/gtest.h>

#include "coupling/core/state.hpp"
#include "coupling/drainage/swmm_boundary.hpp"
#include "coupling/river/dflowfm_boundary.hpp"

namespace {

template <typename T, typename = void>
struct HasRollback : std::false_type {};

template <typename T>
struct HasRollback<T, std::void_t<decltype(&T::rollback)>> : std::true_type {};

template <typename T, typename = void>
struct HasReplayPending : std::false_type {};

template <typename T>
struct HasReplayPending<T, std::void_t<decltype(&T::replay_pending)>> : std::true_type {};

template <typename T, typename = void>
struct HasMassDeficitAccountMember : std::false_type {};

template <typename T>
struct HasMassDeficitAccountMember<T, std::void_t<decltype(&T::mass_deficit_account)>> : std::true_type {};

template <typename T, typename = void>
struct HasQLimitMember : std::false_type {};

template <typename T>
struct HasQLimitMember<T, std::void_t<decltype(&T::q_limit)>> : std::true_type {};

template <typename T, typename = void>
struct HasVLimitMember : std::false_type {};

template <typename T>
struct HasVLimitMember<T, std::void_t<decltype(&T::v_limit)>> : std::true_type {};

template <typename T, typename = void>
struct HasAcceptExchangeDecision : std::false_type {};

template <typename T>
struct HasAcceptExchangeDecision<T, std::void_t<decltype(&T::accept_exchange_decision)>> : std::true_type {};

}  // namespace

TEST(CouplingAdapterBoundaries, SwmmMockIsThirdPartyFreeAndUsesCoreExchangeDtos) {
    scau::coupling::drainage::MockSwmmEngine engine;
    engine.initialize("mock.inp");

    const auto request = scau::coupling::drainage::make_swmm_exchange_request(2.0, 5.0);
    const scau::coupling::core::ExchangeDecision decision{
        .q_granted = 1.25,
        .v_granted = 6.25,
        .q_repay = 0.25,
        .v_repay = 1.25,
        .v_unmet = 3.75,
    };

    scau::coupling::drainage::accept_swmm_exchange_decision(engine, 7, decision, request.dt_sub);

    EXPECT_DOUBLE_EQ(request.q_request, 2.0);
    EXPECT_DOUBLE_EQ(request.dt_sub, 5.0);
    EXPECT_DOUBLE_EQ(engine.get_node_lateral_inflow(7), 1.5);
    EXPECT_FALSE(engine.is_surcharged(7));
}

TEST(CouplingAdapterBoundaries, SwmmMockFixturesProvideDeterministicState) {
    scau::coupling::drainage::MockSwmmEngine engine;
    engine.initialize("mock.inp");

    engine.set_node_head_fixture(3, 12.5);
    engine.set_link_flow_fixture(4, 2.75);
    engine.set_node_surcharge_fixture(5, true);

    EXPECT_DOUBLE_EQ(engine.get_node_head(3), 12.5);
    EXPECT_DOUBLE_EQ(engine.get_link_flow(4), 2.75);
    EXPECT_TRUE(engine.is_surcharged(5));
    EXPECT_FALSE(engine.is_surcharged(6));
}

TEST(CouplingAdapterBoundaries, SwmmMockFixturesRejectInvalidInputs) {
    scau::coupling::drainage::MockSwmmEngine engine;
    engine.initialize("mock.inp");

    EXPECT_THROW(engine.set_node_head_fixture(-1, 1.0), scau::coupling::drainage::SwmmEngineError);
    EXPECT_THROW(engine.set_node_head_fixture(1, std::numeric_limits<double>::quiet_NaN()), scau::coupling::drainage::SwmmEngineError);
    EXPECT_THROW(engine.set_link_flow_fixture(-1, 1.0), scau::coupling::drainage::SwmmEngineError);
    EXPECT_THROW(engine.set_link_flow_fixture(1, std::numeric_limits<double>::infinity()), scau::coupling::drainage::SwmmEngineError);
    EXPECT_THROW(engine.set_node_surcharge_fixture(-1, true), scau::coupling::drainage::SwmmEngineError);
}

TEST(CouplingAdapterBoundaries, SwmmMockFixturesRequireInitializedEngine) {
    scau::coupling::drainage::MockSwmmEngine engine;

    EXPECT_THROW(engine.set_node_head_fixture(1, 1.0), scau::coupling::drainage::SwmmEngineError);
    EXPECT_THROW(engine.set_link_flow_fixture(1, 1.0), scau::coupling::drainage::SwmmEngineError);
    EXPECT_THROW(engine.set_node_surcharge_fixture(1, true), scau::coupling::drainage::SwmmEngineError);
}

TEST(CouplingAdapterBoundaries, DFlowFMMockTypedFixturesProvideDeterministicState) {
    scau::coupling::river::MockDFlowFMEngine engine;
    engine.initialize("mock.mdu");

    engine.set_water_level_fixture(1, 2.5);
    engine.set_discharge_fixture(2, 3.5);
    engine.set_lateral_discharge_fixture(3, 4.5);
    engine.set_gate_opening_fixture(4, 0.75);

    EXPECT_DOUBLE_EQ(engine.get_value("water_level", 1), 2.5);
    EXPECT_DOUBLE_EQ(engine.get_value("discharge", 2), 3.5);
    EXPECT_DOUBLE_EQ(engine.get_value("lateral_discharge", 3), 4.5);
    EXPECT_DOUBLE_EQ(engine.get_value("gate_opening", 4), 0.75);
}

TEST(CouplingAdapterBoundaries, DFlowFMMockSupportsCompoundLateralPathsAtLocationZero) {
    scau::coupling::river::MockDFlowFMEngine engine;
    engine.initialize("mock.mdu");

    engine.set_value("laterals/lat1/water_discharge", 0, 0.125);
    EXPECT_DOUBLE_EQ(engine.get_value("laterals/lat1/water_discharge", 0), 0.125);
    EXPECT_THROW(
        engine.set_value("laterals/lat1/water_discharge", 1, 0.125),
        scau::coupling::river::DFlowFMEngineError);
    EXPECT_THROW(
        engine.set_value("laterals//water_discharge", 0, 0.125),
        scau::coupling::river::DFlowFMEngineError);
    EXPECT_THROW(
        engine.set_value("laterals/lat1/discharge", 0, 0.125),
        scau::coupling::river::DFlowFMEngineError);
}

TEST(CouplingAdapterBoundaries, DFlowFMMockTypedFixturesReuseBoundaryValidation) {
    scau::coupling::river::MockDFlowFMEngine engine;
    engine.initialize("mock.mdu");

    EXPECT_THROW(engine.set_water_level_fixture(-1, 1.0), scau::coupling::river::DFlowFMEngineError);
    EXPECT_THROW(engine.set_discharge_fixture(1, std::numeric_limits<double>::quiet_NaN()), scau::coupling::river::DFlowFMEngineError);
    EXPECT_THROW(engine.set_lateral_discharge_fixture(1, std::numeric_limits<double>::infinity()), scau::coupling::river::DFlowFMEngineError);
    EXPECT_THROW(engine.set_gate_opening_fixture(1, 1.1), scau::coupling::river::DFlowFMEngineError);
}

TEST(CouplingAdapterBoundaries, DFlowFMMockTypedFixturesRequireInitializedEngine) {
    scau::coupling::river::MockDFlowFMEngine engine;

    EXPECT_THROW(engine.set_water_level_fixture(1, 1.0), scau::coupling::river::DFlowFMEngineError);
    EXPECT_THROW(engine.set_discharge_fixture(1, 1.0), scau::coupling::river::DFlowFMEngineError);
    EXPECT_THROW(engine.set_lateral_discharge_fixture(1, 1.0), scau::coupling::river::DFlowFMEngineError);
    EXPECT_THROW(engine.set_gate_opening_fixture(1, 0.5), scau::coupling::river::DFlowFMEngineError);
}

TEST(CouplingAdapterBoundaries, DFlowFMMockIsThirdPartyFreeAndUsesCoreExchangeDtos) {
    scau::coupling::river::MockDFlowFMEngine engine;
    engine.initialize("mock.mdu");

    const auto request = scau::coupling::river::make_dflowfm_exchange_request(3.0, 4.0);
    const scau::coupling::core::ExchangeDecision decision{
        .q_granted = 1.75,
        .v_granted = 7.0,
        .q_repay = 0.5,
        .v_repay = 2.0,
        .v_unmet = 5.0,
    };

    scau::coupling::river::accept_dflowfm_exchange_decision(
        engine,
        "lateral_discharge",
        11,
        decision,
        request.dt_sub);

    EXPECT_DOUBLE_EQ(request.q_request, 3.0);
    EXPECT_DOUBLE_EQ(request.dt_sub, 4.0);
    EXPECT_DOUBLE_EQ(engine.get_value("lateral_discharge", 11), 2.25);
}

TEST(CouplingAdapterBoundaries, SwmmAndDFlowFMMocksRemainIndependent) {
    scau::coupling::drainage::MockSwmmEngine swmm;
    scau::coupling::river::MockDFlowFMEngine dflowfm;
    swmm.initialize("mock.inp");
    dflowfm.initialize("mock.mdu");

    swmm.set_node_lateral_inflow(1, 4.0);
    dflowfm.set_value("lateral_discharge", 1, 9.0);
    swmm.step(2.0);
    dflowfm.update(3.0);

    EXPECT_DOUBLE_EQ(swmm.get_node_lateral_inflow(1), 4.0);
    EXPECT_DOUBLE_EQ(dflowfm.get_value("lateral_discharge", 1), 9.0);
    EXPECT_DOUBLE_EQ(swmm.elapsed_time(), 2.0);
    EXPECT_DOUBLE_EQ(dflowfm.elapsed_time(), 3.0);
}

TEST(CouplingAdapterBoundaries, DualMockFixtureScenarioAcceptsCoreDecisionsIndependently) {
    scau::coupling::drainage::MockSwmmEngine swmm;
    scau::coupling::river::MockDFlowFMEngine dflowfm;
    swmm.initialize("mock.inp");
    dflowfm.initialize("mock.mdu");

    swmm.set_node_head_fixture(10, 3.25);
    swmm.set_link_flow_fixture(20, 1.5);
    swmm.set_node_surcharge_fixture(10, true);
    dflowfm.set_water_level_fixture(30, 2.75);
    dflowfm.set_discharge_fixture(40, 6.0);
    dflowfm.set_gate_opening_fixture(50, 0.5);

    const auto swmm_request = scau::coupling::drainage::make_swmm_exchange_request(2.0, 4.0);
    const auto dflowfm_request = scau::coupling::river::make_dflowfm_exchange_request(3.0, 4.0);
    const scau::coupling::core::ExchangeDecision swmm_decision{
        .q_granted = 1.0,
        .v_granted = 4.0,
        .q_repay = 0.25,
        .v_repay = 1.0,
        .v_unmet = 3.0,
    };
    const scau::coupling::core::ExchangeDecision dflowfm_decision{
        .q_granted = 1.5,
        .v_granted = 6.0,
        .q_repay = 0.5,
        .v_repay = 2.0,
        .v_unmet = 4.0,
    };

    scau::coupling::drainage::accept_swmm_exchange_decision(swmm, 10, swmm_decision, swmm_request.dt_sub);
    scau::coupling::river::accept_dflowfm_exchange_decision(
        dflowfm,
        "lateral_discharge",
        30,
        dflowfm_decision,
        dflowfm_request.dt_sub);

    EXPECT_DOUBLE_EQ(swmm.get_node_head(10), 3.25);
    EXPECT_DOUBLE_EQ(swmm.get_link_flow(20), 1.5);
    EXPECT_TRUE(swmm.is_surcharged(10));
    EXPECT_DOUBLE_EQ(swmm.get_node_lateral_inflow(10), 1.25);
    EXPECT_DOUBLE_EQ(dflowfm.get_value("water_level", 30), 2.75);
    EXPECT_DOUBLE_EQ(dflowfm.get_value("discharge", 40), 6.0);
    EXPECT_DOUBLE_EQ(dflowfm.get_value("gate_opening", 50), 0.5);
    EXPECT_DOUBLE_EQ(dflowfm.get_value("lateral_discharge", 30), 2.0);
}

TEST(CouplingAdapterBoundaries, RiverExchangePointPreservesPositivePriorityWeight) {
    const scau::coupling::river::RiverExchangePoint point{
        .branch_id = 2,
        .link_id = 5,
        .exchange_type = "lateral_weir",
        .crest_level = 8.5,
        .exchange_width = 12.0,
        .priority_weight = 3.0,
        .rtc_controlled = true,
        .neighbor_2d_edges = {10, 11},
    };

    EXPECT_TRUE(scau::coupling::river::is_valid_river_exchange_point(point));
    EXPECT_DOUBLE_EQ(point.priority_weight, 3.0);
    EXPECT_EQ(point.neighbor_2d_edges.size(), 2U);
}

TEST(CouplingAdapterBoundaries, RiverExchangePointRejectsNonPositivePriorityWeight) {
    const scau::coupling::river::RiverExchangePoint point{
        .exchange_type = "gate_outlet",
        .exchange_width = 1.0,
        .priority_weight = 0.0,
        .neighbor_2d_edges = {1},
    };

    EXPECT_FALSE(scau::coupling::river::is_valid_river_exchange_point(point));
}

TEST(CouplingAdapterBoundaries, RiverExchangePointRejectsNonFinitePriorityWeight) {
    const scau::coupling::river::RiverExchangePoint infinite_point{
        .exchange_type = "gate_outlet",
        .exchange_width = 1.0,
        .priority_weight = std::numeric_limits<double>::infinity(),
        .neighbor_2d_edges = {1},
    };
    const scau::coupling::river::RiverExchangePoint nan_point{
        .exchange_type = "gate_outlet",
        .exchange_width = 1.0,
        .priority_weight = std::numeric_limits<double>::quiet_NaN(),
        .neighbor_2d_edges = {1},
    };

    EXPECT_FALSE(scau::coupling::river::is_valid_river_exchange_point(infinite_point));
    EXPECT_FALSE(scau::coupling::river::is_valid_river_exchange_point(nan_point));
}

TEST(CouplingAdapterBoundaries, RiverExchangePointRejectsNonFiniteExchangeWidth) {
    const scau::coupling::river::RiverExchangePoint infinite_point{
        .exchange_type = "gate_outlet",
        .exchange_width = std::numeric_limits<double>::infinity(),
        .priority_weight = 1.0,
        .neighbor_2d_edges = {1},
    };
    const scau::coupling::river::RiverExchangePoint nan_point{
        .exchange_type = "gate_outlet",
        .exchange_width = std::numeric_limits<double>::quiet_NaN(),
        .priority_weight = 1.0,
        .neighbor_2d_edges = {1},
    };

    EXPECT_FALSE(scau::coupling::river::is_valid_river_exchange_point(infinite_point));
    EXPECT_FALSE(scau::coupling::river::is_valid_river_exchange_point(nan_point));
}

TEST(CouplingAdapterBoundaries, RiverExchangePointRejectsUnsupportedExchangeType) {
    const scau::coupling::river::RiverExchangePoint point{
        .exchange_type = "unsupported",
        .exchange_width = 1.0,
        .priority_weight = 1.0,
        .neighbor_2d_edges = {1},
    };

    EXPECT_FALSE(scau::coupling::river::is_valid_river_exchange_point(point));
}

TEST(CouplingAdapterBoundaries, RiverExchangePointRejectsInvalidNeighborEdges) {
    const scau::coupling::river::RiverExchangePoint empty_edges{
        .exchange_type = "bank_overtop",
        .exchange_width = 1.0,
        .priority_weight = 1.0,
    };
    const scau::coupling::river::RiverExchangePoint negative_edge{
        .exchange_type = "culvert_link",
        .exchange_width = 1.0,
        .priority_weight = 1.0,
        .neighbor_2d_edges = {1, -2},
    };

    EXPECT_FALSE(scau::coupling::river::is_valid_river_exchange_point(empty_edges));
    EXPECT_FALSE(scau::coupling::river::is_valid_river_exchange_point(negative_edge));
}

TEST(CouplingAdapterBoundaries, RiverExchangePointAcceptsSupportedExchangeTypes) {
    for (const char* exchange_type : {"bank_overtop", "lateral_weir", "gate_outlet", "culvert_link"}) {
        const scau::coupling::river::RiverExchangePoint point{
            .exchange_type = exchange_type,
            .exchange_width = 1.0,
            .priority_weight = 1.0,
            .neighbor_2d_edges = {1},
        };

        EXPECT_TRUE(scau::coupling::river::is_valid_river_exchange_point(point));
    }
}

TEST(CouplingAdapterBoundaries, RiverExchangePointRejectsInvalidIdsAndCrestLevel) {
    const scau::coupling::river::RiverExchangePoint negative_branch{
        .branch_id = -1,
        .link_id = 1,
        .exchange_type = "bank_overtop",
        .crest_level = 1.0,
        .exchange_width = 1.0,
        .priority_weight = 1.0,
        .neighbor_2d_edges = {1},
    };
    const scau::coupling::river::RiverExchangePoint negative_link{
        .branch_id = 1,
        .link_id = -1,
        .exchange_type = "lateral_weir",
        .crest_level = 1.0,
        .exchange_width = 1.0,
        .priority_weight = 1.0,
        .neighbor_2d_edges = {1},
    };
    const scau::coupling::river::RiverExchangePoint nonfinite_crest{
        .branch_id = 1,
        .link_id = 1,
        .exchange_type = "gate_outlet",
        .crest_level = std::numeric_limits<double>::infinity(),
        .exchange_width = 1.0,
        .priority_weight = 1.0,
        .neighbor_2d_edges = {1},
    };

    EXPECT_FALSE(scau::coupling::river::is_valid_river_exchange_point(negative_branch));
    EXPECT_FALSE(scau::coupling::river::is_valid_river_exchange_point(negative_link));
    EXPECT_FALSE(scau::coupling::river::is_valid_river_exchange_point(nonfinite_crest));
}

TEST(CouplingAdapterBoundaries, SwmmMockGettersRequireInitializedEngine) {
    scau::coupling::drainage::MockSwmmEngine engine;

    EXPECT_THROW(static_cast<void>(engine.get_node_head(1)), scau::coupling::drainage::SwmmEngineError);
    EXPECT_THROW(static_cast<void>(engine.get_node_lateral_inflow(1)), scau::coupling::drainage::SwmmEngineError);
    EXPECT_THROW(static_cast<void>(engine.get_link_flow(1)), scau::coupling::drainage::SwmmEngineError);
    EXPECT_THROW(static_cast<void>(engine.is_surcharged(1)), scau::coupling::drainage::SwmmEngineError);

    engine.initialize("mock.inp");
    engine.finalize();

    EXPECT_THROW(static_cast<void>(engine.get_node_head(1)), scau::coupling::drainage::SwmmEngineError);
    EXPECT_THROW(static_cast<void>(engine.get_node_lateral_inflow(1)), scau::coupling::drainage::SwmmEngineError);
    EXPECT_THROW(static_cast<void>(engine.get_link_flow(1)), scau::coupling::drainage::SwmmEngineError);
    EXPECT_THROW(static_cast<void>(engine.is_surcharged(1)), scau::coupling::drainage::SwmmEngineError);
}

TEST(CouplingAdapterBoundaries, DFlowFMMockGettersRequireInitializedEngine) {
    scau::coupling::river::MockDFlowFMEngine engine;

    EXPECT_THROW(static_cast<void>(engine.get_value("lateral_discharge", 1)), scau::coupling::river::DFlowFMEngineError);

    engine.initialize("mock.mdu");
    engine.finalize();

    EXPECT_THROW(static_cast<void>(engine.get_value("lateral_discharge", 1)), scau::coupling::river::DFlowFMEngineError);
}

TEST(CouplingAdapterBoundaries, SwmmMockRejectsNegativeIds) {
    scau::coupling::drainage::MockSwmmEngine engine;
    engine.initialize("mock.inp");

    EXPECT_THROW(static_cast<void>(engine.get_node_head(-1)), scau::coupling::drainage::SwmmEngineError);
    EXPECT_THROW(static_cast<void>(engine.get_node_lateral_inflow(-1)), scau::coupling::drainage::SwmmEngineError);
    EXPECT_THROW(static_cast<void>(engine.get_link_flow(-1)), scau::coupling::drainage::SwmmEngineError);
    EXPECT_THROW(static_cast<void>(engine.is_surcharged(-1)), scau::coupling::drainage::SwmmEngineError);
    EXPECT_THROW(engine.set_node_lateral_inflow(-1, 1.0), scau::coupling::drainage::SwmmEngineError);

    const scau::coupling::core::ExchangeDecision decision{
        .q_granted = 1.0,
        .v_granted = 1.0,
        .q_repay = 0.0,
        .v_repay = 0.0,
        .v_unmet = 0.0,
    };
    EXPECT_THROW(
        scau::coupling::drainage::accept_swmm_exchange_decision(engine, -1, decision, 1.0),
        scau::coupling::drainage::SwmmEngineError);
}

TEST(CouplingAdapterBoundaries, DFlowFMMockRejectsNegativeLocationIds) {
    scau::coupling::river::MockDFlowFMEngine engine;
    engine.initialize("mock.mdu");

    EXPECT_THROW(static_cast<void>(engine.get_value("lateral_discharge", -1)), scau::coupling::river::DFlowFMEngineError);
    EXPECT_THROW(engine.set_value("lateral_discharge", -1, 1.0), scau::coupling::river::DFlowFMEngineError);

    const scau::coupling::core::ExchangeDecision decision{
        .q_granted = 1.0,
        .v_granted = 1.0,
        .q_repay = 0.0,
        .v_repay = 0.0,
        .v_unmet = 0.0,
    };
    EXPECT_THROW(
        scau::coupling::river::accept_dflowfm_exchange_decision(engine, "lateral_discharge", -1, decision, 1.0),
        scau::coupling::river::DFlowFMEngineError);
}

TEST(CouplingAdapterBoundaries, SwmmMockRejectsNonFiniteInputs) {
    scau::coupling::drainage::MockSwmmEngine engine;
    engine.initialize("mock.inp");

    EXPECT_THROW(engine.step(std::numeric_limits<double>::infinity()), scau::coupling::drainage::SwmmEngineError);
    EXPECT_THROW(engine.set_node_lateral_inflow(1, std::numeric_limits<double>::quiet_NaN()), scau::coupling::drainage::SwmmEngineError);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::drainage::make_swmm_exchange_request(
            std::numeric_limits<double>::infinity(),
            1.0)),
        scau::coupling::drainage::SwmmEngineError);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::drainage::make_swmm_exchange_request(
            1.0,
            std::numeric_limits<double>::quiet_NaN())),
        scau::coupling::drainage::SwmmEngineError);
}

TEST(CouplingAdapterBoundaries, DFlowFMMockRejectsUnsupportedVariables) {
    scau::coupling::river::MockDFlowFMEngine engine;
    engine.initialize("mock.mdu");

    EXPECT_NO_THROW(engine.set_value("water_level", 1, 2.0));
    EXPECT_NO_THROW(engine.set_value("discharge", 1, 3.0));
    EXPECT_NO_THROW(engine.set_value("lateral_discharge", 1, 4.0));
    EXPECT_NO_THROW(engine.set_value("gate_opening", 1, 0.5));
    EXPECT_THROW(engine.set_value("unknown_variable", 1, 1.0), scau::coupling::river::DFlowFMEngineError);
    EXPECT_THROW(static_cast<void>(engine.get_value("unknown_variable", 1)), scau::coupling::river::DFlowFMEngineError);

    const scau::coupling::core::ExchangeDecision decision{
        .q_granted = 1.0,
        .v_granted = 1.0,
        .q_repay = 0.0,
        .v_repay = 0.0,
        .v_unmet = 0.0,
    };
    EXPECT_THROW(
        scau::coupling::river::accept_dflowfm_exchange_decision(engine, "unknown_variable", 1, decision, 1.0),
        scau::coupling::river::DFlowFMEngineError);
}

TEST(CouplingAdapterBoundaries, DFlowFMMockRejectsOutOfRangeGateOpening) {
    scau::coupling::river::MockDFlowFMEngine engine;
    engine.initialize("mock.mdu");

    EXPECT_NO_THROW(engine.set_value("gate_opening", 1, 0.0));
    EXPECT_NO_THROW(engine.set_value("gate_opening", 1, 1.0));
    EXPECT_THROW(engine.set_value("gate_opening", 1, -0.1), scau::coupling::river::DFlowFMEngineError);
    EXPECT_THROW(engine.set_value("gate_opening", 1, 1.1), scau::coupling::river::DFlowFMEngineError);
}

TEST(CouplingAdapterBoundaries, DFlowFMMockRejectsNonFiniteInputs) {
    scau::coupling::river::MockDFlowFMEngine engine;
    engine.initialize("mock.mdu");

    EXPECT_THROW(engine.update(std::numeric_limits<double>::infinity()), scau::coupling::river::DFlowFMEngineError);
    EXPECT_THROW(engine.set_value("lateral_discharge", 1, std::numeric_limits<double>::quiet_NaN()), scau::coupling::river::DFlowFMEngineError);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::river::make_dflowfm_exchange_request(
            std::numeric_limits<double>::infinity(),
            1.0)),
        scau::coupling::river::DFlowFMEngineError);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::river::make_dflowfm_exchange_request(
            1.0,
            std::numeric_limits<double>::quiet_NaN())),
        scau::coupling::river::DFlowFMEngineError);
}

TEST(CouplingAdapterBoundaries, AdapterAcceptRejectsNonFiniteDecisionFlows) {
    scau::coupling::drainage::MockSwmmEngine swmm;
    scau::coupling::river::MockDFlowFMEngine dflowfm;
    swmm.initialize("mock.inp");
    dflowfm.initialize("mock.mdu");

    const scau::coupling::core::ExchangeDecision bad_decision{
        .q_granted = std::numeric_limits<double>::infinity(),
        .q_repay = 0.0,
    };

    EXPECT_THROW(
        scau::coupling::drainage::accept_swmm_exchange_decision(swmm, 1, bad_decision, 1.0),
        scau::coupling::drainage::SwmmEngineError);
    EXPECT_THROW(
        scau::coupling::river::accept_dflowfm_exchange_decision(dflowfm, "lateral_discharge", 1, bad_decision, 1.0),
        scau::coupling::river::DFlowFMEngineError);
}

TEST(CouplingAdapterBoundaries, AdapterAcceptRejectsNegativeDecisionFlows) {
    scau::coupling::drainage::MockSwmmEngine swmm;
    scau::coupling::river::MockDFlowFMEngine dflowfm;
    swmm.initialize("mock.inp");
    dflowfm.initialize("mock.mdu");

    const scau::coupling::core::ExchangeDecision negative_granted{
        .q_granted = -1.0,
        .q_repay = 0.0,
    };
    const scau::coupling::core::ExchangeDecision negative_repay{
        .q_granted = 0.0,
        .q_repay = -1.0,
    };

    EXPECT_THROW(
        scau::coupling::drainage::accept_swmm_exchange_decision(swmm, 1, negative_granted, 1.0),
        scau::coupling::drainage::SwmmEngineError);
    EXPECT_THROW(
        scau::coupling::drainage::accept_swmm_exchange_decision(swmm, 1, negative_repay, 1.0),
        scau::coupling::drainage::SwmmEngineError);
    EXPECT_THROW(
        scau::coupling::river::accept_dflowfm_exchange_decision(dflowfm, "lateral_discharge", 1, negative_granted, 1.0),
        scau::coupling::river::DFlowFMEngineError);
    EXPECT_THROW(
        scau::coupling::river::accept_dflowfm_exchange_decision(dflowfm, "lateral_discharge", 1, negative_repay, 1.0),
        scau::coupling::river::DFlowFMEngineError);
}

TEST(CouplingAdapterBoundaries, AdapterAcceptRejectsInvalidDecisionVolumeFields) {
    scau::coupling::drainage::MockSwmmEngine swmm;
    scau::coupling::river::MockDFlowFMEngine dflowfm;
    swmm.initialize("mock.inp");
    dflowfm.initialize("mock.mdu");

    const scau::coupling::core::ExchangeDecision nonfinite_volume{
        .q_granted = 1.0,
        .v_granted = std::numeric_limits<double>::infinity(),
        .q_repay = 0.0,
        .v_repay = 0.0,
        .v_unmet = 0.0,
    };
    const scau::coupling::core::ExchangeDecision negative_unmet{
        .q_granted = 1.0,
        .v_granted = 1.0,
        .q_repay = 0.0,
        .v_repay = 0.0,
        .v_unmet = -1.0,
    };

    EXPECT_THROW(
        scau::coupling::drainage::accept_swmm_exchange_decision(swmm, 1, nonfinite_volume, 1.0),
        scau::coupling::drainage::SwmmEngineError);
    EXPECT_THROW(
        scau::coupling::drainage::accept_swmm_exchange_decision(swmm, 1, negative_unmet, 1.0),
        scau::coupling::drainage::SwmmEngineError);
    EXPECT_THROW(
        scau::coupling::river::accept_dflowfm_exchange_decision(dflowfm, "lateral_discharge", 1, nonfinite_volume, 1.0),
        scau::coupling::river::DFlowFMEngineError);
    EXPECT_THROW(
        scau::coupling::river::accept_dflowfm_exchange_decision(dflowfm, "lateral_discharge", 1, negative_unmet, 1.0),
        scau::coupling::river::DFlowFMEngineError);
}

TEST(CouplingAdapterBoundaries, AdapterAcceptRejectsFlowVolumeInconsistentDecisions) {
    scau::coupling::drainage::MockSwmmEngine swmm;
    scau::coupling::river::MockDFlowFMEngine dflowfm;
    swmm.initialize("mock.inp");
    dflowfm.initialize("mock.mdu");

    const scau::coupling::core::ExchangeDecision inconsistent_granted{
        .q_granted = 1.0,
        .v_granted = 2.0,
        .q_repay = 0.0,
        .v_repay = 0.0,
        .v_unmet = 0.0,
    };
    const scau::coupling::core::ExchangeDecision inconsistent_repay{
        .q_granted = 1.0,
        .v_granted = 1.0,
        .q_repay = 0.5,
        .v_repay = 2.0,
        .v_unmet = 0.0,
    };

    EXPECT_THROW(
        scau::coupling::drainage::accept_swmm_exchange_decision(swmm, 1, inconsistent_granted, 1.0),
        scau::coupling::drainage::SwmmEngineError);
    EXPECT_THROW(
        scau::coupling::drainage::accept_swmm_exchange_decision(swmm, 1, inconsistent_repay, 1.0),
        scau::coupling::drainage::SwmmEngineError);
    EXPECT_THROW(
        scau::coupling::river::accept_dflowfm_exchange_decision(dflowfm, "lateral_discharge", 1, inconsistent_granted, 1.0),
        scau::coupling::river::DFlowFMEngineError);
    EXPECT_THROW(
        scau::coupling::river::accept_dflowfm_exchange_decision(dflowfm, "lateral_discharge", 1, inconsistent_repay, 1.0),
        scau::coupling::river::DFlowFMEngineError);
}

TEST(CouplingAdapterBoundaries, DFlowFMAcceptRejectsNonLateralDischargeTargets) {
    scau::coupling::river::MockDFlowFMEngine dflowfm;
    dflowfm.initialize("mock.mdu");

    const scau::coupling::core::ExchangeDecision decision{
        .q_granted = 1.0,
        .v_granted = 1.0,
        .q_repay = 0.0,
        .v_repay = 0.0,
        .v_unmet = 0.0,
    };

    EXPECT_THROW(
        scau::coupling::river::accept_dflowfm_exchange_decision(dflowfm, "water_level", 1, decision, 1.0),
        scau::coupling::river::DFlowFMEngineError);
    EXPECT_THROW(
        scau::coupling::river::accept_dflowfm_exchange_decision(dflowfm, "discharge", 1, decision, 1.0),
        scau::coupling::river::DFlowFMEngineError);
    EXPECT_THROW(
        scau::coupling::river::accept_dflowfm_exchange_decision(dflowfm, "gate_opening", 1, decision, 1.0),
        scau::coupling::river::DFlowFMEngineError);
}

TEST(CouplingAdapterBoundaries, EngineErrorsExposeMachineReadableContext) {
    const scau::coupling::drainage::SwmmEngineError swmm_error(
        "bad SWMM call",
        "SWMM",
        "invalid_node_id");
    const scau::coupling::river::DFlowFMEngineError dflowfm_error(
        "bad D-Flow FM call",
        "DFlowFM",
        "unsupported_variable");

    EXPECT_STREQ(swmm_error.what(), "bad SWMM call");
    EXPECT_EQ(swmm_error.engine_id(), "SWMM");
    EXPECT_EQ(swmm_error.error_code(), "invalid_node_id");
    EXPECT_STREQ(dflowfm_error.what(), "bad D-Flow FM call");
    EXPECT_EQ(dflowfm_error.engine_id(), "DFlowFM");
    EXPECT_EQ(dflowfm_error.error_code(), "unsupported_variable");
}

TEST(CouplingAdapterBoundaries, EngineErrorsHaveDefaultMachineReadableContext) {
    const scau::coupling::drainage::SwmmEngineError swmm_error("bad SWMM call");
    const scau::coupling::river::DFlowFMEngineError dflowfm_error("bad D-Flow FM call");

    EXPECT_EQ(swmm_error.engine_id(), "SWMM");
    EXPECT_EQ(swmm_error.error_code(), "swmm_engine_error");
    EXPECT_EQ(dflowfm_error.engine_id(), "DFlowFM");
    EXPECT_EQ(dflowfm_error.error_code(), "dflowfm_engine_error");
}

TEST(CouplingAdapterBoundaries, EngineInterfacesDoNotCarryCoreRuntimeSemantics) {
    using scau::coupling::drainage::ISwmmEngine;
    using scau::coupling::river::IDFlowFMEngine;

    EXPECT_FALSE(HasRollback<ISwmmEngine>::value);
    EXPECT_FALSE(HasRollback<IDFlowFMEngine>::value);
    EXPECT_FALSE(HasReplayPending<ISwmmEngine>::value);
    EXPECT_FALSE(HasReplayPending<IDFlowFMEngine>::value);
    EXPECT_FALSE(HasMassDeficitAccountMember<ISwmmEngine>::value);
    EXPECT_FALSE(HasMassDeficitAccountMember<IDFlowFMEngine>::value);
    EXPECT_FALSE(HasQLimitMember<ISwmmEngine>::value);
    EXPECT_FALSE(HasQLimitMember<IDFlowFMEngine>::value);
    EXPECT_FALSE(HasVLimitMember<ISwmmEngine>::value);
    EXPECT_FALSE(HasVLimitMember<IDFlowFMEngine>::value);
    EXPECT_FALSE(HasAcceptExchangeDecision<ISwmmEngine>::value);
    EXPECT_FALSE(HasAcceptExchangeDecision<IDFlowFMEngine>::value);
}

#include "coupling/drainage/swmm_boundary.hpp"
#include "coupling/drainage/roof_drainage_adapter.hpp"

#include <gtest/gtest.h>

using scau::coupling::drainage::MockSwmmEngine;
using scau::coupling::drainage::SwmmEngineError;
using scau::coupling::drainage::SwmmRoofDrainageAcceptanceAdapter;
using scau::surface2d::RoofDrainageIntent;
using scau::surface2d::RoofDrainageRejectionReason;

TEST(SwmmRoofDrainageAcceptanceAdapter, AcceptedVolumeWritesLateralInflowRate) {
    MockSwmmEngine engine;
    engine.initialize("mock.inp");
    SwmmRoofDrainageAcceptanceAdapter adapter(engine, 2.0);
    const RoofDrainageIntent intent{.source_cell_index = 3, .target_swmm_node_index = 0, .requested_volume = 0.8, .source_roof_area = 10.0};

    const auto accepted = adapter(intent);

    EXPECT_NEAR(accepted.requested_volume, 0.8, 1.0e-12);
    EXPECT_NEAR(accepted.accepted_volume, 0.8, 1.0e-12);
    EXPECT_NEAR(accepted.rejected_volume, 0.0, 1.0e-12);
    EXPECT_EQ(accepted.rejection_reason, RoofDrainageRejectionReason::None);
    EXPECT_NEAR(engine.get_node_lateral_inflow(0), 0.4, 1.0e-12);
}

TEST(SwmmRoofDrainageAcceptanceAdapter, MultipleRoofIntentsAccumulatePerNodeWithinStep) {
    MockSwmmEngine engine;
    engine.initialize("mock.inp");
    SwmmRoofDrainageAcceptanceAdapter adapter(engine, 2.0);

    const auto first = adapter(RoofDrainageIntent{.source_cell_index = 0, .target_swmm_node_index = 0, .requested_volume = 0.5, .source_roof_area = 1.0});
    const auto second = adapter(RoofDrainageIntent{.source_cell_index = 1, .target_swmm_node_index = 0, .requested_volume = 0.25, .source_roof_area = 1.0});

    EXPECT_EQ(first.rejection_reason, RoofDrainageRejectionReason::None);
    EXPECT_EQ(second.rejection_reason, RoofDrainageRejectionReason::None);
    EXPECT_NEAR(engine.get_node_lateral_inflow(0), 0.375, 1.0e-12);
}

TEST(SwmmRoofDrainageAcceptanceAdapter, BeginStepClearsPerNodeAccumulation) {
    MockSwmmEngine engine;
    engine.initialize("mock.inp");
    SwmmRoofDrainageAcceptanceAdapter adapter(engine, 2.0);

    (void)adapter(RoofDrainageIntent{.source_cell_index = 0, .target_swmm_node_index = 0, .requested_volume = 0.5, .source_roof_area = 1.0});
    EXPECT_NEAR(engine.get_node_lateral_inflow(0), 0.25, 1.0e-12);

    adapter.begin_step();
    (void)adapter(RoofDrainageIntent{.source_cell_index = 0, .target_swmm_node_index = 0, .requested_volume = 0.25, .source_roof_area = 1.0});

    EXPECT_NEAR(engine.get_node_lateral_inflow(0), 0.125, 1.0e-12);
}

TEST(SwmmRoofDrainageAcceptanceAdapter, BeginStepZeroesStaleNodesNotRewritten) {
    MockSwmmEngine engine;
    engine.initialize("mock.inp");
    SwmmRoofDrainageAcceptanceAdapter adapter(engine, 2.0);

    (void)adapter(RoofDrainageIntent{.source_cell_index = 0, .target_swmm_node_index = 0, .requested_volume = 0.5, .source_roof_area = 1.0});
    EXPECT_NEAR(engine.get_node_lateral_inflow(0), 0.25, 1.0e-12);

    // A node not re-written after begin_step must not keep routing stale
    // flow: the SWMM API inflow buffer persists until overwritten.
    adapter.begin_step();
    EXPECT_NEAR(engine.get_node_lateral_inflow(0), 0.0, 1.0e-12);
}

TEST(SwmmRoofDrainageAcceptanceAdapter, InvalidNodeRejectsFailClosedAndDoesNotWrite) {
    MockSwmmEngine engine;
    engine.initialize("mock.inp");
    SwmmRoofDrainageAcceptanceAdapter adapter(engine, 1.0);

    const auto accepted = adapter(RoofDrainageIntent{.source_cell_index = 0, .target_swmm_node_index = -1, .requested_volume = 0.5, .source_roof_area = 1.0});

    EXPECT_NEAR(accepted.accepted_volume, 0.0, 1.0e-12);
    EXPECT_NEAR(accepted.rejected_volume, 0.5, 1.0e-12);
    EXPECT_EQ(accepted.rejection_reason, RoofDrainageRejectionReason::InvalidTargetNode);
    EXPECT_NEAR(engine.get_node_lateral_inflow(0), 0.0, 1.0e-12);
}

TEST(SwmmRoofDrainageAcceptanceAdapter, SurchargedNodeRejectsWithoutWriting) {
    MockSwmmEngine engine;
    engine.initialize("mock.inp");
    engine.set_node_surcharge_fixture(0, true);
    SwmmRoofDrainageAcceptanceAdapter adapter(engine, 1.0);

    const auto accepted = adapter(RoofDrainageIntent{.source_cell_index = 0, .target_swmm_node_index = 0, .requested_volume = 0.5, .source_roof_area = 1.0});

    EXPECT_NEAR(accepted.accepted_volume, 0.0, 1.0e-12);
    EXPECT_NEAR(accepted.rejected_volume, 0.5, 1.0e-12);
    EXPECT_EQ(accepted.rejection_reason, RoofDrainageRejectionReason::NodeSurcharged);
    EXPECT_NEAR(engine.get_node_lateral_inflow(0), 0.0, 1.0e-12);
}

// ASSERTION MIGRATION (confluence 2026-07-16): the unified map-based
// MockSwmmEngine has no node-count bound, so "out-of-range node id" cannot
// be expressed on the mock anymore. The real-engine out-of-range coverage
// lives in test_coupling_roof_swmm_real (target node 99 -> fail-closed
// reject). This test keeps the same adapter contract - an engine that
// throws SwmmEngineError while vouching for the node yields a fail-closed
// InvalidTargetNode rejection with no write - via an uninitialized engine.
TEST(SwmmRoofDrainageAcceptanceAdapter, EngineErrorOnNodeQueryRejectsFailClosed) {
    MockSwmmEngine engine;
    SwmmRoofDrainageAcceptanceAdapter adapter(engine, 1.0);

    const auto accepted = adapter(RoofDrainageIntent{.source_cell_index = 0, .target_swmm_node_index = 3, .requested_volume = 0.5, .source_roof_area = 1.0});

    EXPECT_NEAR(accepted.accepted_volume, 0.0, 1.0e-12);
    EXPECT_NEAR(accepted.rejected_volume, 0.5, 1.0e-12);
    EXPECT_EQ(accepted.rejection_reason, RoofDrainageRejectionReason::InvalidTargetNode);

    engine.initialize("mock.inp");
    EXPECT_NEAR(engine.get_node_lateral_inflow(3), 0.0, 1.0e-12);
}

TEST(SwmmRoofDrainageAcceptanceAdapter, InvalidRequestedVolumeRejectsFailClosed) {
    MockSwmmEngine engine;
    engine.initialize("mock.inp");
    SwmmRoofDrainageAcceptanceAdapter adapter(engine, 1.0);

    const auto accepted = adapter(RoofDrainageIntent{.source_cell_index = 0, .target_swmm_node_index = 0, .requested_volume = -0.5, .source_roof_area = 1.0});

    EXPECT_NEAR(accepted.accepted_volume, 0.0, 1.0e-12);
    EXPECT_NEAR(accepted.rejected_volume, 0.5, 1.0e-12);
    EXPECT_EQ(accepted.rejection_reason, RoofDrainageRejectionReason::EngineUnavailable);
    EXPECT_NEAR(engine.get_node_lateral_inflow(0), 0.0, 1.0e-12);
}

TEST(SwmmRoofDrainageAcceptanceAdapter, RejectsInvalidDtAtConstruction) {
    MockSwmmEngine engine;
    engine.initialize("mock.inp");

    EXPECT_THROW(SwmmRoofDrainageAcceptanceAdapter(engine, 0.0), SwmmEngineError);
}

#include "coupling/drainage/mock_swmm_engine.hpp"
#include "coupling/drainage/roof_drainage_adapter.hpp"

#include <gtest/gtest.h>

using scau::coupling::drainage::MockSwmmEngine;
using scau::coupling::drainage::SwmmEngineError;
using scau::coupling::drainage::SwmmRoofDrainageAcceptanceAdapter;
using scau::surface2d::RoofDrainageIntent;
using scau::surface2d::RoofDrainageRejectionReason;

TEST(SwmmRoofDrainageAcceptanceAdapter, AcceptedVolumeWritesLateralInflowRate) {
    MockSwmmEngine engine(1U);
    SwmmRoofDrainageAcceptanceAdapter adapter(engine, 2.0);
    const RoofDrainageIntent intent{.source_cell_index = 3, .target_swmm_node_index = 0, .requested_volume = 0.8, .source_roof_area = 10.0};

    const auto accepted = adapter(intent);

    EXPECT_NEAR(accepted.requested_volume, 0.8, 1.0e-12);
    EXPECT_NEAR(accepted.accepted_volume, 0.8, 1.0e-12);
    EXPECT_NEAR(accepted.rejected_volume, 0.0, 1.0e-12);
    EXPECT_EQ(accepted.rejection_reason, RoofDrainageRejectionReason::None);
    EXPECT_NEAR(engine.get_node_lateral_inflow(0U), 0.4, 1.0e-12);
}

TEST(SwmmRoofDrainageAcceptanceAdapter, MultipleRoofIntentsAccumulatePerNodeWithinStep) {
    MockSwmmEngine engine(1U);
    SwmmRoofDrainageAcceptanceAdapter adapter(engine, 2.0);

    const auto first = adapter(RoofDrainageIntent{.source_cell_index = 0, .target_swmm_node_index = 0, .requested_volume = 0.5, .source_roof_area = 1.0});
    const auto second = adapter(RoofDrainageIntent{.source_cell_index = 1, .target_swmm_node_index = 0, .requested_volume = 0.25, .source_roof_area = 1.0});

    EXPECT_EQ(first.rejection_reason, RoofDrainageRejectionReason::None);
    EXPECT_EQ(second.rejection_reason, RoofDrainageRejectionReason::None);
    EXPECT_NEAR(engine.get_node_lateral_inflow(0U), 0.375, 1.0e-12);
}

TEST(SwmmRoofDrainageAcceptanceAdapter, BeginStepClearsPerNodeAccumulation) {
    MockSwmmEngine engine(1U);
    SwmmRoofDrainageAcceptanceAdapter adapter(engine, 2.0);

    (void)adapter(RoofDrainageIntent{.source_cell_index = 0, .target_swmm_node_index = 0, .requested_volume = 0.5, .source_roof_area = 1.0});
    EXPECT_NEAR(engine.get_node_lateral_inflow(0U), 0.25, 1.0e-12);

    adapter.begin_step();
    (void)adapter(RoofDrainageIntent{.source_cell_index = 0, .target_swmm_node_index = 0, .requested_volume = 0.25, .source_roof_area = 1.0});

    EXPECT_NEAR(engine.get_node_lateral_inflow(0U), 0.125, 1.0e-12);
}

TEST(SwmmRoofDrainageAcceptanceAdapter, InvalidNodeRejectsFailClosedAndDoesNotWrite) {
    MockSwmmEngine engine(1U);
    SwmmRoofDrainageAcceptanceAdapter adapter(engine, 1.0);

    const auto accepted = adapter(RoofDrainageIntent{.source_cell_index = 0, .target_swmm_node_index = -1, .requested_volume = 0.5, .source_roof_area = 1.0});

    EXPECT_NEAR(accepted.accepted_volume, 0.0, 1.0e-12);
    EXPECT_NEAR(accepted.rejected_volume, 0.5, 1.0e-12);
    EXPECT_EQ(accepted.rejection_reason, RoofDrainageRejectionReason::InvalidTargetNode);
    EXPECT_NEAR(engine.get_node_lateral_inflow(0U), 0.0, 1.0e-12);
}

TEST(SwmmRoofDrainageAcceptanceAdapter, SurchargedNodeRejectsWithoutWriting) {
    MockSwmmEngine engine(1U);
    engine.set_surcharged(0U, true);
    SwmmRoofDrainageAcceptanceAdapter adapter(engine, 1.0);

    const auto accepted = adapter(RoofDrainageIntent{.source_cell_index = 0, .target_swmm_node_index = 0, .requested_volume = 0.5, .source_roof_area = 1.0});

    EXPECT_NEAR(accepted.accepted_volume, 0.0, 1.0e-12);
    EXPECT_NEAR(accepted.rejected_volume, 0.5, 1.0e-12);
    EXPECT_EQ(accepted.rejection_reason, RoofDrainageRejectionReason::NodeSurcharged);
    EXPECT_NEAR(engine.get_node_lateral_inflow(0U), 0.0, 1.0e-12);
}

TEST(SwmmRoofDrainageAcceptanceAdapter, OutOfRangeNodeRejectsFailClosed) {
    MockSwmmEngine engine(1U);
    SwmmRoofDrainageAcceptanceAdapter adapter(engine, 1.0);

    const auto accepted = adapter(RoofDrainageIntent{.source_cell_index = 0, .target_swmm_node_index = 3, .requested_volume = 0.5, .source_roof_area = 1.0});

    EXPECT_NEAR(accepted.accepted_volume, 0.0, 1.0e-12);
    EXPECT_NEAR(accepted.rejected_volume, 0.5, 1.0e-12);
    EXPECT_EQ(accepted.rejection_reason, RoofDrainageRejectionReason::InvalidTargetNode);
    EXPECT_NEAR(engine.get_node_lateral_inflow(0U), 0.0, 1.0e-12);
}

TEST(SwmmRoofDrainageAcceptanceAdapter, InvalidRequestedVolumeRejectsFailClosed) {
    MockSwmmEngine engine(1U);
    SwmmRoofDrainageAcceptanceAdapter adapter(engine, 1.0);

    const auto accepted = adapter(RoofDrainageIntent{.source_cell_index = 0, .target_swmm_node_index = 0, .requested_volume = -0.5, .source_roof_area = 1.0});

    EXPECT_NEAR(accepted.accepted_volume, 0.0, 1.0e-12);
    EXPECT_NEAR(accepted.rejected_volume, 0.5, 1.0e-12);
    EXPECT_EQ(accepted.rejection_reason, RoofDrainageRejectionReason::EngineUnavailable);
    EXPECT_NEAR(engine.get_node_lateral_inflow(0U), 0.0, 1.0e-12);
}

TEST(SwmmRoofDrainageAcceptanceAdapter, RejectsInvalidDtAtConstruction) {
    MockSwmmEngine engine(1U);

    EXPECT_THROW(SwmmRoofDrainageAcceptanceAdapter(engine, 0.0), SwmmEngineError);
}

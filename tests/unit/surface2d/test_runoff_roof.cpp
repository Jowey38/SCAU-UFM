#include <stdexcept>

#include <gtest/gtest.h>

#include "surface2d/source_terms/runoff/runoff_generation.hpp"

namespace {

using scau::surface2d::evaluate_roof_emit;
using scau::surface2d::RunoffCellInputs;
using scau::surface2d::RunoffCellParams;
using scau::surface2d::RunoffCellState;

}  // namespace

TEST(RoofEmit, AbstractionThenPendingAccumulates) {
    RunoffCellInputs in{.rainfall_rate = 1.0e-3, .phi_t = 1.0, .cell_area = 100.0};
    RunoffCellParams p{.pervious_fraction = 0.0, .roof_fraction = 1.0,
                       .roof_abstraction_capacity = 2.0e-3, .roof_drain_capacity = 1.0};
    RunoffCellState s{};
    const auto r = evaluate_roof_emit(in, p, s, 10.0);
    EXPECT_NEAR(r.roof_abstraction_volume, 2.0e-3 * 100.0, 1.0e-9);
    EXPECT_NEAR(r.roof_input_volume, 8.0e-3 * 100.0, 1.0e-9);
    EXPECT_NEAR(s.roof_pending_volume, 0.8, 1.0e-9);
    EXPECT_NEAR(r.requested_volume, 0.8, 1.0e-9);
    EXPECT_FALSE(r.drain_capacity_limited);
}

TEST(RoofEmit, DrainRequestCappedByCapacity) {
    RunoffCellInputs in{.rainfall_rate = 1.0e-3, .phi_t = 1.0, .cell_area = 100.0};
    RunoffCellParams p{.pervious_fraction = 0.0, .roof_fraction = 1.0,
                       .roof_abstraction_capacity = 2.0e-3, .roof_drain_capacity = 0.05};
    RunoffCellState s{};
    const auto r = evaluate_roof_emit(in, p, s, 10.0);
    EXPECT_NEAR(r.requested_volume, 0.5, 1.0e-9);
    EXPECT_TRUE(r.drain_capacity_limited);
    EXPECT_NEAR(s.roof_pending_volume, 0.8, 1.0e-9);
}

TEST(RoofEmit, ZeroRoofFractionEmitsNothing) {
    RunoffCellInputs in{.rainfall_rate = 1.0e-3, .phi_t = 1.0, .cell_area = 100.0};
    RunoffCellParams p{.pervious_fraction = 1.0, .roof_fraction = 0.0, .roof_drain_capacity = 1.0};
    RunoffCellState s{};
    const auto r = evaluate_roof_emit(in, p, s, 10.0);
    EXPECT_EQ(r.roof_input_volume, 0.0);
    EXPECT_EQ(r.requested_volume, 0.0);
    EXPECT_EQ(s.roof_pending_volume, 0.0);
}

TEST(RoofEmit, InvalidRoofDrainCapacityFailsClosed) {
    RunoffCellInputs in{.rainfall_rate = 1.0e-3, .phi_t = 1.0, .cell_area = 100.0};
    RunoffCellParams p{.pervious_fraction = 0.0, .roof_fraction = 1.0, .roof_drain_capacity = -1.0};
    RunoffCellState s{};
    EXPECT_THROW(static_cast<void>(evaluate_roof_emit(in, p, s, 10.0)), std::invalid_argument);
}

// --- acceptance application tests ---

TEST(RoofAcceptance, FullAcceptDrainsPending) {
    using scau::surface2d::apply_roof_drainage_acceptance;
    using scau::surface2d::RoofDrainageAcceptance;
    scau::surface2d::RunoffCellInputs in{.rainfall_rate = 0.0, .phi_t = 1.0, .cell_area = 100.0};
    scau::surface2d::RunoffCellParams p{.pervious_fraction = 0.0, .roof_fraction = 1.0,
                                        .roof_storage_capacity = 1.0e-2};
    scau::surface2d::RunoffCellState s{};
    s.roof_pending_volume = 0.8;
    RoofDrainageAcceptance acc{.requested_volume = 0.5, .accepted_volume = 0.5, .rejected_volume = 0.0};
    const auto r = apply_roof_drainage_acceptance(in, p, s, acc, true);
    EXPECT_NEAR(r.accepted_volume, 0.5, 1.0e-12);
    EXPECT_NEAR(s.roof_pending_volume, 0.3, 1.0e-12);
    EXPECT_NEAR(r.pending_delta_volume, -0.5, 1.0e-12);
    EXPECT_FALSE(r.overflowed);
}

TEST(RoofAcceptance, RejectionKeepsWaterInPending) {
    using scau::surface2d::apply_roof_drainage_acceptance;
    using scau::surface2d::RoofDrainageAcceptance;
    using scau::surface2d::RoofDrainageRejectionReason;
    scau::surface2d::RunoffCellInputs in{.rainfall_rate = 0.0, .phi_t = 1.0, .cell_area = 100.0};
    scau::surface2d::RunoffCellParams p{.pervious_fraction = 0.0, .roof_fraction = 1.0,
                                        .roof_storage_capacity = 1.0e-2};
    scau::surface2d::RunoffCellState s{};
    s.roof_pending_volume = 0.8;
    RoofDrainageAcceptance acc{.requested_volume = 0.5, .accepted_volume = 0.0, .rejected_volume = 0.5,
                               .rejection_reason = RoofDrainageRejectionReason::NodeSurcharged};
    const auto r = apply_roof_drainage_acceptance(in, p, s, acc, true);
    EXPECT_NEAR(r.accepted_volume, 0.0, 1.0e-12);
    EXPECT_NEAR(r.rejected_volume, 0.5, 1.0e-12);
    EXPECT_TRUE(r.node_rejected);
    EXPECT_NEAR(s.roof_pending_volume, 0.8, 1.0e-12);
    EXPECT_FALSE(r.overflowed);
}

TEST(RoofAcceptance, OverflowWhenPendingExceedsStorage) {
    using scau::surface2d::apply_roof_drainage_acceptance;
    using scau::surface2d::RoofDrainageAcceptance;
    scau::surface2d::RunoffCellInputs in{.rainfall_rate = 0.0, .phi_t = 1.0, .cell_area = 100.0};
    scau::surface2d::RunoffCellParams p{.pervious_fraction = 0.0, .roof_fraction = 1.0,
                                        .roof_storage_capacity = 5.0e-3};
    scau::surface2d::RunoffCellState s{};
    s.roof_pending_volume = 0.8;
    RoofDrainageAcceptance acc{.requested_volume = 0.1, .accepted_volume = 0.1, .rejected_volume = 0.0};
    const auto r = apply_roof_drainage_acceptance(in, p, s, acc, true);
    EXPECT_NEAR(r.overflow_to_surface_volume, 0.2, 1.0e-12);
    EXPECT_NEAR(s.roof_pending_volume, 0.5, 1.0e-12);
    EXPECT_TRUE(r.overflowed);
    EXPECT_TRUE(r.pending_saturated);
}

TEST(RoofAcceptance, MissingOverflowTargetFailsClosedRetainsWater) {
    using scau::surface2d::apply_roof_drainage_acceptance;
    using scau::surface2d::RoofDrainageAcceptance;
    scau::surface2d::RunoffCellInputs in{.rainfall_rate = 0.0, .phi_t = 1.0, .cell_area = 100.0};
    scau::surface2d::RunoffCellParams p{.pervious_fraction = 0.0, .roof_fraction = 1.0,
                                        .roof_storage_capacity = 5.0e-3};
    scau::surface2d::RunoffCellState s{};
    s.roof_pending_volume = 0.8;
    RoofDrainageAcceptance acc{.requested_volume = 0.0, .accepted_volume = 0.0, .rejected_volume = 0.0};
    const auto r = apply_roof_drainage_acceptance(in, p, s, acc, false);
    EXPECT_TRUE(r.missing_overflow_target);
    EXPECT_FALSE(r.overflowed);
    EXPECT_NEAR(r.overflow_to_surface_volume, 0.0, 1.0e-12);
    EXPECT_NEAR(s.roof_pending_volume, 0.8, 1.0e-12);
}

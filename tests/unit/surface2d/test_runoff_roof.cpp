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

#include <stdexcept>

#include <gtest/gtest.h>

#include "surface2d/source_terms/runoff/runoff_generation.hpp"

namespace {

using scau::surface2d::evaluate_ground_runoff;
using scau::surface2d::RunoffCellInputs;
using scau::surface2d::RunoffCellParams;
using scau::surface2d::RunoffCellState;
using scau::surface2d::SoilParams;

SoilParams sample_soil() {
    return SoilParams{.k_s = 1.0e-5, .psi_f = 0.1, .theta_s = 0.4, .theta_i = 0.1};
}

}  // namespace

TEST(GroundRunoff, PerviousNoLossAllInfiltratesBelowCapacity) {
    RunoffCellInputs in{.rainfall_rate = 1.0e-3, .phi_t = 1.0, .cell_area = 100.0};
    RunoffCellParams p{.pervious_fraction = 1.0, .impervious_fraction = 0.0, .soil = sample_soil()};
    RunoffCellState s{};
    const auto g = evaluate_ground_runoff(in, p, s, 10.0, 1.0e-4);
    EXPECT_NEAR(g.infiltration_volume, 1.0e-2 * 100.0, 1.0e-9);
    EXPECT_NEAR(g.surface_added_volume, 0.0, 1.0e-9);
    EXPECT_NEAR(s.cumulative_infiltration, 1.0e-2, 1.0e-9);
}

TEST(GroundRunoff, ImperviousAllBecomesRunoffNoInfiltration) {
    RunoffCellInputs in{.rainfall_rate = 1.0e-3, .phi_t = 1.0, .cell_area = 100.0};
    RunoffCellParams p{.pervious_fraction = 0.0, .impervious_fraction = 1.0, .soil = sample_soil()};
    RunoffCellState s{};
    const auto g = evaluate_ground_runoff(in, p, s, 10.0, 1.0e-4);
    EXPECT_NEAR(g.infiltration_volume, 0.0, 1.0e-12);
    EXPECT_NEAR(g.surface_added_volume, 1.0e-2 * 100.0, 1.0e-9);
    EXPECT_EQ(s.cumulative_infiltration, 0.0);
}

TEST(GroundRunoff, AbstractionThenDepressionConsumedFirst) {
    RunoffCellInputs in{.rainfall_rate = 1.0e-3, .phi_t = 1.0, .cell_area = 100.0};
    RunoffCellParams p{.pervious_fraction = 0.0, .impervious_fraction = 1.0,
                       .initial_abstraction_capacity = 3.0e-3, .depression_storage_capacity = 2.0e-3,
                       .soil = sample_soil()};
    RunoffCellState s{};
    const auto g = evaluate_ground_runoff(in, p, s, 10.0, 1.0e-4);
    EXPECT_NEAR(g.abstraction_volume, 3.0e-3 * 100.0, 1.0e-9);
    EXPECT_NEAR(g.depression_storage_delta_volume, 2.0e-3 * 100.0, 1.0e-9);
    EXPECT_NEAR(g.surface_added_volume, 5.0e-3 * 100.0, 1.0e-9);
    EXPECT_NEAR(s.abstraction_filled, 3.0e-3, 1.0e-12);
    EXPECT_NEAR(s.depression_storage_filled, 2.0e-3, 1.0e-12);
}

TEST(GroundRunoff, GroundChainConservesMass) {
    RunoffCellInputs in{.rainfall_rate = 2.0e-3, .phi_t = 1.0, .cell_area = 100.0};
    RunoffCellParams p{.pervious_fraction = 0.5, .impervious_fraction = 0.5,
                       .initial_abstraction_capacity = 1.0e-3, .depression_storage_capacity = 1.0e-3,
                       .soil = sample_soil()};
    RunoffCellState s{};
    const auto g = evaluate_ground_runoff(in, p, s, 5.0, 1.0e-4);
    const double rain_ground = 2.0e-3 * 5.0 * 100.0;
    const double sinks = g.surface_added_volume + g.infiltration_volume +
                         g.abstraction_volume + g.depression_storage_delta_volume;
    EXPECT_NEAR(sinks, rain_ground, 1.0e-9);
}

TEST(GroundRunoff, InvalidFractionFailsClosed) {
    RunoffCellInputs in{.rainfall_rate = 1.0e-3, .phi_t = 1.0, .cell_area = 100.0};
    RunoffCellParams p{.pervious_fraction = 1.5, .impervious_fraction = 0.0, .soil = sample_soil()};
    RunoffCellState s{};
    EXPECT_THROW(static_cast<void>(evaluate_ground_runoff(in, p, s, 10.0, 1.0e-4)), std::invalid_argument);
}

TEST(GroundRunoff, InvalidInputsFailClosed) {
    RunoffCellParams p{.pervious_fraction = 1.0, .soil = sample_soil()};
    RunoffCellState s{};
    EXPECT_THROW(
        static_cast<void>(evaluate_ground_runoff(
            RunoffCellInputs{.rainfall_rate = -1.0, .phi_t = 1.0, .cell_area = 100.0}, p, s, 10.0, 1.0e-4)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(evaluate_ground_runoff(
            RunoffCellInputs{.rainfall_rate = 1.0e-3, .phi_t = 1.0, .cell_area = 100.0}, p, s, 0.0, 1.0e-4)),
        std::invalid_argument);
}

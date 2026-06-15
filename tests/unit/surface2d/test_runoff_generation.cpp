#include <gtest/gtest.h>

#include "surface2d/source_terms/runoff/result.hpp"
#include "surface2d/source_terms/runoff/runoff_generation.hpp"

namespace {

using scau::surface2d::apply_roof_drainage_acceptance;
using scau::surface2d::check_runoff_mass_closure;
using scau::surface2d::evaluate_runoff_generation;
using scau::surface2d::RoofDrainageAcceptance;
using scau::surface2d::RunoffCellInputs;
using scau::surface2d::RunoffCellParams;
using scau::surface2d::RunoffCellState;
using scau::surface2d::SoilParams;

SoilParams sample_soil() {
    return SoilParams{.k_s = 1.0e-5, .psi_f = 0.1, .theta_s = 0.4, .theta_i = 0.1};
}

}  // namespace

TEST(RunoffGeneration, EmitResultClosesBeforeAcceptance) {
    RunoffCellInputs in{.rainfall_rate = 2.0e-3, .phi_t = 0.8, .cell_area = 100.0};
    RunoffCellParams p{.pervious_fraction = 0.4, .impervious_fraction = 0.3, .roof_fraction = 0.3,
                       .initial_abstraction_capacity = 1.0e-3, .depression_storage_capacity = 1.0e-3,
                       .roof_abstraction_capacity = 1.0e-3, .roof_storage_capacity = 1.0e-2,
                       .roof_drain_capacity = 0.05, .soil = sample_soil()};
    RunoffCellState s{};
    auto out = evaluate_runoff_generation(in, p, s, 5.0, 1.0e-4, 7, 3);

    EXPECT_NEAR(out.result.rainfall_volume, 1.0, 1.0e-9);
    EXPECT_EQ(out.intent.source_cell_index, 7);
    EXPECT_EQ(out.intent.target_swmm_node_index, 3);
    EXPECT_NEAR(out.intent.source_roof_area, 30.0, 1.0e-12);
    EXPECT_NEAR(out.intent.requested_volume, out.result.roof_to_swmm_requested_volume, 1.0e-12);
    EXPECT_TRUE(check_runoff_mass_closure(out.result, 1.0e-9, 1.0e-9));
    EXPECT_FALSE(out.result.flags.mass_balance_violation);
}

TEST(RunoffGeneration, ClosesAfterPartialAcceptance) {
    RunoffCellInputs in{.rainfall_rate = 2.0e-3, .phi_t = 0.8, .cell_area = 100.0};
    RunoffCellParams p{.pervious_fraction = 0.4, .impervious_fraction = 0.3, .roof_fraction = 0.3,
                       .initial_abstraction_capacity = 1.0e-3, .depression_storage_capacity = 1.0e-3,
                       .roof_abstraction_capacity = 1.0e-3, .roof_storage_capacity = 1.0e-2,
                       .roof_drain_capacity = 0.05, .soil = sample_soil()};
    RunoffCellState s{};
    auto out = evaluate_runoff_generation(in, p, s, 5.0, 1.0e-4, 7, 3);

    const double requested = out.result.roof_to_swmm_requested_volume;
    RoofDrainageAcceptance acc{.requested_volume = requested,
                               .accepted_volume = 0.5 * requested,
                               .rejected_volume = 0.5 * requested};
    const auto ar = apply_roof_drainage_acceptance(in, p, s, acc, true);

    out.result.roof_to_swmm_accepted_volume = ar.accepted_volume;
    out.result.roof_to_swmm_rejected_volume = ar.rejected_volume;
    out.result.roof_overflow_to_surface_volume = ar.overflow_to_surface_volume;
    out.result.roof_pending_delta_volume += ar.pending_delta_volume;

    EXPECT_TRUE(check_runoff_mass_closure(out.result, 1.0e-9, 1.0e-9));
    EXPECT_FALSE(out.result.flags.mass_balance_violation);
}

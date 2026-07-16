#include <gtest/gtest.h>

#include "surface2d/source_terms/runoff/result.hpp"

namespace {

using scau::surface2d::check_runoff_mass_closure;
using scau::surface2d::runoff_mass_balance_error;
using scau::surface2d::RunoffGenerationResult;

// A result whose sinks exactly account for the rainfall.
RunoffGenerationResult balanced_result() {
    RunoffGenerationResult r;
    r.rainfall_volume = 10.0;
    r.surface_added_volume = 4.0;
    r.infiltration_volume = 2.0;
    r.abstraction_volume = 1.0;
    r.depression_storage_delta_volume = 0.5;
    r.roof_to_swmm_accepted_volume = 1.5;
    r.roof_pending_delta_volume = 0.5;
    r.roof_overflow_to_surface_volume = 0.5;
    r.rejected_fail_closed_volume = 0.0;
    return r;  // sinks sum = 10.0
}

}  // namespace

TEST(RunoffMassClosure, BalancedResultHasZeroError) {
    EXPECT_NEAR(runoff_mass_balance_error(balanced_result()), 0.0, 1.0e-12);
}

TEST(RunoffMassClosure, BalancedResultIsClosed) {
    auto r = balanced_result();
    EXPECT_TRUE(check_runoff_mass_closure(r, 1.0e-9, 1.0e-9));
    EXPECT_FALSE(r.flags.mass_balance_violation);
}

TEST(RunoffMassClosure, ImbalanceIsDetected) {
    auto r = balanced_result();
    r.surface_added_volume += 1.0;  // create a 1 m^3 surplus sink
    EXPECT_NEAR(runoff_mass_balance_error(r), -1.0, 1.0e-12);
    EXPECT_FALSE(check_runoff_mass_closure(r, 1.0e-9, 1.0e-9));
    EXPECT_TRUE(r.flags.mass_balance_violation);
}

TEST(RunoffMassClosure, RelativeToleranceScalesWithRainfall) {
    auto r = balanced_result();
    r.surface_added_volume += 1.0e-6;  // tiny imbalance
    EXPECT_TRUE(check_runoff_mass_closure(r, 0.0, 1.0e-6));
    EXPECT_FALSE(r.flags.mass_balance_violation);
}

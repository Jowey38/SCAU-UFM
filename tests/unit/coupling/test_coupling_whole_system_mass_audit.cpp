#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "coupling/driver/whole_system_mass_audit.hpp"

namespace {

using scau::coupling::driver::DeficitAgeObservation;
using scau::coupling::driver::WholeSystemMassSample;
using scau::coupling::driver::WholeSystemMassTolerance;
using scau::coupling::driver::WholeSystemMassVerdict;
using scau::coupling::driver::audit_whole_system_mass;
using scau::coupling::driver::update_deficit_ages;

WholeSystemMassSample baseline_sample() {
    WholeSystemMassSample sample{};
    sample.epoch = 0U;
    sample.logical_time = 0.0;
    sample.surface_volume = 100.0;
    sample.surface_reference_volume = 100.0;
    sample.coupling_deficit_volume = 0.0;
    sample.swmm_storage_volume = 20.0;
    sample.dflowfm_volume = 80.0;
    sample.swmm_external_net_volume = 0.0;
    sample.dflowfm_external_net_volume = 0.0;
    return sample;
}

}  // namespace

TEST(WholeSystemMassAudit, InternalTransfersDoNotAppearInClosure) {
    const auto baseline = baseline_sample();
    auto current = baseline;
    current.epoch = 1U;
    current.logical_time = 60.0;
    current.surface_volume = 90.0;
    current.swmm_storage_volume = 26.0;
    current.dflowfm_volume = 84.0;

    const auto report = audit_whole_system_mass(baseline, current);
    EXPECT_DOUBLE_EQ(report.baseline_storage_total, 200.0);
    EXPECT_DOUBLE_EQ(report.current_storage_total, 200.0);
    EXPECT_DOUBLE_EQ(report.external_net_volume, 0.0);
    EXPECT_DOUBLE_EQ(report.residual, 0.0);
    EXPECT_DOUBLE_EQ(report.epsilon_deficit, 1.0e-10);
    EXPECT_TRUE(report.conserved);
    EXPECT_EQ(report.verdict, WholeSystemMassVerdict::conserved);
}

TEST(WholeSystemMassAudit, ExternalSourcesSinksAndDepressionStorageClose) {
    const auto baseline = baseline_sample();
    auto current = baseline;
    current.epoch = 2U;
    current.logical_time = 120.0;
    current.surface_volume = 101.5;
    // External net = +5 boundary +2 rain -3 infiltration -1 abstraction = +3.
    current.cumulative_boundary_inflow_volume = 5.0;
    current.cumulative_rainfall_volume = 2.0;
    current.cumulative_infiltration_volume = 3.0;
    current.cumulative_abstraction_volume = 1.0;
    // Depression storage grew by 1.5 m3 and is part of storage_total.
    current.cumulative_depression_storage_delta_volume = 1.5;

    const auto report = audit_whole_system_mass(baseline, current);
    EXPECT_DOUBLE_EQ(report.current_storage_total, 203.0);
    EXPECT_DOUBLE_EQ(report.external_net_volume, 3.0);
    EXPECT_DOUBLE_EQ(report.residual, 0.0);
    EXPECT_TRUE(report.conserved);
}

TEST(WholeSystemMassAudit, DeficitIsAParallelObligationNotPhysicalStorage) {
    const auto baseline = baseline_sample();
    auto current = baseline;
    current.epoch = 1U;
    current.logical_time = 60.0;
    // v_unmet remains physically on the surface; its deficit obligation is
    // reported but must not be added again to storage_total.
    current.coupling_deficit_volume = 4.0;

    const auto report = audit_whole_system_mass(baseline, current);
    EXPECT_DOUBLE_EQ(report.residual, 0.0);
    EXPECT_TRUE(report.conserved);

    // A genuine physical storage loss still requires review regardless of the
    // parallel deficit ledger.
    current.surface_volume = 99.0;
    const auto drifted = audit_whole_system_mass(baseline, current);
    EXPECT_DOUBLE_EQ(drifted.residual, -1.0);
    EXPECT_EQ(drifted.verdict, WholeSystemMassVerdict::review_required);
}

TEST(WholeSystemMassAudit, AppliesCanonicalEpsilonAndDocumentedRealTolerance) {
    auto large = baseline_sample();
    large.surface_volume = 1.0e6;
    large.surface_reference_volume = 1.0e6;
    auto current = large;
    current.epoch = 1U;
    current.logical_time = 60.0;
    current.surface_volume += 5.0e-7;

    const auto strict = audit_whole_system_mass(large, current);
    EXPECT_DOUBLE_EQ(strict.epsilon_deficit, 1.0e-6);
    EXPECT_TRUE(strict.conserved);

    current.surface_volume += 2.0e-6;
    EXPECT_FALSE(audit_whole_system_mass(large, current).conserved);

    WholeSystemMassTolerance real_tolerance{};
    real_tolerance.strict = false;
    real_tolerance.engine_residual_absolute = 1.0e-4;
    real_tolerance.engine_residual_relative = 1.0e-6;
    const auto real = audit_whole_system_mass(large, current, real_tolerance);
    EXPECT_TRUE(real.conserved);
    EXPECT_GT(real.applied_tolerance, strict.applied_tolerance);
}

TEST(WholeSystemMassAudit, MissingExternalEngineScopeRequiresReview) {
    auto baseline = baseline_sample();
    auto current = baseline;
    current.epoch = 1U;
    current.logical_time = 60.0;
    baseline.swmm_external_net_volume.reset();
    current.swmm_external_net_volume.reset();

    const auto report = audit_whole_system_mass(baseline, current);
    EXPECT_DOUBLE_EQ(report.residual, 0.0);
    EXPECT_FALSE(report.scope_complete);
    EXPECT_FALSE(report.conserved);
    EXPECT_EQ(report.verdict, WholeSystemMassVerdict::review_required);
}

TEST(WholeSystemMassAudit, RejectsScopeMismatchAndInvalidInput) {
    const auto baseline = baseline_sample();
    auto missing_swmm = baseline;
    missing_swmm.epoch = 1U;
    missing_swmm.swmm_storage_volume.reset();
    EXPECT_THROW(static_cast<void>(audit_whole_system_mass(baseline, missing_swmm)),
                 std::invalid_argument);

    auto negative = baseline;
    negative.surface_volume = -1.0;
    EXPECT_THROW(static_cast<void>(audit_whole_system_mass(baseline, negative)),
                 std::invalid_argument);

    auto backwards = baseline;
    backwards.logical_time = -1.0;
    EXPECT_THROW(static_cast<void>(audit_whole_system_mass(baseline, backwards)),
                 std::invalid_argument);
}

TEST(WholeSystemMassAudit, TracksDeficitAgeWithoutWritingOff) {
    std::vector<DeficitAgeObservation> ages;
    ages = update_deficit_ages({2.0, 0.0}, ages);
    EXPECT_EQ(ages[0].deficit_age_steps, 1U);
    EXPECT_EQ(ages[1].deficit_age_steps, 0U);

    ages = update_deficit_ages({1.0, 3.0}, ages);
    EXPECT_EQ(ages[0].deficit_age_steps, 2U);
    EXPECT_EQ(ages[1].deficit_age_steps, 1U);

    ages = update_deficit_ages({0.0, 3.0}, ages);
    EXPECT_EQ(ages[0].deficit_age_steps, 0U);
    EXPECT_EQ(ages[1].deficit_age_steps, 2U);
    EXPECT_DOUBLE_EQ(ages[1].volume, 3.0);

    EXPECT_THROW(static_cast<void>(update_deficit_ages({1.0}, ages)),
                 std::invalid_argument);
}

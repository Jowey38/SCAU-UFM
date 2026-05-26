#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "coupling/core/state.hpp"

namespace {

constexpr double kHWet = 1.0e-4;  // §11.4 default

scau::coupling::core::ExchangeCellState wet_cell(
    double h = 2.0,
    double phi_t = 0.4,
    double area = 50.0,
    double deficit = 0.0) {
    return scau::coupling::core::ExchangeCellState{
        .volume = 0.0,
        .mass_deficit_account = {.volume = deficit},
        .phi_t = phi_t,
        .h = h,
        .area = area,
    };
}

}  // namespace

TEST(CouplingSystemMassAudit, EmptyCellListProducesZeroMass) {
    const std::vector<scau::coupling::core::ExchangeCellState> empty;
    const auto audit = scau::coupling::core::compute_system_mass(empty, kHWet);

    EXPECT_DOUBLE_EQ(audit.surface_mass, 0.0);
    EXPECT_DOUBLE_EQ(audit.deficit_mass, 0.0);
    EXPECT_DOUBLE_EQ(audit.total_mass, 0.0);
    EXPECT_EQ(audit.wet_cell_count, 0U);
}

TEST(CouplingSystemMassAudit, SingleWetCellComputesPhiTHAreaProduct) {
    const std::vector cells{wet_cell(/*h=*/2.0, /*phi_t=*/0.4, /*area=*/50.0, /*deficit=*/0.0)};
    const auto audit = scau::coupling::core::compute_system_mass(cells, kHWet);

    EXPECT_DOUBLE_EQ(audit.surface_mass, 40.0);
    EXPECT_DOUBLE_EQ(audit.deficit_mass, 0.0);
    EXPECT_DOUBLE_EQ(audit.total_mass, 40.0);
    EXPECT_EQ(audit.wet_cell_count, 1U);
}

TEST(CouplingSystemMassAudit, DryCellsAreExcludedFromSurfaceMassButDeficitStillCounted) {
    const std::vector cells{
        wet_cell(/*h=*/2.0,        /*phi_t=*/0.4, /*area=*/50.0, /*deficit=*/1.5),
        wet_cell(/*h=*/kHWet / 2.0, /*phi_t=*/0.4, /*area=*/50.0, /*deficit=*/3.0),
    };
    const auto audit = scau::coupling::core::compute_system_mass(cells, kHWet);

    EXPECT_DOUBLE_EQ(audit.surface_mass, 40.0);
    EXPECT_DOUBLE_EQ(audit.deficit_mass, 4.5);
    EXPECT_DOUBLE_EQ(audit.total_mass, 44.5);
    EXPECT_EQ(audit.wet_cell_count, 1U);
}

TEST(CouplingSystemMassAudit, BoundaryHEqualsHWetIsCountedAsWet) {
    const std::vector cells{wet_cell(/*h=*/kHWet, /*phi_t=*/0.4, /*area=*/50.0)};
    const auto audit = scau::coupling::core::compute_system_mass(cells, kHWet);

    EXPECT_DOUBLE_EQ(audit.surface_mass, 0.4 * kHWet * 50.0);
    EXPECT_EQ(audit.wet_cell_count, 1U);
}

TEST(CouplingSystemMassAudit, DeficitVolumesAccumulateAcrossAllCells) {
    const std::vector cells{
        wet_cell(/*h=*/2.0,        /*phi_t=*/0.4, /*area=*/50.0, /*deficit=*/1.0),
        wet_cell(/*h=*/2.0,        /*phi_t=*/0.4, /*area=*/50.0, /*deficit=*/2.0),
        wet_cell(/*h=*/kHWet / 2.0, /*phi_t=*/0.4, /*area=*/50.0, /*deficit=*/4.0),
    };
    const auto audit = scau::coupling::core::compute_system_mass(cells, kHWet);

    EXPECT_DOUBLE_EQ(audit.deficit_mass, 7.0);
}

TEST(CouplingSystemMassAudit, RejectsInvalidInputs) {
    const std::vector good{wet_cell()};

    EXPECT_THROW(static_cast<void>(scau::coupling::core::compute_system_mass(good, 0.0)),
                 std::invalid_argument);
    EXPECT_THROW(static_cast<void>(scau::coupling::core::compute_system_mass(good, -1e-6)),
                 std::invalid_argument);

    EXPECT_THROW(static_cast<void>(scau::coupling::core::compute_system_mass(
                     std::vector{wet_cell(2.0, -0.1)}, kHWet)),
                 std::invalid_argument);
    EXPECT_THROW(static_cast<void>(scau::coupling::core::compute_system_mass(
                     std::vector{wet_cell(-0.1)}, kHWet)),
                 std::invalid_argument);
    EXPECT_THROW(static_cast<void>(scau::coupling::core::compute_system_mass(
                     std::vector{wet_cell(2.0, 0.4, -1.0)}, kHWet)),
                 std::invalid_argument);
    EXPECT_THROW(static_cast<void>(scau::coupling::core::compute_system_mass(
                     std::vector{wet_cell(2.0, 0.4, 50.0, -0.1)}, kHWet)),
                 std::invalid_argument);
}

TEST(CouplingSystemMassAudit, AuditDetectsZeroResidualWhenStateUnchanged) {
    const std::vector cells{
        wet_cell(/*h=*/2.0, /*phi_t=*/0.4, /*area=*/50.0, /*deficit=*/1.0),
        wet_cell(/*h=*/1.5, /*phi_t=*/0.5, /*area=*/40.0, /*deficit=*/0.5),
    };
    const auto baseline = scau::coupling::core::compute_system_mass(cells, kHWet);
    const auto delta = scau::coupling::core::audit_system_mass_against_reference(
        baseline, cells, kHWet);

    EXPECT_DOUBLE_EQ(delta.residual, 0.0);
    EXPECT_TRUE(delta.conserved);
}

TEST(CouplingSystemMassAudit, AuditDetectsNonZeroResidualWhenMassMissing) {
    std::vector cells{wet_cell(/*h=*/2.0, /*phi_t=*/0.4, /*area=*/50.0)};
    const auto baseline = scau::coupling::core::compute_system_mass(cells, kHWet);

    cells[0].h = 1.0;
    const auto delta = scau::coupling::core::audit_system_mass_against_reference(
        baseline, cells, kHWet);

    EXPECT_DOUBLE_EQ(delta.residual, -20.0);
    EXPECT_FALSE(delta.conserved);
}

TEST(CouplingSystemMassAudit, AuditPreservesConservationWhenGrantedMatchedByDeficit) {
    std::vector cells{wet_cell(/*h=*/2.0, /*phi_t=*/0.4, /*area=*/50.0, /*deficit=*/0.0)};
    const auto baseline = scau::coupling::core::compute_system_mass(cells, kHWet);

    const double dh = 0.5;
    const double matching_deficit = cells[0].phi_t * dh * cells[0].area;
    cells[0].h -= dh;
    cells[0].mass_deficit_account.volume += matching_deficit;

    const auto delta = scau::coupling::core::audit_system_mass_against_reference(
        baseline, cells, kHWet);

    EXPECT_DOUBLE_EQ(delta.residual, 0.0);
    EXPECT_TRUE(delta.conserved);
}

TEST(CouplingSystemMassAudit, RejectsNegativeBaseline) {
    const std::vector cells{wet_cell()};
    scau::coupling::core::SystemMassAudit bad_baseline{};
    bad_baseline.total_mass = -1.0;

    EXPECT_THROW(static_cast<void>(scau::coupling::core::audit_system_mass_against_reference(
                     bad_baseline, cells, kHWet)),
                 std::invalid_argument);
}

TEST(CouplingSystemMassAudit, ClassifiesStrictlyConservedDelta) {
    const scau::coupling::core::SystemMassDelta delta{.residual = 0.0, .conserved = true};

    EXPECT_EQ(
        scau::coupling::core::classify_system_mass_conservation(delta),
        scau::coupling::core::SystemMassConservationStatus::conserved);
}

TEST(CouplingSystemMassAudit, ClassifiesNonConservedDeltaAsDrifted) {
    const scau::coupling::core::SystemMassDelta delta{.residual = 0.0, .conserved = false};

    EXPECT_EQ(
        scau::coupling::core::classify_system_mass_conservation(delta),
        scau::coupling::core::SystemMassConservationStatus::drifted);
}

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
        .volume = phi_t * h * area,
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
                     good, std::numeric_limits<double>::quiet_NaN())),
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
    EXPECT_THROW(static_cast<void>(scau::coupling::core::compute_system_mass(
                     std::vector{wet_cell(std::numeric_limits<double>::quiet_NaN())}, kHWet)),
                 std::invalid_argument);
    EXPECT_THROW(static_cast<void>(scau::coupling::core::compute_system_mass(
                     std::vector{wet_cell(2.0, std::numeric_limits<double>::infinity())}, kHWet)),
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

TEST(CouplingSystemMassAudit, AuditDetectsReplayedExchangeStorageChange) {
    scau::coupling::core::CouplingState state{{{
        .volume = 100.0,
        .mass_deficit_account = {.volume = 0.0},
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
    }}};

    const auto baseline = state.compute_system_mass(kHWet);
    const auto decision = state.apply_exchange(0U, {.q_request = 4.0, .dt_sub = 4.0});
    state.replay_pending();

    const auto delta = state.audit_system_mass_against_reference(baseline, kHWet);

    EXPECT_DOUBLE_EQ(delta.residual, -(decision.exchange.v_granted + decision.exchange.v_repay));
    EXPECT_FALSE(delta.conserved);
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

TEST(CouplingSystemMassAudit, BuildsConservationDiagnosticFromDelta) {
    const scau::coupling::core::SystemMassDelta delta{
        .baseline = {.total_mass = 40.0},
        .current = {.total_mass = 40.0},
        .residual = 0.0,
        .conserved = true,
    };

    const auto diagnostic = scau::coupling::core::make_system_mass_conservation_diagnostic(delta);

    EXPECT_EQ(diagnostic.status, scau::coupling::core::SystemMassConservationStatus::conserved);
    EXPECT_DOUBLE_EQ(diagnostic.residual, 0.0);
    EXPECT_DOUBLE_EQ(diagnostic.baseline_total_mass, 40.0);
    EXPECT_DOUBLE_EQ(diagnostic.current_total_mass, 40.0);
}

TEST(CouplingSystemMassAudit, BuildsDriftDiagnosticFromDelta) {
    const scau::coupling::core::SystemMassDelta delta{
        .baseline = {.total_mass = 40.0},
        .current = {.total_mass = 44.0},
        .residual = 4.0,
        .conserved = false,
    };

    const auto diagnostic = scau::coupling::core::make_system_mass_conservation_diagnostic(delta);

    EXPECT_EQ(diagnostic.status, scau::coupling::core::SystemMassConservationStatus::drifted);
    EXPECT_DOUBLE_EQ(diagnostic.residual, 4.0);
    EXPECT_DOUBLE_EQ(diagnostic.baseline_total_mass, 40.0);
    EXPECT_DOUBLE_EQ(diagnostic.current_total_mass, 44.0);
}

TEST(CouplingSystemMassAudit, GateDecisionContinuesForConservedDiagnostic) {
    const scau::coupling::core::SystemMassConservationDiagnostic diagnostic{
        .status = scau::coupling::core::SystemMassConservationStatus::conserved,
        .residual = 0.0,
        .baseline_total_mass = 40.0,
        .current_total_mass = 40.0,
    };

    const auto decision = scau::coupling::core::decide_system_mass_gate_action(diagnostic);

    EXPECT_EQ(decision.action, scau::coupling::core::SystemMassGateAction::continue_run);
    EXPECT_EQ(decision.diagnostic.status, diagnostic.status);
    EXPECT_DOUBLE_EQ(decision.diagnostic.residual, diagnostic.residual);
    EXPECT_DOUBLE_EQ(decision.diagnostic.baseline_total_mass, diagnostic.baseline_total_mass);
    EXPECT_DOUBLE_EQ(decision.diagnostic.current_total_mass, diagnostic.current_total_mass);
}

TEST(CouplingSystemMassAudit, GateDecisionAbortsForDriftedDiagnostic) {
    const scau::coupling::core::SystemMassConservationDiagnostic diagnostic{
        .status = scau::coupling::core::SystemMassConservationStatus::drifted,
        .residual = 4.0,
        .baseline_total_mass = 40.0,
        .current_total_mass = 44.0,
    };

    const auto decision = scau::coupling::core::decide_system_mass_gate_action(diagnostic);

    EXPECT_EQ(decision.action, scau::coupling::core::SystemMassGateAction::abort_run);
    EXPECT_EQ(decision.diagnostic.status, diagnostic.status);
    EXPECT_DOUBLE_EQ(decision.diagnostic.residual, diagnostic.residual);
    EXPECT_DOUBLE_EQ(decision.diagnostic.baseline_total_mass, diagnostic.baseline_total_mass);
    EXPECT_DOUBLE_EQ(decision.diagnostic.current_total_mass, diagnostic.current_total_mass);
}

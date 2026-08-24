// G29 backwater_reverse_debit (M278, failure-revealing first).
//
// bug-208: driving the SWMM outfall stage from the river lets SWMM import
// volume through its boundary with NO ledger debit from D-Flow FM -- the
// tri-model system fabricates water. This golden first LOCKS that the legacy
// path has no debit mechanism at all, then locks the governed reverse
// exchange: backwater imports measured from negative cumulative-outflow
// register deltas are debited from the river as capacity-clamped negative
// laterals through the same CouplingLib interface governance, with the
// in-flight reverse buffer owned by the audit.

#include <gtest/gtest.h>

#include "coupling/driver/tri_coupling.hpp"

namespace {

using scau::coupling::core::CouplingState;
using scau::coupling::drainage::MockSwmmEngine;
using scau::coupling::driver::DrainageRiverInjectionMode;
using scau::coupling::driver::DrainageRiverInterfaceState;
using scau::coupling::driver::TriCouplingStepConfig;
using scau::coupling::driver::advance_tri_coupling_step;
using scau::coupling::river::MockDFlowFMEngine;

constexpr double kDtSub = 4.0;
constexpr int kOutfall = 7;
constexpr int kRiverLocation = 5;
constexpr double kNear = 1.0e-9;
constexpr double kRiverStage = 3.5;

// Backwater scenario: the register runs NEGATIVE (boundary import) while the
// river stage drives the outfall: -0.9 then -0.6 m3 imported. The substep-1
// import is debited at substep 2; the substep-2 import stays as an in-flight
// reverse obligation the audit owns.
const double kCumulativeAfter[] = {-0.9, -1.5};
constexpr double kTrueImportedTotal = 1.5;

CouplingState make_state() {
    return CouplingState{{
        {.volume = 40.0, .mass_deficit_account = {.volume = 0.0},
         .phi_t = 0.4, .h = 2.0, .area = 50.0},
    }};
}

TriCouplingStepConfig make_config(DrainageRiverInjectionMode mode) {
    TriCouplingStepConfig config{};
    config.drainage_river.push_back({
        .outfall_node_id = kOutfall,
        .river_location_id = kRiverLocation,
        .q_capacity = 10.0,
        .drive_outfall_stage = true,
        .injection_mode = mode,
    });
    config.step_engines = true;
    return config;
}

}  // namespace

TEST(G29BackwaterReverseDebit, LegacyStageDrivenBackwaterHasNoRiverDebit) {
    auto state = make_state();
    MockSwmmEngine swmm;
    MockDFlowFMEngine dflowfm;
    swmm.initialize("mock.inp");
    dflowfm.initialize("mock.mdu");
    dflowfm.set_water_level_fixture(kRiverLocation, kRiverStage);
    const auto config = make_config(DrainageRiverInjectionMode::sampled_rate_legacy);

    for (const double cumulative : kCumulativeAfter) {
        swmm.set_node_cumulative_outflow_fixture(kOutfall, cumulative);
        const auto report = advance_tri_coupling_step(state, swmm, dflowfm, config, kDtSub);
        // Failure-revealing lock (bug-208): the stage IS driven onto the
        // outfall, but the imported boundary volume is never measured and the
        // river is never debited -- the interface reports carry no movement
        // and the river lateral stays untouched.
        EXPECT_DOUBLE_EQ(swmm.outfall_stage(kOutfall), kRiverStage);
        ASSERT_EQ(report.interface_buffer_reports.size(), 1U);
        EXPECT_EQ(report.interface_buffer_reports[0].reverse_volume_m3, 0.0);
        EXPECT_EQ(report.interface_buffer_reports[0].debited_volume_m3, 0.0);
        EXPECT_DOUBLE_EQ(dflowfm.get_value("lateral_discharge", kRiverLocation), 0.0);
    }
}

TEST(G29BackwaterReverseDebit, GovernedReverseExchangeDebitsExactlyWhatWasImported) {
    auto state = make_state();
    MockSwmmEngine swmm;
    MockDFlowFMEngine dflowfm;
    swmm.initialize("mock.inp");
    dflowfm.initialize("mock.mdu");
    dflowfm.set_water_level_fixture(kRiverLocation, kRiverStage);
    const auto config = make_config(DrainageRiverInjectionMode::emitted_volume);
    DrainageRiverInterfaceState interface_state;

    double observed_imported = 0.0;
    double debited_volume = 0.0;
    double last_lateral_q = 0.0;
    for (const double cumulative : kCumulativeAfter) {
        swmm.set_node_cumulative_outflow_fixture(kOutfall, cumulative);
        const auto report = advance_tri_coupling_step(
            state, swmm, dflowfm, config, interface_state, kDtSub);
        EXPECT_DOUBLE_EQ(swmm.outfall_stage(kOutfall), kRiverStage);
        ASSERT_EQ(report.interface_buffer_reports.size(), 1U);
        const auto& entry = report.interface_buffer_reports[0];
        observed_imported += entry.reverse_volume_m3;
        debited_volume += entry.debited_volume_m3;
        last_lateral_q = dflowfm.get_value("lateral_discharge", kRiverLocation);
    }

    // Every imported cubic metre was measured from the register deltas.
    EXPECT_NEAR(observed_imported, kTrueImportedTotal, kNear);
    // Exact governance: debited + still-buffered == imported. The debit is a
    // NEGATIVE river lateral (withdrawal), never a tolerance.
    const double buffered = interface_state.reverse.at(kOutfall).volume;
    EXPECT_NEAR(debited_volume + buffered, kTrueImportedTotal, kNear);
    // Substep 2 debited the substep-1 import of 0.9 m3 at q = -0.225 m3/s.
    EXPECT_NEAR(last_lateral_q, -0.9 / kDtSub, kNear);
    // The audit in-flight term is NEGATIVE for reverse obligations: the
    // tri-model system currently holds water the river must still surrender.
    EXPECT_NEAR(interface_state.inflight_volume(), -buffered, kNear);
    // The substep-2 import (0.6 m3) is the remaining in-flight obligation.
    EXPECT_NEAR(buffered, 0.6, kNear);
}

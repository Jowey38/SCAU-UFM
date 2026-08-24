// G28 interface_emitted_volume_conservation (M278, failure-revealing first).
//
// bug-210: the legacy SWMM outfall -> river injection samples an
// instantaneous rate and applies it in the same substep, so the injected
// volume disagrees with the boundary volume SWMM actually emitted. This
// golden first LOCKS that mismatch on the legacy path (the failure the M278
// redesign governs), then locks exact volume conservation on the governed
// emitted-volume path: injected volume + in-flight interface buffer ==
// cumulative emitted boundary volume, with the capacity clamp only delaying
// (never destroying) volume.

#include <gtest/gtest.h>

#include "coupling/driver/tri_coupling.hpp"

namespace {

using scau::coupling::core::CouplingState;
using scau::coupling::drainage::MockSwmmEngine;
using scau::coupling::driver::DrainageRiverInjectionMode;
using scau::coupling::driver::DrainageRiverInterfaceState;
using scau::coupling::driver::TriCouplingStepConfig;
using scau::coupling::driver::advance_tri_coupling_step;
using scau::coupling::driver::age_drainage_river_interface_buffers;
using scau::coupling::river::MockDFlowFMEngine;

constexpr double kDtSub = 4.0;
constexpr int kOutfall = 7;
constexpr int kRiverLocation = 5;
constexpr double kNear = 1.0e-9;

struct SubstepDrive {
    double sampled_rate;              // instantaneous outfall rate fixture
    double cumulative_outflow_after;  // massbal register after this substep
};

// Sampled rates deliberately disagree with the emitted boundary volume:
// legacy injects sum(q*dt) = 4.0 m3 while SWMM only emitted 2.6 m3.
const SubstepDrive kDrives[] = {
    {.sampled_rate = 0.5, .cumulative_outflow_after = 1.2},
    {.sampled_rate = 0.5, .cumulative_outflow_after = 2.0},
    {.sampled_rate = 0.0, .cumulative_outflow_after = 2.6},
};
constexpr double kTrueEmittedTotal = 2.6;
constexpr double kLegacyInjectedTotal = (0.5 + 0.5 + 0.0) * kDtSub;

CouplingState make_state() {
    return CouplingState{{
        {.volume = 40.0, .mass_deficit_account = {.volume = 0.0},
         .phi_t = 0.4, .h = 2.0, .area = 50.0},
    }};
}

TriCouplingStepConfig make_config(DrainageRiverInjectionMode mode, double q_capacity) {
    TriCouplingStepConfig config{};
    config.drainage_river.push_back({
        .outfall_node_id = kOutfall,
        .river_location_id = kRiverLocation,
        .q_capacity = q_capacity,
        .drive_outfall_stage = false,
        .injection_mode = mode,
    });
    config.step_engines = true;
    return config;
}

}  // namespace

TEST(G28InterfaceEmittedVolumeConservation, LegacySampledRatePathIsNotVolumeConservative) {
    auto state = make_state();
    MockSwmmEngine swmm;
    MockDFlowFMEngine dflowfm;
    swmm.initialize("mock.inp");
    dflowfm.initialize("mock.mdu");
    const auto config = make_config(DrainageRiverInjectionMode::sampled_rate_legacy, 10.0);

    double injected_volume = 0.0;
    for (const auto& drive : kDrives) {
        swmm.set_node_inflow_fixture(kOutfall, drive.sampled_rate);
        swmm.set_node_cumulative_outflow_fixture(kOutfall, drive.cumulative_outflow_after);
        const auto report = advance_tri_coupling_step(state, swmm, dflowfm, config, kDtSub);
        ASSERT_EQ(report.interface_decisions.size(), 1U);
        injected_volume += report.interface_decisions[0].v_granted;
    }

    // Failure-revealing lock (bug-210): sampled-rate injection is 4.0 m3
    // against 2.6 m3 actually emitted -- a 1.4 m3 fabrication. This is the
    // ungoverned mass path the whole-system audit guards.
    EXPECT_NEAR(injected_volume, kLegacyInjectedTotal, kNear);
    EXPECT_GT(injected_volume - kTrueEmittedTotal, 1.0);
}

TEST(G28InterfaceEmittedVolumeConservation, GovernedEmittedVolumePathConservesExactly) {
    auto state = make_state();
    MockSwmmEngine swmm;
    MockDFlowFMEngine dflowfm;
    swmm.initialize("mock.inp");
    dflowfm.initialize("mock.mdu");
    const auto config = make_config(DrainageRiverInjectionMode::emitted_volume, 10.0);
    DrainageRiverInterfaceState interface_state;

    double injected_volume = 0.0;
    double observed_emitted = 0.0;
    double first_substep_injection = -1.0;
    for (const auto& drive : kDrives) {
        // The sampled rate must be IGNORED by the governed path.
        swmm.set_node_inflow_fixture(kOutfall, drive.sampled_rate);
        swmm.set_node_cumulative_outflow_fixture(kOutfall, drive.cumulative_outflow_after);
        const auto report = advance_tri_coupling_step(
            state, swmm, dflowfm, config, interface_state, kDtSub);
        ASSERT_EQ(report.interface_buffer_reports.size(), 1U);
        injected_volume += report.interface_buffer_reports[0].injected_volume_m3;
        observed_emitted += report.interface_buffer_reports[0].emitted_volume_m3;
        if (first_substep_injection < 0.0) {
            first_substep_injection = report.interface_buffer_reports[0].injected_volume_m3;
        }
    }

    // Substep N emission is injected at substep N+1: nothing to inject on
    // the very first substep.
    EXPECT_EQ(first_substep_injection, 0.0);
    // Every emitted cubic metre was observed through the register deltas.
    EXPECT_NEAR(observed_emitted, kTrueEmittedTotal, kNear);
    // Exact conservation: injected + still-buffered == emitted. No volume is
    // fabricated from the sampled rate and none is lost.
    const double buffered = interface_state.emitted.at(kOutfall).volume;
    EXPECT_NEAR(injected_volume + buffered, kTrueEmittedTotal, kNear);
    // With generous capacity only the final substep's emission remains.
    EXPECT_NEAR(buffered, 0.6, kNear);
    EXPECT_NEAR(interface_state.inflight_volume(), buffered, kNear);
}

TEST(G28InterfaceEmittedVolumeConservation, CapacityClampDelaysButNeverDestroysVolume) {
    auto state = make_state();
    MockSwmmEngine swmm;
    MockDFlowFMEngine dflowfm;
    swmm.initialize("mock.inp");
    dflowfm.initialize("mock.mdu");
    // 0.1 m3/s over dt_sub = 4 s allows at most 0.4 m3 per substep.
    const auto config = make_config(DrainageRiverInjectionMode::emitted_volume, 0.1);
    DrainageRiverInterfaceState interface_state;

    double injected_volume = 0.0;
    for (const auto& drive : kDrives) {
        swmm.set_node_cumulative_outflow_fixture(kOutfall, drive.cumulative_outflow_after);
        const auto report = advance_tri_coupling_step(
            state, swmm, dflowfm, config, interface_state, kDtSub);
        const auto& entry = report.interface_buffer_reports[0];
        EXPECT_LE(entry.injected_volume_m3, 0.1 * kDtSub + kNear);
        injected_volume += entry.injected_volume_m3;
    }
    const double buffered = interface_state.emitted.at(kOutfall).volume;
    EXPECT_NEAR(injected_volume + buffered, kTrueEmittedTotal, kNear);
    EXPECT_GT(buffered, 0.4);  // clamp left volume in flight, not destroyed

    // Deficit-style aging: standing volume ages per committed epoch and a
    // cleared account resets.
    age_drainage_river_interface_buffers(interface_state);
    age_drainage_river_interface_buffers(interface_state);
    EXPECT_EQ(interface_state.emitted.at(kOutfall).age_epochs, 2U);
    interface_state.emitted.at(kOutfall).volume = 0.0;
    age_drainage_river_interface_buffers(interface_state);
    EXPECT_EQ(interface_state.emitted.at(kOutfall).age_epochs, 0U);
}

TEST(G28InterfaceEmittedVolumeConservation, LegacyEntryPointFailsClosedForGovernedMode) {
    auto state = make_state();
    MockSwmmEngine swmm;
    MockDFlowFMEngine dflowfm;
    swmm.initialize("mock.inp");
    dflowfm.initialize("mock.mdu");
    const auto config = make_config(DrainageRiverInjectionMode::emitted_volume, 10.0);
    EXPECT_THROW(
        static_cast<void>(advance_tri_coupling_step(state, swmm, dflowfm, config, kDtSub)),
        std::invalid_argument);
}

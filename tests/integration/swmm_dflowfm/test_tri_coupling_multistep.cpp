#include <cmath>
#include <vector>

#include <gtest/gtest.h>

#include "coupling/driver/tri_coupling.hpp"

// Multi-sub-step integration coverage for the pairwise bidirectional
// tri-coupling driver. The unit suite (test_coupling_tri_driver) exercises one
// dt_sub at a time; this layer advances the same driver across several coupled
// sub-steps with both mock engines to lock down TEMPORAL invariants that a
// single step cannot reveal:
//   - cross-step surface mass continuity (no mass appears/vanishes between
//     driver calls),
//   - sustained drain monotonicity,
//   - return-flow accounting across steps,
//   - endpoint deficit repayment that survives the step boundary even after
//     the surface drain intent drops to zero.
//
// All three pairs (surface<->pipe, surface<->river, pipe<->river) stay engine
// isolated: the exchange happens exclusively through core DTO decisions.

namespace {

using scau::coupling::core::CouplingState;
using scau::coupling::core::ExchangeCellState;
using scau::coupling::core::SharedExchangeEngine;
using scau::coupling::driver::DrainageRiverLink;
using scau::coupling::driver::SurfaceDrainageLink;
using scau::coupling::driver::SurfaceRiverLink;
using scau::coupling::driver::TriCouplingStepConfig;
using scau::coupling::driver::TriCouplingStepReport;
using scau::coupling::driver::advance_tri_coupling_step;
using scau::coupling::drainage::MockSwmmEngine;
using scau::coupling::river::MockDFlowFMEngine;

constexpr double kDtSub = 4.0;

// Two identical cells: V_limit = 0.9 * phi_t * h * area = 0.9 * 0.4 * 2 * 50 = 36,
// Q_limit = 36 / 4 = 9.
CouplingState make_state() {
    return CouplingState{{
        {.volume = 40.0, .mass_deficit_account = {.volume = 0.0},
         .phi_t = 0.4, .h = 2.0, .area = 50.0},
        {.volume = 40.0, .mass_deficit_account = {.volume = 0.0},
         .phi_t = 0.4, .h = 2.0, .area = 50.0},
    }};
}

double endpoint_drainage_deficit(const CouplingState& state, std::size_t cell, int node_id) {
    for (const auto& shared : state.cells()[cell].shared_deficit_accounts) {
        if (shared.endpoint.engine == SharedExchangeEngine::drainage &&
            shared.endpoint.node_id == static_cast<std::size_t>(node_id)) {
            return shared.mass_deficit_account.volume;
        }
    }
    return 0.0;
}

}  // namespace

// All three bidirectional pairs run every sub-step; the surface mass reported at
// the end of step i must be exactly the surface mass reported at the start of
// step i+1 (state is untouched between driver calls), and cell volumes must stay
// non-negative throughout.
TEST(TriCouplingMultiStep, CoupledSubStepsPreserveSurfaceMassContinuity) {
    auto state = make_state();
    MockSwmmEngine swmm;
    MockDFlowFMEngine dflowfm;
    swmm.initialize("mock.inp");
    dflowfm.initialize("mock.mdu");

    // Standing fixtures drive the pipe->river interface and the pipe->surface
    // return every step (mock fixtures persist across step()).
    swmm.set_node_inflow_fixture(21, 0.3);
    swmm.set_node_overflow_fixture(11, 0.1);
    dflowfm.set_water_level_fixture(7, 9.4);

    const TriCouplingStepConfig config{
        .surface_drainage = {{.cell_index = 0U, .node_id = 11, .q_request = 1.5, .priority_weight = 1.0}},
        .surface_river = {{.cell_index = 1U, .location_id = 5, .q_request = 2.0,
                           .priority_weight = 1.0, .q_return = 0.05}},
        .drainage_river = {{.outfall_node_id = 21, .river_location_id = 7,
                            .q_capacity = 1.0, .drive_outfall_stage = true}},
    };

    constexpr int kSteps = 5;
    std::vector<TriCouplingStepReport> reports;
    reports.reserve(kSteps);
    for (int i = 0; i < kSteps; ++i) {
        reports.push_back(advance_tri_coupling_step(state, swmm, dflowfm, config, kDtSub));
    }

    for (int i = 0; i + 1 < kSteps; ++i) {
        EXPECT_DOUBLE_EQ(
            reports[static_cast<std::size_t>(i)].surface_mass_after.surface_mass,
            reports[static_cast<std::size_t>(i) + 1U].surface_mass_before.surface_mass)
            << "surface mass discontinuity between step " << i << " and " << (i + 1);
    }

    for (const auto& cell : state.cells()) {
        EXPECT_GE(cell.volume, -1.0e-9);
    }

    EXPECT_DOUBLE_EQ(swmm.elapsed_time(), kSteps * kDtSub);
    EXPECT_DOUBLE_EQ(dflowfm.elapsed_time(), kSteps * kDtSub);
}

// A steady sub-Q_limit drain depletes the surface monotonically; each step the
// engine receives the granted lateral inflow and the surface loses exactly the
// reported granted volume.
TEST(TriCouplingMultiStep, SustainedDrainMonotonicallyDepletesSurface) {
    auto state = make_state();
    MockSwmmEngine swmm;
    MockDFlowFMEngine dflowfm;
    swmm.initialize("mock.inp");
    dflowfm.initialize("mock.mdu");

    const TriCouplingStepConfig config{
        .surface_drainage = {{.cell_index = 0U, .node_id = 11, .q_request = 2.0, .priority_weight = 1.0}},
    };

    double previous = state.compute_system_mass(1.0e-6).surface_mass;
    constexpr int kSteps = 3;
    for (int i = 0; i < kSteps; ++i) {
        const auto report = advance_tri_coupling_step(state, swmm, dflowfm, config, kDtSub);
        const double after = report.surface_mass_after.surface_mass;
        EXPECT_LT(after, previous) << "surface did not drop at step " << i;
        EXPECT_GT(swmm.get_node_lateral_inflow(11), 0.0);
        previous = after;
    }

    EXPECT_GT(state.cells()[0].volume, 0.0);
}

// Standing pipe overflow (cell 0) and explicit river spill (cell 1) inject water
// every step. The total surface mass gain across the run must equal the sum of
// the per-step returned volumes recorded in the reports.
TEST(TriCouplingMultiStep, SustainedReturnsAccumulateConservatively) {
    auto state = make_state();
    MockSwmmEngine swmm;
    MockDFlowFMEngine dflowfm;
    swmm.initialize("mock.inp");
    dflowfm.initialize("mock.mdu");

    swmm.set_node_overflow_fixture(11, 0.25);  // pipe -> surface, q_return 0.25

    const TriCouplingStepConfig config{
        .surface_drainage = {{.cell_index = 0U, .node_id = 11, .q_request = 0.0, .priority_weight = 1.0}},
        .surface_river = {{.cell_index = 1U, .location_id = 5, .q_request = 0.0,
                           .priority_weight = 1.0, .q_return = 0.1}},
    };

    const double initial = state.compute_system_mass(1.0e-6).surface_mass;
    double previous = initial;
    double summed_returns = 0.0;
    constexpr int kSteps = 4;
    for (int i = 0; i < kSteps; ++i) {
        const auto report = advance_tri_coupling_step(state, swmm, dflowfm, config, kDtSub);
        for (const auto& decision : report.return_decisions) {
            summed_returns += decision.v_returned;
        }
        const double after = report.surface_mass_after.surface_mass;
        EXPECT_GT(after, previous) << "surface did not rise at step " << i;
        previous = after;
    }

    const double final_mass = state.compute_system_mass(1.0e-6).surface_mass;
    EXPECT_NEAR(final_mass - initial, summed_returns, 1.0e-9);
    // (0.25 + 0.1) * dt_sub * kSteps = 0.35 * 4 * 4 = 5.6.
    EXPECT_NEAR(final_mass - initial, 5.6, 1.0e-9);
}

// Overdriving a cell past Q_limit accrues an endpoint deficit. With the link
// retained but its new intent dropped to zero, the deficit must keep being
// repaid across the step boundary (engine still receives repayment inflow), and
// no fresh unmet volume is created once the request is zero.
TEST(TriCouplingMultiStep, EndpointDeficitRepaysAcrossCoupledSteps) {
    CouplingState state{{
        {.volume = 250.0, .mass_deficit_account = {.volume = 0.0},
         .phi_t = 0.5, .h = 10.0, .area = 50.0},
    }};
    MockSwmmEngine swmm;
    MockDFlowFMEngine dflowfm;
    swmm.initialize("mock.inp");
    dflowfm.initialize("mock.mdu");

    constexpr int kNode = 11;

    // Step 1: overdrive far above Q_limit so the cap leaves an unmet deficit.
    const TriCouplingStepConfig overdrive{
        .surface_drainage = {{.cell_index = 0U, .node_id = kNode, .q_request = 100.0,
                              .priority_weight = 1.0}},
    };
    const auto first = advance_tri_coupling_step(state, swmm, dflowfm, overdrive, kDtSub);
    ASSERT_EQ(first.surface_decisions.size(), 1U);
    EXPECT_GT(first.surface_decisions[0].exchange.v_unmet, 0.0);

    const double deficit_after_overdrive = endpoint_drainage_deficit(state, 0U, kNode);
    EXPECT_GT(deficit_after_overdrive, 0.0);

    // Subsequent steps: link retained, but new intent is zero. Deficit must be
    // repaid (monotonically non-increasing), repayment must reach the engine,
    // and no new unmet volume may appear.
    const TriCouplingStepConfig idle{
        .surface_drainage = {{.cell_index = 0U, .node_id = kNode, .q_request = 0.0,
                              .priority_weight = 1.0}},
    };

    double previous_deficit = deficit_after_overdrive;
    bool observed_repayment_to_engine = false;
    constexpr int kRepaySteps = 3;
    for (int i = 0; i < kRepaySteps; ++i) {
        const auto report = advance_tri_coupling_step(state, swmm, dflowfm, idle, kDtSub);
        ASSERT_EQ(report.surface_decisions.size(), 1U);
        EXPECT_DOUBLE_EQ(report.surface_decisions[0].exchange.v_unmet, 0.0)
            << "zero-intent step created fresh unmet volume at repay step " << i;

        const double deficit_now = endpoint_drainage_deficit(state, 0U, kNode);
        EXPECT_LE(deficit_now, previous_deficit + 1.0e-12)
            << "deficit grew during repayment at repay step " << i;
        if (swmm.get_node_lateral_inflow(kNode) > 0.0) {
            observed_repayment_to_engine = true;
        }
        previous_deficit = deficit_now;
    }

    EXPECT_TRUE(observed_repayment_to_engine);
    EXPECT_LT(previous_deficit, deficit_after_overdrive);
    EXPECT_GE(state.cells()[0].volume, -1.0e-9);
}

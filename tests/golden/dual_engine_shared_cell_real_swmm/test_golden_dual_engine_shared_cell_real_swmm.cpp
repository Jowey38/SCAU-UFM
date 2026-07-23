#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "coupling/driver/tri_coupling.hpp"
#include "coupling/drainage/swmm_engine.hpp"

// G15 dual_engine_shared_cell_real_swmm golden (candidate_non_gating):
// the SWMM half of the shared-cell contract on the REAL embedded SWMM 5.2.4
// engine. SWMM (drainage) and a mock D-Flow FM (river) share ONE 2D exchange
// cell through the tri-coupling driver; the drainage grant is written into
// and routed by the real solver.
//
// This is the real-SWMM companion to G12 (all-mock). The river side stays
// mocked because a real D-Flow FM runtime needs an external BMI kernel DLL
// (G11); ci_gate stays false for both until G11 lands, per the GoldenSuite
// plan. What is genuinely exercised here beyond G12:
//   - the shared-cell arbitration decision drives a real SWMM node write
//   - the real solver routes exactly q_granted + q_repay toward the outfall
//   - cross-substep deficit repayment survives real engine stepping

namespace {

using scau::coupling::core::CouplingState;
using scau::coupling::core::SharedExchangeDecision;
using scau::coupling::core::SharedExchangeEngine;
using scau::coupling::drainage::SwmmEngine;
using scau::coupling::river::MockDFlowFMEngine;
using scau::coupling::driver::TriCouplingStepConfig;
using scau::coupling::driver::advance_tri_coupling_step;

constexpr double kDtSub = 4.0;
constexpr int kLocationId = 5;

std::string minimal_case_path() {
    return std::string(SCAU_SWMM_TEST_CASE_DIR) + "/swmm_minimal.inp";
}

// Shared cell: V_limit = 0.9 * 0.4 * 2.0 * 50.0 = 36 m3, Q_limit = 9 m3/s.
// priority weights 1:2 => V_limit_k = 12 (drainage) and 24 (river).
CouplingState make_shared_cell_state() {
    return CouplingState{{
        {.volume = 40.0,
         .mass_deficit_account = {.volume = 0.0},
         .phi_t = 0.4,
         .h = 2.0,
         .area = 50.0},
    }};
}

TriCouplingStepConfig make_config(int j1_node, double q_drainage, double q_river) {
    return TriCouplingStepConfig{
        .surface_drainage = {{.cell_index = 0U,
                              .node_id = j1_node,
                              .q_request = q_drainage,
                              .priority_weight = 1.0}},
        .surface_river = {{.cell_index = 0U,
                           .location_id = kLocationId,
                           .q_request = q_river,
                           .priority_weight = 2.0}},
    };
}

const SharedExchangeDecision& decision_for(
    const std::vector<SharedExchangeDecision>& decisions,
    SharedExchangeEngine engine) {
    for (const auto& decision : decisions) {
        if (decision.endpoint.engine == engine) {
            return decision;
        }
    }
    throw std::logic_error("expected a decision for the requested engine");
}

}  // namespace

TEST(GoldenDualEngineSharedCellRealSwmm, RealSwmmRoutesArbitratedSharedCellGrant) {
    SwmmEngine swmm;
    swmm.initialize(minimal_case_path());
    const int j1 = swmm.node_index("J1");
    const int o1 = swmm.node_index("O1");

    MockDFlowFMEngine dflowfm;
    dflowfm.initialize("mock.mdu");

    auto state = make_shared_cell_state();

    // Weighted request 1*4 + 2*4 = 12 m3/s > Q_limit 9: scale-then-cut.
    // drainage scaled grant 4*0.75 = 3 m3/s meets its k-limit exactly (12 m3),
    // 4 m3 unmet accrues on the drainage endpoint; river grant caps at its own
    // 4 m3/s request.
    const auto report =
        advance_tri_coupling_step(state, swmm, dflowfm, make_config(j1, 4.0, 4.0), kDtSub);

    ASSERT_EQ(report.surface_decisions.size(), 2U);
    const auto& drainage = decision_for(report.surface_decisions, SharedExchangeEngine::drainage);
    const auto& river = decision_for(report.surface_decisions, SharedExchangeEngine::river);

    EXPECT_DOUBLE_EQ(drainage.allocated_limit.v_limit, 12.0);
    EXPECT_DOUBLE_EQ(river.allocated_limit.v_limit, 24.0);
    EXPECT_DOUBLE_EQ(drainage.exchange.v_granted, 12.0);
    EXPECT_DOUBLE_EQ(drainage.exchange.v_unmet, 4.0);
    EXPECT_DOUBLE_EQ(river.exchange.v_granted, 16.0);

    // The arbitrated drainage grant (q = 3 m3/s) was written into and routed
    // by the REAL SWMM solver toward the outfall.
    EXPECT_NEAR(swmm.get_node_lateral_inflow(j1), 3.0, 1.0e-6);
    EXPECT_GT(swmm.get_node_head(j1), 10.0);
    EXPECT_GT(swmm.get_node_inflow(o1), 0.0);

    // River side unchanged from G12 (mock): 4 m3/s accepted.
    EXPECT_DOUBLE_EQ(dflowfm.get_value("lateral_discharge", kLocationId), 4.0);

    // Ledger after replay: granted volume left the cell; unmet drainage volume
    // accrued on the drainage endpoint account only.
    ASSERT_EQ(state.cells().size(), 1U);
    EXPECT_DOUBLE_EQ(state.cells()[0].volume, 12.0);
    for (const auto& account : state.cells()[0].shared_deficit_accounts) {
        if (account.endpoint.engine == SharedExchangeEngine::drainage) {
            EXPECT_DOUBLE_EQ(account.mass_deficit_account.volume, 4.0);
        } else {
            EXPECT_DOUBLE_EQ(account.mass_deficit_account.volume, 0.0);
        }
    }

    swmm.finalize();
}

TEST(GoldenDualEngineSharedCellRealSwmm, DeficitRepaysThroughRealSwmmAcrossSubsteps) {
    SwmmEngine swmm;
    swmm.initialize(minimal_case_path());
    const int j1 = swmm.node_index("J1");

    MockDFlowFMEngine dflowfm;
    dflowfm.initialize("mock.mdu");

    auto state = make_shared_cell_state();

    // Substep 1: accrue 4 m3 of drainage-endpoint deficit (as above).
    (void)advance_tri_coupling_step(state, swmm, dflowfm, make_config(j1, 4.0, 4.0), kDtSub);
    EXPECT_DOUBLE_EQ(state.cells()[0].volume, 12.0);

    // Substep 2: intents drop to zero, link persists -> accrued deficit repays.
    // Cell drained to volume 12 -> h = 0.6 -> V_limit = 0.9*0.4*0.6*50 = 10.8;
    // drainage 1/3 share gives Q_limit_k = 0.9, so q_repay = min(4/4, 0.9) =
    // 0.9 -> v_repay = 3.6, leaving 0.4 on the account. The real solver must
    // route exactly this repayment.
    const auto second =
        advance_tri_coupling_step(state, swmm, dflowfm, make_config(j1, 0.0, 0.0), kDtSub);
    const auto& drainage2 = decision_for(second.surface_decisions, SharedExchangeEngine::drainage);
    EXPECT_DOUBLE_EQ(drainage2.exchange.v_repay, 3.6);
    EXPECT_DOUBLE_EQ(drainage2.exchange.v_granted, 0.0);
    EXPECT_NEAR(swmm.get_node_lateral_inflow(j1), 0.9, 1.0e-6);
    EXPECT_DOUBLE_EQ(state.cells()[0].volume, 8.4);

    // Substep 3: residual 0.4 m3 clears in full.
    const auto third =
        advance_tri_coupling_step(state, swmm, dflowfm, make_config(j1, 0.0, 0.0), kDtSub);
    const auto& drainage3 = decision_for(third.surface_decisions, SharedExchangeEngine::drainage);
    EXPECT_NEAR(drainage3.exchange.v_repay, 0.4, 1.0e-9);
    EXPECT_NEAR(swmm.get_node_lateral_inflow(j1), 0.1, 1.0e-6);
    EXPECT_NEAR(state.cells()[0].volume, 8.0, 1.0e-9);
    for (const auto& account : state.cells()[0].shared_deficit_accounts) {
        EXPECT_NEAR(account.mass_deficit_account.volume, 0.0, 1.0e-9);
    }

    swmm.finalize();
}

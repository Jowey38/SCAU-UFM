#include <vector>

#include <gtest/gtest.h>

#include "coupling/driver/tri_coupling.hpp"

// G12 dual_engine_shared_cell golden (first landing, candidate_non_gating):
// SWMM and D-Flow FM share ONE 2D exchange cell through the tri-coupling
// driver. Locks the main-spec 6.1 shared-cell contract:
//   1. V_limit_k split by priority_weight (V_limit = 0.9 * phi_t * h * A)
//   2. Q_limit arbitration: granted + repaid volume never exceeds V_limit_k
//   3. per-endpoint mass_deficit_account accrual and cross-substep repayment
//   4. snapshot/rollback/replay determinism of the shared-cell ledger
// Mock engines only: real dual-engine evidence lands after G11 (real D-Flow
// FM runtime); ci_gate stays false until then per the GoldenSuite plan.

namespace {

using scau::coupling::core::CouplingState;
using scau::coupling::core::ExchangeCellState;
using scau::coupling::core::SharedExchangeDecision;
using scau::coupling::core::SharedExchangeEngine;
using scau::coupling::driver::SurfaceDrainageLink;
using scau::coupling::driver::SurfaceRiverLink;
using scau::coupling::driver::TriCouplingStepConfig;
using scau::coupling::driver::advance_tri_coupling_step;
using scau::coupling::drainage::MockSwmmEngine;
using scau::coupling::river::MockDFlowFMEngine;

constexpr double kDtSub = 4.0;
constexpr int kNodeId = 11;
constexpr int kLocationId = 5;

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

// Substep-1 intents: drainage 4 m3/s (w=1), river 4 m3/s (w=2). Weighted
// request = 1*4 + 2*4 = 12 m3/s > Q_limit = 9, so the spec 6.1 rule-3 order
// applies: proportional scaling by priority_weight FIRST (scale = 9/12 =
// 0.75, exact in binary), then the per-endpoint V_limit_k hard cut.
TriCouplingStepConfig make_drain_config(double q_drainage, double q_river) {
    return TriCouplingStepConfig{
        .surface_drainage = {{.cell_index = 0U,
                              .node_id = kNodeId,
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

TEST(GoldenDualEngineSharedCell, VLimitSplitsByPriorityWeightAndClampsGrants) {
    auto state = make_shared_cell_state();
    MockSwmmEngine swmm;
    MockDFlowFMEngine dflowfm;
    swmm.initialize("mock.inp");
    dflowfm.initialize("mock.mdu");

    const auto report = advance_tri_coupling_step(
        state, swmm, dflowfm, make_drain_config(4.0, 4.0), kDtSub);

    ASSERT_EQ(report.surface_decisions.size(), 2U);
    const auto& drainage = decision_for(report.surface_decisions, SharedExchangeEngine::drainage);
    const auto& river = decision_for(report.surface_decisions, SharedExchangeEngine::river);

    // 1. V_limit_k split by priority_weight: 36 m3 split 1:2.
    EXPECT_DOUBLE_EQ(drainage.allocated_limit.v_limit, 12.0);
    EXPECT_DOUBLE_EQ(river.allocated_limit.v_limit, 24.0);
    EXPECT_DOUBLE_EQ(drainage.allocated_limit.q_limit, 3.0);
    EXPECT_DOUBLE_EQ(river.allocated_limit.q_limit, 6.0);

    // 2. Q_limit arbitration (scale-then-cut): drainage scaled grant
    //    1*4*0.75 = 3 m3/s coincides with its k-limit (12 m3); river scaled
    //    grant 2*4*0.75 = 6 m3/s caps at its own request 4 m3/s (16 m3).
    EXPECT_DOUBLE_EQ(drainage.exchange.v_granted, 12.0);
    EXPECT_DOUBLE_EQ(drainage.exchange.v_unmet, 4.0);
    EXPECT_DOUBLE_EQ(river.exchange.v_granted, 16.0);
    EXPECT_DOUBLE_EQ(river.exchange.v_unmet, 0.0);
    for (const auto& decision : report.surface_decisions) {
        EXPECT_LE(
            decision.exchange.v_granted + decision.exchange.v_repay,
            decision.allocated_limit.v_limit + 1.0e-12);
    }

    // Engine-side acceptance: q = q_granted + q_repay.
    EXPECT_DOUBLE_EQ(swmm.get_node_lateral_inflow(kNodeId), 3.0);
    EXPECT_DOUBLE_EQ(dflowfm.get_value("lateral_discharge", kLocationId), 4.0);

    // 3. Ledger after replay: granted volume left the cell; the unmet
    //    drainage volume accrued on the drainage endpoint account only.
    ASSERT_EQ(state.cells().size(), 1U);
    EXPECT_DOUBLE_EQ(state.cells()[0].volume, 12.0);
    ASSERT_EQ(state.cells()[0].shared_deficit_accounts.size(), 2U);
    const auto& accounts = state.cells()[0].shared_deficit_accounts;
    for (const auto& account : accounts) {
        if (account.endpoint.engine == SharedExchangeEngine::drainage) {
            EXPECT_DOUBLE_EQ(account.mass_deficit_account.volume, 4.0);
        } else {
            EXPECT_DOUBLE_EQ(account.mass_deficit_account.volume, 0.0);
        }
    }
    EXPECT_DOUBLE_EQ(state.cells()[0].mass_deficit_account.volume, 0.0);
}

TEST(GoldenDualEngineSharedCell, DeficitRepaysAcrossSubstepsAndQuiesces) {
    auto state = make_shared_cell_state();
    MockSwmmEngine swmm;
    MockDFlowFMEngine dflowfm;
    swmm.initialize("mock.inp");
    dflowfm.initialize("mock.mdu");

    // Substep 1: accrue exactly 4 m3 of drainage-endpoint deficit.
    (void)advance_tri_coupling_step(state, swmm, dflowfm, make_drain_config(4.0, 4.0), kDtSub);
    EXPECT_DOUBLE_EQ(state.cells()[0].volume, 12.0);

    // Substep 2: intents drop to zero but the links persist, so the accrued
    // endpoint deficit repays. Repayment is capped by the CURRENT allocated
    // Q_limit_k, which has shrunk because the cell drained: after substep 1
    // volume 12 maps to h = 0.6, so V_limit = 0.9*0.4*0.6*50 = 10.8, and the
    // drainage 1/3 share gives Q_limit_k = 0.9. Thus q_repay = min(4/4,
    // 0.9) = 0.9 -> v_repay = 3.6, leaving 0.4 on the account.
    const auto second =
        advance_tri_coupling_step(state, swmm, dflowfm, make_drain_config(0.0, 0.0), kDtSub);
    const auto& drainage2 = decision_for(second.surface_decisions, SharedExchangeEngine::drainage);
    const auto& river2 = decision_for(second.surface_decisions, SharedExchangeEngine::river);
    EXPECT_DOUBLE_EQ(drainage2.exchange.v_repay, 3.6);
    EXPECT_DOUBLE_EQ(drainage2.exchange.v_granted, 0.0);
    EXPECT_DOUBLE_EQ(river2.exchange.v_repay, 0.0);
    EXPECT_DOUBLE_EQ(river2.exchange.v_granted, 0.0);
    EXPECT_DOUBLE_EQ(swmm.get_node_lateral_inflow(kNodeId), 0.9);
    EXPECT_DOUBLE_EQ(dflowfm.get_value("lateral_discharge", kLocationId), 0.0);
    EXPECT_DOUBLE_EQ(state.cells()[0].volume, 8.4);
    for (const auto& account : state.cells()[0].shared_deficit_accounts) {
        if (account.endpoint.engine == SharedExchangeEngine::drainage) {
            EXPECT_NEAR(account.mass_deficit_account.volume, 0.4, 1.0e-9);
        } else {
            EXPECT_DOUBLE_EQ(account.mass_deficit_account.volume, 0.0);
        }
    }

    // Substep 3: the residual 0.4 m3 clears in full (well under the current
    // Q_limit_k), settling the ledger.
    const auto third =
        advance_tri_coupling_step(state, swmm, dflowfm, make_drain_config(0.0, 0.0), kDtSub);
    const auto& drainage3 = decision_for(third.surface_decisions, SharedExchangeEngine::drainage);
    EXPECT_NEAR(drainage3.exchange.v_repay, 0.4, 1.0e-9);
    EXPECT_NEAR(swmm.get_node_lateral_inflow(kNodeId), 0.1, 1.0e-9);
    EXPECT_NEAR(state.cells()[0].volume, 8.0, 1.0e-9);
    for (const auto& account : state.cells()[0].shared_deficit_accounts) {
        EXPECT_NEAR(account.mass_deficit_account.volume, 0.0, 1.0e-9);
    }

    // Substep 4: ledger settled -> the coupled system is fully quiescent.
    const auto fourth =
        advance_tri_coupling_step(state, swmm, dflowfm, make_drain_config(0.0, 0.0), kDtSub);
    for (const auto& decision : fourth.surface_decisions) {
        EXPECT_DOUBLE_EQ(decision.exchange.v_granted, 0.0);
        EXPECT_DOUBLE_EQ(decision.exchange.v_repay, 0.0);
        EXPECT_DOUBLE_EQ(decision.exchange.v_unmet, 0.0);
    }
    EXPECT_DOUBLE_EQ(swmm.get_node_lateral_inflow(kNodeId), 0.0);
    EXPECT_DOUBLE_EQ(dflowfm.get_value("lateral_discharge", kLocationId), 0.0);
    EXPECT_NEAR(state.cells()[0].volume, 8.0, 1.0e-9);
    EXPECT_DOUBLE_EQ(fourth.surface_mass_after.surface_mass, fourth.surface_mass_before.surface_mass);
}

TEST(GoldenDualEngineSharedCell, ReplayAfterRollbackIsBitwiseIdentical) {
    // Reference run.
    auto fresh = make_shared_cell_state();
    {
        MockSwmmEngine swmm;
        MockDFlowFMEngine dflowfm;
        swmm.initialize("mock.inp");
        dflowfm.initialize("mock.mdu");
        (void)advance_tri_coupling_step(fresh, swmm, dflowfm, make_drain_config(4.0, 4.0), kDtSub);
        (void)advance_tri_coupling_step(fresh, swmm, dflowfm, make_drain_config(0.0, 0.0), kDtSub);
    }

    // Same schedule after a corrupting event + snapshot rollback.
    auto rolled_back = make_shared_cell_state();
    {
        MockSwmmEngine swmm;
        MockDFlowFMEngine dflowfm;
        swmm.initialize("mock.inp");
        dflowfm.initialize("mock.mdu");
        const auto snapshot = rolled_back.snapshot();
        rolled_back.enqueue_event(
            {.exchange_cell_index = 0U,
             .direction = scau::coupling::core::ExchangeDirection::engine_to_surface,
             .volume_delta = 999.0});
        rolled_back.replay_pending();
        rolled_back.rollback(snapshot);
        (void)advance_tri_coupling_step(
            rolled_back, swmm, dflowfm, make_drain_config(4.0, 4.0), kDtSub);
        (void)advance_tri_coupling_step(
            rolled_back, swmm, dflowfm, make_drain_config(0.0, 0.0), kDtSub);
    }

    ASSERT_EQ(fresh.cells().size(), rolled_back.cells().size());
    EXPECT_DOUBLE_EQ(fresh.cells()[0].volume, rolled_back.cells()[0].volume);
    EXPECT_DOUBLE_EQ(
        fresh.cells()[0].mass_deficit_account.volume,
        rolled_back.cells()[0].mass_deficit_account.volume);
    ASSERT_EQ(
        fresh.cells()[0].shared_deficit_accounts.size(),
        rolled_back.cells()[0].shared_deficit_accounts.size());
    for (std::size_t i = 0; i < fresh.cells()[0].shared_deficit_accounts.size(); ++i) {
        const auto& lhs = fresh.cells()[0].shared_deficit_accounts[i];
        const auto& rhs = rolled_back.cells()[0].shared_deficit_accounts[i];
        EXPECT_EQ(lhs.endpoint.engine, rhs.endpoint.engine);
        EXPECT_EQ(lhs.endpoint.node_id, rhs.endpoint.node_id);
        EXPECT_DOUBLE_EQ(lhs.mass_deficit_account.volume, rhs.mass_deficit_account.volume);
    }
}

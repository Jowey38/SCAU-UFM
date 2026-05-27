#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "coupling/core/state.hpp"

TEST(CouplingCoreState, SnapshotCapturesCurrentCellsAndRemainsImmutable) {
    scau::coupling::core::CouplingState state{{
        {.volume = 10.0},
        {.volume = 20.0},
    }};

    const auto snapshot = state.snapshot();
    state.enqueue_event({.exchange_cell_index = 0U, .volume_delta = 5.0});
    state.replay_pending();

    ASSERT_EQ(snapshot.cells().size(), 2U);
    EXPECT_DOUBLE_EQ(snapshot.cells()[0].volume, 10.0);
    EXPECT_DOUBLE_EQ(snapshot.cells()[1].volume, 20.0);
    EXPECT_DOUBLE_EQ(state.cells()[0].volume, 15.0);
}

TEST(CouplingCoreState, RollbackRestoresSnapshotVolumes) {
    scau::coupling::core::CouplingState state{{
        {.volume = 10.0},
        {.volume = 20.0},
    }};

    const auto snapshot = state.snapshot();
    state.enqueue_event({.exchange_cell_index = 0U, .volume_delta = -3.0});
    state.enqueue_event({.exchange_cell_index = 1U, .volume_delta = 7.0});
    state.replay_pending();
    state.rollback(snapshot);

    ASSERT_EQ(state.cells().size(), 2U);
    EXPECT_DOUBLE_EQ(state.cells()[0].volume, 10.0);
    EXPECT_DOUBLE_EQ(state.cells()[1].volume, 20.0);
}

TEST(CouplingCoreState, RollbackPreservesPendingEventsForReplay) {
    scau::coupling::core::CouplingState state{{
        {.volume = 10.0},
        {.volume = 20.0},
    }};

    const auto snapshot = state.snapshot();
    state.enqueue_event({.exchange_cell_index = 0U, .volume_delta = 2.0});
    state.enqueue_event({.exchange_cell_index = 0U, .volume_delta = 3.0});
    state.enqueue_event({.exchange_cell_index = 1U, .volume_delta = -4.0});
    state.rollback(snapshot);
    state.replay_pending();

    ASSERT_EQ(state.cells().size(), 2U);
    EXPECT_DOUBLE_EQ(state.cells()[0].volume, 15.0);
    EXPECT_DOUBLE_EQ(state.cells()[1].volume, 16.0);
}

TEST(CouplingCoreState, EnqueueRejectsOutOfRangeExchangeCellIndex) {
    scau::coupling::core::CouplingState state{{
        {.volume = 10.0},
    }};

    EXPECT_THROW(
        state.enqueue_event({.exchange_cell_index = 1U, .volume_delta = 1.0}),
        std::out_of_range);
}

TEST(CouplingCoreState, SnapshotCapturesDeficitAccountsAndRemainsImmutable) {
    scau::coupling::core::CouplingState state{{
        {.volume = 10.0, .mass_deficit_account = {.volume = 1.0}},
        {.volume = 20.0, .mass_deficit_account = {.volume = 2.0}},
    }};

    const auto snapshot = state.snapshot();
    state.enqueue_event({.exchange_cell_index = 0U, .volume_delta = 5.0, .unmet_volume = 3.0});
    state.replay_pending();

    ASSERT_EQ(snapshot.cells().size(), 2U);
    EXPECT_DOUBLE_EQ(snapshot.cells()[0].mass_deficit_account.volume, 1.0);
    EXPECT_DOUBLE_EQ(snapshot.cells()[1].mass_deficit_account.volume, 2.0);
    EXPECT_DOUBLE_EQ(state.cells()[0].mass_deficit_account.volume, 4.0);
}

TEST(CouplingCoreState, RollbackRestoresSnapshotDeficitAccounts) {
    scau::coupling::core::CouplingState state{{
        {.volume = 10.0, .mass_deficit_account = {.volume = 1.0}},
        {.volume = 20.0, .mass_deficit_account = {.volume = 2.0}},
    }};

    const auto snapshot = state.snapshot();
    state.enqueue_event({.exchange_cell_index = 0U, .volume_delta = -3.0, .unmet_volume = 4.0});
    state.enqueue_event({.exchange_cell_index = 1U, .volume_delta = 7.0, .repayment_volume = 1.5});
    state.replay_pending();
    state.rollback(snapshot);

    ASSERT_EQ(state.cells().size(), 2U);
    EXPECT_DOUBLE_EQ(state.cells()[0].mass_deficit_account.volume, 1.0);
    EXPECT_DOUBLE_EQ(state.cells()[1].mass_deficit_account.volume, 2.0);
}

TEST(CouplingCoreState, RollbackPreservesPendingDeficitEventsForReplay) {
    scau::coupling::core::CouplingState state{{
        {.volume = 10.0, .mass_deficit_account = {.volume = 1.0}},
        {.volume = 20.0, .mass_deficit_account = {.volume = 5.0}},
    }};

    const auto snapshot = state.snapshot();
    state.enqueue_event({.exchange_cell_index = 0U, .volume_delta = 2.0, .unmet_volume = 3.0});
    state.enqueue_event({.exchange_cell_index = 1U, .volume_delta = -4.0, .repayment_volume = 2.5});
    state.rollback(snapshot);
    state.replay_pending();

    ASSERT_EQ(state.cells().size(), 2U);
    EXPECT_DOUBLE_EQ(state.cells()[0].volume, 12.0);
    EXPECT_DOUBLE_EQ(state.cells()[0].mass_deficit_account.volume, 4.0);
    EXPECT_DOUBLE_EQ(state.cells()[1].volume, 16.0);
    EXPECT_DOUBLE_EQ(state.cells()[1].mass_deficit_account.volume, 2.5);
}

TEST(CouplingCoreState, RuntimeCountersStartAtZero) {
    scau::coupling::core::CouplingState state{{{.volume = 10.0}}};

    EXPECT_EQ(state.runtime_counters().count_drain_split, 0U);
    EXPECT_EQ(state.runtime_counters().count_negative_depth_fix, 0U);
}

TEST(CouplingCoreState, RecordPipelineDecisionIncrementsDrainSplitCounter) {
    scau::coupling::core::CouplingState state{{{.volume = 10.0}}};

    state.record_pipeline_decision({
        .drain_split_engaged = true,
        .negative_depth_fix_engaged = false,
    });

    EXPECT_EQ(state.runtime_counters().count_drain_split, 1U);
    EXPECT_EQ(state.runtime_counters().count_negative_depth_fix, 0U);
}

TEST(CouplingCoreState, RecordPipelineDecisionIncrementsNegativeDepthFixCounter) {
    scau::coupling::core::CouplingState state{{{.volume = 10.0}}};

    state.record_pipeline_decision({
        .drain_split_engaged = false,
        .negative_depth_fix_engaged = true,
    });

    EXPECT_EQ(state.runtime_counters().count_drain_split, 0U);
    EXPECT_EQ(state.runtime_counters().count_negative_depth_fix, 1U);
}

TEST(CouplingCoreState, RecordPipelineDecisionIncrementsBothCountersWhenBothFlagsEngaged) {
    scau::coupling::core::CouplingState state{{{.volume = 10.0}}};

    state.record_pipeline_decision({
        .drain_split_engaged = true,
        .negative_depth_fix_engaged = true,
    });
    state.record_pipeline_decision({
        .drain_split_engaged = true,
        .negative_depth_fix_engaged = true,
    });

    EXPECT_EQ(state.runtime_counters().count_drain_split, 2U);
    EXPECT_EQ(state.runtime_counters().count_negative_depth_fix, 2U);
}

TEST(CouplingCoreState, RecordPipelineDecisionLeavesCountersUnchangedWhenNoFlagsEngaged) {
    scau::coupling::core::CouplingState state{{{.volume = 10.0}}};

    state.record_pipeline_decision({
        .drain_split_engaged = false,
        .negative_depth_fix_engaged = false,
    });

    EXPECT_EQ(state.runtime_counters().count_drain_split, 0U);
    EXPECT_EQ(state.runtime_counters().count_negative_depth_fix, 0U);
}

TEST(CouplingCoreState, SnapshotCapturesRuntimeCountersAndRemainsImmutable) {
    scau::coupling::core::CouplingState state{{{.volume = 10.0}}};
    state.record_pipeline_decision({.drain_split_engaged = true});
    state.record_pipeline_decision({.negative_depth_fix_engaged = true});

    const auto snapshot = state.snapshot();
    state.record_pipeline_decision({
        .drain_split_engaged = true,
        .negative_depth_fix_engaged = true,
    });

    EXPECT_EQ(snapshot.runtime_counters().count_drain_split, 1U);
    EXPECT_EQ(snapshot.runtime_counters().count_negative_depth_fix, 1U);
    EXPECT_EQ(state.runtime_counters().count_drain_split, 2U);
    EXPECT_EQ(state.runtime_counters().count_negative_depth_fix, 2U);
}

TEST(CouplingCoreState, RollbackRestoresSnapshotRuntimeCounters) {
    scau::coupling::core::CouplingState state{{{.volume = 10.0}}};
    state.record_pipeline_decision({.drain_split_engaged = true});

    const auto snapshot = state.snapshot();
    state.record_pipeline_decision({
        .drain_split_engaged = true,
        .negative_depth_fix_engaged = true,
    });
    state.rollback(snapshot);

    EXPECT_EQ(state.runtime_counters().count_drain_split, 1U);
    EXPECT_EQ(state.runtime_counters().count_negative_depth_fix, 0U);
}

TEST(CouplingCoreState, ReplayPendingDoesNotMutateRuntimeCounters) {
    scau::coupling::core::CouplingState state{{{.volume = 10.0}}};
    state.record_pipeline_decision({.drain_split_engaged = true});

    state.enqueue_event({.exchange_cell_index = 0U, .volume_delta = 2.0, .unmet_volume = 1.0});
    state.replay_pending();

    EXPECT_EQ(state.runtime_counters().count_drain_split, 1U);
    EXPECT_EQ(state.runtime_counters().count_negative_depth_fix, 0U);
    EXPECT_DOUBLE_EQ(state.cells()[0].volume, 12.0);
    EXPECT_DOUBLE_EQ(state.cells()[0].mass_deficit_account.volume, 1.0);
}

TEST(CouplingCoreState, DeterministicReplayPreservesCellsAndCountersWithIdenticalSequence) {
    const std::vector<scau::coupling::core::ExchangeCellState> initial_cells{
        {.volume = 10.0, .mass_deficit_account = {.volume = 1.0}},
        {.volume = 20.0, .mass_deficit_account = {.volume = 2.0}},
    };

    scau::coupling::core::CouplingState state_a{initial_cells};
    scau::coupling::core::CouplingState state_b{initial_cells};

    const auto drive = [](scau::coupling::core::CouplingState& state) {
        state.record_pipeline_decision({.drain_split_engaged = true});
        state.enqueue_event({.exchange_cell_index = 0U, .volume_delta = 4.0, .unmet_volume = 1.5});
        state.enqueue_event({.exchange_cell_index = 1U, .volume_delta = -2.0, .repayment_volume = 0.5});
        state.replay_pending();
        state.record_pipeline_decision({.negative_depth_fix_engaged = true});
        state.record_pipeline_decision({.drain_split_engaged = true, .negative_depth_fix_engaged = true});
    };

    drive(state_a);
    drive(state_b);

    ASSERT_EQ(state_a.cells().size(), state_b.cells().size());
    for (std::size_t i = 0; i < state_a.cells().size(); ++i) {
        EXPECT_DOUBLE_EQ(state_a.cells()[i].volume, state_b.cells()[i].volume);
        EXPECT_DOUBLE_EQ(
            state_a.cells()[i].mass_deficit_account.volume,
            state_b.cells()[i].mass_deficit_account.volume);
    }
    EXPECT_EQ(
        state_a.runtime_counters().count_drain_split,
        state_b.runtime_counters().count_drain_split);
    EXPECT_EQ(
        state_a.runtime_counters().count_negative_depth_fix,
        state_b.runtime_counters().count_negative_depth_fix);
}

TEST(CouplingCoreState, RollbackAndReplayProducesIdenticalStateToFreshRun) {
    const std::vector<scau::coupling::core::ExchangeCellState> initial_cells{
        {.volume = 10.0, .mass_deficit_account = {.volume = 1.0}},
        {.volume = 20.0, .mass_deficit_account = {.volume = 2.0}},
    };

    scau::coupling::core::CouplingState fresh{initial_cells};
    scau::coupling::core::CouplingState rolled_back{initial_cells};

    const auto authoritative_sequence = [](scau::coupling::core::CouplingState& state) {
        state.record_pipeline_decision({.drain_split_engaged = true});
        state.enqueue_event({.exchange_cell_index = 0U, .volume_delta = 3.0, .unmet_volume = 2.0});
        state.enqueue_event({.exchange_cell_index = 1U, .volume_delta = -1.0, .repayment_volume = 1.0});
        state.replay_pending();
        state.record_pipeline_decision({.negative_depth_fix_engaged = true});
    };

    const auto fresh_snapshot = fresh.snapshot();
    authoritative_sequence(fresh);

    const auto rolled_back_snapshot = rolled_back.snapshot();
    rolled_back.record_pipeline_decision({.drain_split_engaged = true, .negative_depth_fix_engaged = true});
    rolled_back.enqueue_event({.exchange_cell_index = 0U, .volume_delta = 999.0, .unmet_volume = 999.0});
    rolled_back.replay_pending();
    rolled_back.rollback(rolled_back_snapshot);
    authoritative_sequence(rolled_back);

    ASSERT_EQ(fresh.cells().size(), rolled_back.cells().size());
    for (std::size_t i = 0; i < fresh.cells().size(); ++i) {
        EXPECT_DOUBLE_EQ(fresh.cells()[i].volume, rolled_back.cells()[i].volume);
        EXPECT_DOUBLE_EQ(
            fresh.cells()[i].mass_deficit_account.volume,
            rolled_back.cells()[i].mass_deficit_account.volume);
    }
    EXPECT_EQ(
        fresh.runtime_counters().count_drain_split,
        rolled_back.runtime_counters().count_drain_split);
    EXPECT_EQ(
        fresh.runtime_counters().count_negative_depth_fix,
        rolled_back.runtime_counters().count_negative_depth_fix);
    static_cast<void>(fresh_snapshot);
}

TEST(CouplingCoreState, ReReplayAfterRollbackAndReEnqueueIsBitwiseIdentical) {
    scau::coupling::core::CouplingState state{{
        {.volume = 10.0, .mass_deficit_account = {.volume = 1.0}},
        {.volume = 20.0, .mass_deficit_account = {.volume = 5.0}},
    }};

    const auto snapshot = state.snapshot();
    const std::vector<scau::coupling::core::CouplingEvent> input_events{
        {.exchange_cell_index = 0U, .volume_delta = 2.0, .unmet_volume = 3.0},
        {.exchange_cell_index = 1U, .volume_delta = -4.0, .repayment_volume = 2.5},
    };

    for (const auto& event : input_events) {
        state.enqueue_event(event);
    }
    state.replay_pending();
    state.record_pipeline_decision({.drain_split_engaged = true});

    const std::vector<scau::coupling::core::ExchangeCellState> first_cells(state.cells());
    const auto first_counters = state.runtime_counters();

    state.rollback(snapshot);
    for (const auto& event : input_events) {
        state.enqueue_event(event);
    }
    state.replay_pending();
    state.record_pipeline_decision({.drain_split_engaged = true});

    ASSERT_EQ(state.cells().size(), first_cells.size());
    for (std::size_t i = 0; i < state.cells().size(); ++i) {
        EXPECT_DOUBLE_EQ(state.cells()[i].volume, first_cells[i].volume);
        EXPECT_DOUBLE_EQ(
            state.cells()[i].mass_deficit_account.volume,
            first_cells[i].mass_deficit_account.volume);
    }
    EXPECT_EQ(state.runtime_counters().count_drain_split, first_counters.count_drain_split);
    EXPECT_EQ(
        state.runtime_counters().count_negative_depth_fix,
        first_counters.count_negative_depth_fix);
}

TEST(CouplingCoreState, ComputeSystemMassAuditsCurrentCellsWithoutMutatingState) {
    constexpr double kHWet = 1.0e-4;
    scau::coupling::core::CouplingState state{{
        {
            .volume = 10.0,
            .mass_deficit_account = {.volume = 1.0},
            .phi_t = 0.4,
            .h = 2.0,
            .area = 50.0,
        },
        {
            .volume = 20.0,
            .mass_deficit_account = {.volume = 3.0},
            .phi_t = 0.4,
            .h = kHWet / 2.0,
            .area = 50.0,
        },
    }};

    const auto audit = state.compute_system_mass(kHWet);

    EXPECT_DOUBLE_EQ(audit.surface_mass, 40.0);
    EXPECT_DOUBLE_EQ(audit.deficit_mass, 4.0);
    EXPECT_DOUBLE_EQ(audit.total_mass, 44.0);
    EXPECT_EQ(audit.wet_cell_count, 1U);
    ASSERT_EQ(state.cells().size(), 2U);
    EXPECT_DOUBLE_EQ(state.cells()[0].volume, 10.0);
    EXPECT_DOUBLE_EQ(state.cells()[1].volume, 20.0);
}

TEST(CouplingCoreState, AuditSystemMassAgainstReferenceUsesCurrentCells) {
    constexpr double kHWet = 1.0e-4;
    scau::coupling::core::CouplingState state{{{
        .volume = 10.0,
        .mass_deficit_account = {.volume = 0.0},
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
    }}};

    const auto baseline = state.compute_system_mass(kHWet);
    state.enqueue_event({.exchange_cell_index = 0U, .volume_delta = 0.0, .unmet_volume = 4.0});
    state.replay_pending();

    const auto delta = state.audit_system_mass_against_reference(baseline, kHWet);

    EXPECT_DOUBLE_EQ(delta.baseline.total_mass, 40.0);
    EXPECT_DOUBLE_EQ(delta.current.deficit_mass, 4.0);
    EXPECT_DOUBLE_EQ(delta.current.total_mass, 44.0);
    EXPECT_DOUBLE_EQ(delta.residual, 4.0);
    EXPECT_FALSE(delta.conserved);
}

TEST(CouplingCoreState, SnapshotComputeSystemMassUsesCapturedCellsAfterStateMutates) {
    constexpr double kHWet = 1.0e-4;
    scau::coupling::core::CouplingState state{{{
        .volume = 10.0,
        .mass_deficit_account = {.volume = 1.0},
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
    }}};

    const auto snapshot = state.snapshot();
    state.enqueue_event({.exchange_cell_index = 0U, .volume_delta = 0.0, .unmet_volume = 4.0});
    state.replay_pending();

    const auto snapshot_audit = snapshot.compute_system_mass(kHWet);
    const auto state_audit = state.compute_system_mass(kHWet);

    EXPECT_DOUBLE_EQ(snapshot_audit.surface_mass, 40.0);
    EXPECT_DOUBLE_EQ(snapshot_audit.deficit_mass, 1.0);
    EXPECT_DOUBLE_EQ(snapshot_audit.total_mass, 41.0);
    EXPECT_DOUBLE_EQ(state_audit.deficit_mass, 5.0);
}

TEST(CouplingCoreState, AuditSystemMassAgainstSnapshotUsesSnapshotBaselineAndCurrentCells) {
    constexpr double kHWet = 1.0e-4;
    scau::coupling::core::CouplingState state{{{
        .volume = 10.0,
        .mass_deficit_account = {.volume = 1.0},
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
    }}};

    const auto baseline = state.snapshot();
    state.enqueue_event({.exchange_cell_index = 0U, .volume_delta = 0.0, .unmet_volume = 4.0});
    state.replay_pending();

    const auto delta = state.audit_system_mass_against_snapshot(baseline, kHWet);

    EXPECT_DOUBLE_EQ(delta.baseline.total_mass, 41.0);
    EXPECT_DOUBLE_EQ(delta.current.total_mass, 45.0);
    EXPECT_DOUBLE_EQ(delta.residual, 4.0);
    EXPECT_FALSE(delta.conserved);
}

TEST(CouplingCoreState, DiagnoseSystemMassAgainstSnapshotUsesSnapshotBaselineAndCurrentCells) {
    constexpr double kHWet = 1.0e-4;
    scau::coupling::core::CouplingState state{{{
        .volume = 10.0,
        .mass_deficit_account = {.volume = 1.0},
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
    }}};

    const auto baseline = state.snapshot();
    state.enqueue_event({.exchange_cell_index = 0U, .volume_delta = 0.0, .unmet_volume = 4.0});
    state.replay_pending();

    const auto diagnostic = state.diagnose_system_mass_against_snapshot(baseline, kHWet);

    EXPECT_EQ(diagnostic.status, scau::coupling::core::SystemMassConservationStatus::drifted);
    EXPECT_DOUBLE_EQ(diagnostic.residual, 4.0);
    EXPECT_DOUBLE_EQ(diagnostic.baseline_total_mass, 41.0);
    EXPECT_DOUBLE_EQ(diagnostic.current_total_mass, 45.0);
}

TEST(CouplingCoreState, DiagnoseSystemMassAgainstReferenceContinuesWhenMassIsConserved) {
    constexpr double kHWet = 1.0e-4;
    scau::coupling::core::CouplingState state{{{
        .volume = 10.0,
        .mass_deficit_account = {.volume = 1.0},
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
    }}};

    const auto baseline = state.compute_system_mass(kHWet);

    const auto diagnostic = state.diagnose_system_mass_against_reference(baseline, kHWet);

    EXPECT_EQ(diagnostic.status, scau::coupling::core::SystemMassConservationStatus::conserved);
    EXPECT_DOUBLE_EQ(diagnostic.residual, 0.0);
    EXPECT_DOUBLE_EQ(diagnostic.baseline_total_mass, 41.0);
    EXPECT_DOUBLE_EQ(diagnostic.current_total_mass, 41.0);
}

TEST(CouplingCoreState, DiagnoseSystemMassAgainstReferenceDetectsMassDrift) {
    constexpr double kHWet = 1.0e-4;
    scau::coupling::core::CouplingState state{{{
        .volume = 10.0,
        .mass_deficit_account = {.volume = 1.0},
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
    }}};

    const auto baseline = state.compute_system_mass(kHWet);
    state.enqueue_event({.exchange_cell_index = 0U, .volume_delta = 0.0, .unmet_volume = 4.0});
    state.replay_pending();

    const auto diagnostic = state.diagnose_system_mass_against_reference(baseline, kHWet);

    EXPECT_EQ(diagnostic.status, scau::coupling::core::SystemMassConservationStatus::drifted);
    EXPECT_DOUBLE_EQ(diagnostic.residual, 4.0);
    EXPECT_DOUBLE_EQ(diagnostic.baseline_total_mass, 41.0);
    EXPECT_DOUBLE_EQ(diagnostic.current_total_mass, 45.0);
}

TEST(CouplingCoreState, GateDecisionAgainstReferenceContinuesWhenMassIsConserved) {
    constexpr double kHWet = 1.0e-4;
    scau::coupling::core::CouplingState state{{{
        .volume = 10.0,
        .mass_deficit_account = {.volume = 1.0},
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
    }}};

    const auto baseline = state.compute_system_mass(kHWet);

    const auto decision = state.decide_system_mass_gate_action_against_reference(baseline, kHWet);

    EXPECT_EQ(decision.action, scau::coupling::core::SystemMassGateAction::continue_run);
    EXPECT_EQ(decision.diagnostic.status, scau::coupling::core::SystemMassConservationStatus::conserved);
    EXPECT_DOUBLE_EQ(decision.diagnostic.residual, 0.0);
    EXPECT_DOUBLE_EQ(decision.diagnostic.baseline_total_mass, 41.0);
    EXPECT_DOUBLE_EQ(decision.diagnostic.current_total_mass, 41.0);
}

TEST(CouplingCoreState, GateDecisionAgainstReferenceAbortsWhenMassDrifted) {
    constexpr double kHWet = 1.0e-4;
    scau::coupling::core::CouplingState state{{{
        .volume = 10.0,
        .mass_deficit_account = {.volume = 1.0},
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
    }}};

    const auto baseline = state.compute_system_mass(kHWet);
    state.enqueue_event({.exchange_cell_index = 0U, .volume_delta = 0.0, .unmet_volume = 4.0});
    state.replay_pending();

    const auto decision = state.decide_system_mass_gate_action_against_reference(baseline, kHWet);

    EXPECT_EQ(decision.action, scau::coupling::core::SystemMassGateAction::abort_run);
    EXPECT_EQ(decision.diagnostic.status, scau::coupling::core::SystemMassConservationStatus::drifted);
    EXPECT_DOUBLE_EQ(decision.diagnostic.residual, 4.0);
    EXPECT_DOUBLE_EQ(decision.diagnostic.baseline_total_mass, 41.0);
    EXPECT_DOUBLE_EQ(decision.diagnostic.current_total_mass, 45.0);
}

TEST(CouplingCoreState, GateDecisionAgainstSnapshotContinuesWhenMassIsConserved) {
    constexpr double kHWet = 1.0e-4;
    scau::coupling::core::CouplingState state{{{
        .volume = 10.0,
        .mass_deficit_account = {.volume = 1.0},
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
    }}};

    const auto baseline = state.snapshot();

    const auto decision = state.decide_system_mass_gate_action_against_snapshot(baseline, kHWet);

    EXPECT_EQ(decision.action, scau::coupling::core::SystemMassGateAction::continue_run);
    EXPECT_EQ(decision.diagnostic.status, scau::coupling::core::SystemMassConservationStatus::conserved);
    EXPECT_DOUBLE_EQ(decision.diagnostic.residual, 0.0);
    EXPECT_DOUBLE_EQ(decision.diagnostic.baseline_total_mass, 41.0);
    EXPECT_DOUBLE_EQ(decision.diagnostic.current_total_mass, 41.0);
}

TEST(CouplingCoreState, GateDecisionAgainstSnapshotAbortsWhenMassDrifted) {
    constexpr double kHWet = 1.0e-4;
    scau::coupling::core::CouplingState state{{{
        .volume = 10.0,
        .mass_deficit_account = {.volume = 1.0},
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
    }}};

    const auto baseline = state.snapshot();
    state.enqueue_event({.exchange_cell_index = 0U, .volume_delta = 0.0, .unmet_volume = 4.0});
    state.replay_pending();

    const auto decision = state.decide_system_mass_gate_action_against_snapshot(baseline, kHWet);

    EXPECT_EQ(decision.action, scau::coupling::core::SystemMassGateAction::abort_run);
    EXPECT_EQ(decision.diagnostic.status, scau::coupling::core::SystemMassConservationStatus::drifted);
    EXPECT_DOUBLE_EQ(decision.diagnostic.residual, 4.0);
    EXPECT_DOUBLE_EQ(decision.diagnostic.baseline_total_mass, 41.0);
    EXPECT_DOUBLE_EQ(decision.diagnostic.current_total_mass, 45.0);
}

TEST(CouplingCoreState, RuntimeGateOutcomeHelperKeepsRunningForContinueDecision) {
    const scau::coupling::core::SystemMassGateDecision decision{
        .action = scau::coupling::core::SystemMassGateAction::continue_run,
        .diagnostic = {
            .status = scau::coupling::core::SystemMassConservationStatus::conserved,
            .residual = 0.0,
            .baseline_total_mass = 41.0,
            .current_total_mass = 41.0,
        },
    };

    const auto outcome = scau::coupling::core::make_system_mass_runtime_gate_outcome(decision);

    EXPECT_EQ(outcome.status, scau::coupling::core::SystemMassRuntimeGateStatus::running);
    EXPECT_EQ(outcome.decision.action, scau::coupling::core::SystemMassGateAction::continue_run);
    EXPECT_EQ(outcome.decision.diagnostic.status, scau::coupling::core::SystemMassConservationStatus::conserved);
    EXPECT_DOUBLE_EQ(outcome.decision.diagnostic.baseline_total_mass, 41.0);
    EXPECT_DOUBLE_EQ(outcome.decision.diagnostic.current_total_mass, 41.0);
}

TEST(CouplingCoreState, RuntimeGateOutcomeHelperAbortsForAbortDecision) {
    const scau::coupling::core::SystemMassGateDecision decision{
        .action = scau::coupling::core::SystemMassGateAction::abort_run,
        .diagnostic = {
            .status = scau::coupling::core::SystemMassConservationStatus::drifted,
            .residual = 4.0,
            .baseline_total_mass = 41.0,
            .current_total_mass = 45.0,
        },
    };

    const auto outcome = scau::coupling::core::make_system_mass_runtime_gate_outcome(decision);

    EXPECT_EQ(outcome.status, scau::coupling::core::SystemMassRuntimeGateStatus::abort);
    EXPECT_EQ(outcome.decision.action, scau::coupling::core::SystemMassGateAction::abort_run);
    EXPECT_EQ(outcome.decision.diagnostic.status, scau::coupling::core::SystemMassConservationStatus::drifted);
    EXPECT_DOUBLE_EQ(outcome.decision.diagnostic.residual, 4.0);
    EXPECT_DOUBLE_EQ(outcome.decision.diagnostic.current_total_mass, 45.0);
}

TEST(CouplingCoreState, RuntimeGateOutcomeHelperKeepsRunningForConservedDiagnostic) {
    const scau::coupling::core::SystemMassConservationDiagnostic diagnostic{
        .status = scau::coupling::core::SystemMassConservationStatus::conserved,
        .residual = 0.0,
        .baseline_total_mass = 41.0,
        .current_total_mass = 41.0,
    };

    const auto outcome = scau::coupling::core::make_system_mass_runtime_gate_outcome(diagnostic);

    EXPECT_EQ(outcome.status, scau::coupling::core::SystemMassRuntimeGateStatus::running);
    EXPECT_EQ(outcome.decision.action, scau::coupling::core::SystemMassGateAction::continue_run);
    EXPECT_EQ(outcome.decision.diagnostic.status, scau::coupling::core::SystemMassConservationStatus::conserved);
    EXPECT_DOUBLE_EQ(outcome.decision.diagnostic.baseline_total_mass, 41.0);
    EXPECT_DOUBLE_EQ(outcome.decision.diagnostic.current_total_mass, 41.0);
}

TEST(CouplingCoreState, RuntimeGateOutcomeHelperAbortsForDriftedDiagnostic) {
    const scau::coupling::core::SystemMassConservationDiagnostic diagnostic{
        .status = scau::coupling::core::SystemMassConservationStatus::drifted,
        .residual = 4.0,
        .baseline_total_mass = 41.0,
        .current_total_mass = 45.0,
    };

    const auto outcome = scau::coupling::core::make_system_mass_runtime_gate_outcome(diagnostic);

    EXPECT_EQ(outcome.status, scau::coupling::core::SystemMassRuntimeGateStatus::abort);
    EXPECT_EQ(outcome.decision.action, scau::coupling::core::SystemMassGateAction::abort_run);
    EXPECT_EQ(outcome.decision.diagnostic.status, scau::coupling::core::SystemMassConservationStatus::drifted);
    EXPECT_DOUBLE_EQ(outcome.decision.diagnostic.residual, 4.0);
    EXPECT_DOUBLE_EQ(outcome.decision.diagnostic.current_total_mass, 45.0);
}

TEST(CouplingCoreState, RuntimeGateAgainstReferenceKeepsRunningWhenMassIsConserved) {
    constexpr double kHWet = 1.0e-4;
    scau::coupling::core::CouplingState state{{{
        .volume = 10.0,
        .mass_deficit_account = {.volume = 1.0},
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
    }}};

    const auto baseline = state.compute_system_mass(kHWet);

    const auto outcome = state.evaluate_system_mass_runtime_gate_against_reference(baseline, kHWet);

    EXPECT_EQ(outcome.status, scau::coupling::core::SystemMassRuntimeGateStatus::running);
    EXPECT_EQ(outcome.decision.action, scau::coupling::core::SystemMassGateAction::continue_run);
    EXPECT_EQ(outcome.decision.diagnostic.status, scau::coupling::core::SystemMassConservationStatus::conserved);
    EXPECT_DOUBLE_EQ(outcome.decision.diagnostic.residual, 0.0);
    EXPECT_DOUBLE_EQ(outcome.decision.diagnostic.baseline_total_mass, 41.0);
    EXPECT_DOUBLE_EQ(outcome.decision.diagnostic.current_total_mass, 41.0);
}

TEST(CouplingCoreState, RuntimeGateAgainstReferenceSignalsAbortWhenMassDrifted) {
    constexpr double kHWet = 1.0e-4;
    scau::coupling::core::CouplingState state{{{
        .volume = 10.0,
        .mass_deficit_account = {.volume = 1.0},
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
    }}};

    const auto baseline = state.compute_system_mass(kHWet);
    state.enqueue_event({.exchange_cell_index = 0U, .volume_delta = 0.0, .unmet_volume = 4.0});
    state.replay_pending();

    const auto outcome = state.evaluate_system_mass_runtime_gate_against_reference(baseline, kHWet);

    EXPECT_EQ(outcome.status, scau::coupling::core::SystemMassRuntimeGateStatus::abort);
    EXPECT_EQ(outcome.decision.action, scau::coupling::core::SystemMassGateAction::abort_run);
    EXPECT_EQ(outcome.decision.diagnostic.status, scau::coupling::core::SystemMassConservationStatus::drifted);
    EXPECT_DOUBLE_EQ(outcome.decision.diagnostic.residual, 4.0);
    EXPECT_DOUBLE_EQ(outcome.decision.diagnostic.baseline_total_mass, 41.0);
    EXPECT_DOUBLE_EQ(outcome.decision.diagnostic.current_total_mass, 45.0);
}

TEST(CouplingCoreState, RuntimeGateAgainstSnapshotKeepsRunningWhenMassIsConserved) {
    constexpr double kHWet = 1.0e-4;
    scau::coupling::core::CouplingState state{{{
        .volume = 10.0,
        .mass_deficit_account = {.volume = 1.0},
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
    }}};

    const auto baseline = state.snapshot();

    const auto outcome = state.evaluate_system_mass_runtime_gate_against_snapshot(baseline, kHWet);

    EXPECT_EQ(outcome.status, scau::coupling::core::SystemMassRuntimeGateStatus::running);
    EXPECT_EQ(outcome.decision.action, scau::coupling::core::SystemMassGateAction::continue_run);
    EXPECT_EQ(outcome.decision.diagnostic.status, scau::coupling::core::SystemMassConservationStatus::conserved);
    EXPECT_DOUBLE_EQ(outcome.decision.diagnostic.residual, 0.0);
    EXPECT_DOUBLE_EQ(outcome.decision.diagnostic.baseline_total_mass, 41.0);
    EXPECT_DOUBLE_EQ(outcome.decision.diagnostic.current_total_mass, 41.0);
}

TEST(CouplingCoreState, RuntimeGateAgainstSnapshotSignalsAbortWhenMassDrifted) {
    constexpr double kHWet = 1.0e-4;
    scau::coupling::core::CouplingState state{{{
        .volume = 10.0,
        .mass_deficit_account = {.volume = 1.0},
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
    }}};

    const auto baseline = state.snapshot();
    state.enqueue_event({.exchange_cell_index = 0U, .volume_delta = 0.0, .unmet_volume = 4.0});
    state.replay_pending();

    const auto outcome = state.evaluate_system_mass_runtime_gate_against_snapshot(baseline, kHWet);

    EXPECT_EQ(outcome.status, scau::coupling::core::SystemMassRuntimeGateStatus::abort);
    EXPECT_EQ(outcome.decision.action, scau::coupling::core::SystemMassGateAction::abort_run);
    EXPECT_EQ(outcome.decision.diagnostic.status, scau::coupling::core::SystemMassConservationStatus::drifted);
    EXPECT_DOUBLE_EQ(outcome.decision.diagnostic.residual, 4.0);
    EXPECT_DOUBLE_EQ(outcome.decision.diagnostic.baseline_total_mass, 41.0);
    EXPECT_DOUBLE_EQ(outcome.decision.diagnostic.current_total_mass, 45.0);
}

TEST(CouplingCoreState, RuntimeAbortHandlingContinuesForRunningGateOutcome) {
    const scau::coupling::core::SystemMassRuntimeGateOutcome outcome{
        .decision = {
            .action = scau::coupling::core::SystemMassGateAction::continue_run,
            .diagnostic = {
                .status = scau::coupling::core::SystemMassConservationStatus::conserved,
                .residual = 0.0,
                .baseline_total_mass = 41.0,
                .current_total_mass = 41.0,
            },
        },
        .status = scau::coupling::core::SystemMassRuntimeGateStatus::running,
    };

    const auto handling = scau::coupling::core::classify_system_mass_runtime_abort_handling(outcome);

    EXPECT_EQ(handling, scau::coupling::core::SystemMassRuntimeAbortHandlingState::continue_run);
}

TEST(CouplingCoreState, RuntimeAbortHandlingSignalsAbortForAbortGateOutcome) {
    const scau::coupling::core::SystemMassRuntimeGateOutcome outcome{
        .decision = {
            .action = scau::coupling::core::SystemMassGateAction::abort_run,
            .diagnostic = {
                .status = scau::coupling::core::SystemMassConservationStatus::drifted,
                .residual = 4.0,
                .baseline_total_mass = 41.0,
                .current_total_mass = 45.0,
            },
        },
        .status = scau::coupling::core::SystemMassRuntimeGateStatus::abort,
    };

    const auto handling = scau::coupling::core::classify_system_mass_runtime_abort_handling(outcome);

    EXPECT_EQ(handling, scau::coupling::core::SystemMassRuntimeAbortHandlingState::abort);
}

TEST(CouplingCoreState, ShouldAbortSystemMassRuntimeReturnsFalseForContinueHandling) {
    const bool should_abort = scau::coupling::core::should_abort_system_mass_runtime(
        scau::coupling::core::SystemMassRuntimeAbortHandlingState::continue_run);

    EXPECT_FALSE(should_abort);
}

TEST(CouplingCoreState, ShouldAbortSystemMassRuntimeReturnsTrueForAbortHandling) {
    const bool should_abort = scau::coupling::core::should_abort_system_mass_runtime(
        scau::coupling::core::SystemMassRuntimeAbortHandlingState::abort);

    EXPECT_TRUE(should_abort);
}

TEST(CouplingCoreState, ShouldAbortSystemMassRuntimeReturnsFalseForConservedDiagnostic) {
    const scau::coupling::core::SystemMassConservationDiagnostic diagnostic{
        .status = scau::coupling::core::SystemMassConservationStatus::conserved,
        .residual = 0.0,
        .baseline_total_mass = 41.0,
        .current_total_mass = 41.0,
    };

    const bool should_abort = scau::coupling::core::should_abort_system_mass_runtime(diagnostic);

    EXPECT_FALSE(should_abort);
}

TEST(CouplingCoreState, ShouldAbortSystemMassRuntimeReturnsTrueForDriftedDiagnostic) {
    const scau::coupling::core::SystemMassConservationDiagnostic diagnostic{
        .status = scau::coupling::core::SystemMassConservationStatus::drifted,
        .residual = 4.0,
        .baseline_total_mass = 41.0,
        .current_total_mass = 45.0,
    };

    const bool should_abort = scau::coupling::core::should_abort_system_mass_runtime(diagnostic);

    EXPECT_TRUE(should_abort);
}

TEST(CouplingCoreState, RuntimeControlDecisionHelperContinuesForRunningGateOutcome) {
    const scau::coupling::core::SystemMassRuntimeGateOutcome outcome{
        .decision = {
            .action = scau::coupling::core::SystemMassGateAction::continue_run,
            .diagnostic = {
                .status = scau::coupling::core::SystemMassConservationStatus::conserved,
                .residual = 0.0,
                .baseline_total_mass = 41.0,
                .current_total_mass = 41.0,
            },
        },
        .status = scau::coupling::core::SystemMassRuntimeGateStatus::running,
    };

    const auto decision = scau::coupling::core::make_system_mass_runtime_control_decision(outcome);

    EXPECT_EQ(decision.gate_outcome.status, scau::coupling::core::SystemMassRuntimeGateStatus::running);
    EXPECT_EQ(decision.gate_outcome.decision.action, scau::coupling::core::SystemMassGateAction::continue_run);
    EXPECT_EQ(decision.handling_state, scau::coupling::core::SystemMassRuntimeAbortHandlingState::continue_run);
    EXPECT_FALSE(decision.should_abort);
    EXPECT_DOUBLE_EQ(decision.gate_outcome.decision.diagnostic.baseline_total_mass, 41.0);
    EXPECT_DOUBLE_EQ(decision.gate_outcome.decision.diagnostic.current_total_mass, 41.0);
}

TEST(CouplingCoreState, RuntimeControlDecisionHelperAbortsForAbortGateOutcome) {
    const scau::coupling::core::SystemMassRuntimeGateOutcome outcome{
        .decision = {
            .action = scau::coupling::core::SystemMassGateAction::abort_run,
            .diagnostic = {
                .status = scau::coupling::core::SystemMassConservationStatus::drifted,
                .residual = 4.0,
                .baseline_total_mass = 41.0,
                .current_total_mass = 45.0,
            },
        },
        .status = scau::coupling::core::SystemMassRuntimeGateStatus::abort,
    };

    const auto decision = scau::coupling::core::make_system_mass_runtime_control_decision(outcome);

    EXPECT_EQ(decision.gate_outcome.status, scau::coupling::core::SystemMassRuntimeGateStatus::abort);
    EXPECT_EQ(decision.gate_outcome.decision.action, scau::coupling::core::SystemMassGateAction::abort_run);
    EXPECT_EQ(decision.handling_state, scau::coupling::core::SystemMassRuntimeAbortHandlingState::abort);
    EXPECT_TRUE(decision.should_abort);
    EXPECT_DOUBLE_EQ(decision.gate_outcome.decision.diagnostic.residual, 4.0);
    EXPECT_DOUBLE_EQ(decision.gate_outcome.decision.diagnostic.current_total_mass, 45.0);
}

TEST(CouplingCoreState, RuntimeControlDecisionHelperContinuesForConservedDiagnostic) {
    const scau::coupling::core::SystemMassConservationDiagnostic diagnostic{
        .status = scau::coupling::core::SystemMassConservationStatus::conserved,
        .residual = 0.0,
        .baseline_total_mass = 41.0,
        .current_total_mass = 41.0,
    };

    const auto decision = scau::coupling::core::make_system_mass_runtime_control_decision(diagnostic);

    EXPECT_EQ(decision.gate_outcome.status, scau::coupling::core::SystemMassRuntimeGateStatus::running);
    EXPECT_EQ(decision.gate_outcome.decision.action, scau::coupling::core::SystemMassGateAction::continue_run);
    EXPECT_EQ(decision.handling_state, scau::coupling::core::SystemMassRuntimeAbortHandlingState::continue_run);
    EXPECT_FALSE(decision.should_abort);
    EXPECT_DOUBLE_EQ(decision.gate_outcome.decision.diagnostic.baseline_total_mass, 41.0);
    EXPECT_DOUBLE_EQ(decision.gate_outcome.decision.diagnostic.current_total_mass, 41.0);
}

TEST(CouplingCoreState, RuntimeControlDecisionHelperAbortsForDriftedDiagnostic) {
    const scau::coupling::core::SystemMassConservationDiagnostic diagnostic{
        .status = scau::coupling::core::SystemMassConservationStatus::drifted,
        .residual = 4.0,
        .baseline_total_mass = 41.0,
        .current_total_mass = 45.0,
    };

    const auto decision = scau::coupling::core::make_system_mass_runtime_control_decision(diagnostic);

    EXPECT_EQ(decision.gate_outcome.status, scau::coupling::core::SystemMassRuntimeGateStatus::abort);
    EXPECT_EQ(decision.gate_outcome.decision.action, scau::coupling::core::SystemMassGateAction::abort_run);
    EXPECT_EQ(decision.handling_state, scau::coupling::core::SystemMassRuntimeAbortHandlingState::abort);
    EXPECT_TRUE(decision.should_abort);
    EXPECT_DOUBLE_EQ(decision.gate_outcome.decision.diagnostic.residual, 4.0);
    EXPECT_DOUBLE_EQ(decision.gate_outcome.decision.diagnostic.current_total_mass, 45.0);
}

TEST(CouplingCoreState, ShouldAbortSystemMassRuntimeAgainstSnapshotReturnsFalseWhenMassIsConserved) {
    constexpr double kHWet = 1.0e-4;
    scau::coupling::core::CouplingState state{{{
        .volume = 10.0,
        .mass_deficit_account = {.volume = 1.0},
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
    }}};

    const auto baseline = state.snapshot();

    const bool should_abort = state.should_abort_system_mass_runtime_against_snapshot(baseline, kHWet);

    EXPECT_FALSE(should_abort);
}

TEST(CouplingCoreState, ShouldAbortSystemMassRuntimeAgainstSnapshotReturnsTrueWhenMassDrifted) {
    constexpr double kHWet = 1.0e-4;
    scau::coupling::core::CouplingState state{{{
        .volume = 10.0,
        .mass_deficit_account = {.volume = 1.0},
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
    }}};

    const auto baseline = state.snapshot();
    state.enqueue_event({.exchange_cell_index = 0U, .volume_delta = 0.0, .unmet_volume = 4.0});
    state.replay_pending();

    const bool should_abort = state.should_abort_system_mass_runtime_against_snapshot(baseline, kHWet);

    EXPECT_TRUE(should_abort);
}

TEST(CouplingCoreState, ShouldAbortSystemMassRuntimeAgainstReferenceReturnsFalseWhenMassIsConserved) {
    constexpr double kHWet = 1.0e-4;
    scau::coupling::core::CouplingState state{{{
        .volume = 10.0,
        .mass_deficit_account = {.volume = 1.0},
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
    }}};

    const auto baseline = state.compute_system_mass(kHWet);

    const bool should_abort = state.should_abort_system_mass_runtime_against_reference(baseline, kHWet);

    EXPECT_FALSE(should_abort);
}

TEST(CouplingCoreState, ShouldAbortSystemMassRuntimeAgainstReferenceReturnsTrueWhenMassDrifted) {
    constexpr double kHWet = 1.0e-4;
    scau::coupling::core::CouplingState state{{{
        .volume = 10.0,
        .mass_deficit_account = {.volume = 1.0},
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
    }}};

    const auto baseline = state.compute_system_mass(kHWet);
    state.enqueue_event({.exchange_cell_index = 0U, .volume_delta = 0.0, .unmet_volume = 4.0});
    state.replay_pending();

    const bool should_abort = state.should_abort_system_mass_runtime_against_reference(baseline, kHWet);

    EXPECT_TRUE(should_abort);
}

TEST(CouplingCoreState, RuntimeControlDecisionAgainstReferenceContinuesWhenMassIsConserved) {
    constexpr double kHWet = 1.0e-4;
    scau::coupling::core::CouplingState state{{{
        .volume = 10.0,
        .mass_deficit_account = {.volume = 1.0},
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
    }}};

    const auto baseline = state.compute_system_mass(kHWet);

    const auto decision = state.decide_system_mass_runtime_control_against_reference(baseline, kHWet);

    EXPECT_EQ(decision.gate_outcome.status, scau::coupling::core::SystemMassRuntimeGateStatus::running);
    EXPECT_EQ(decision.gate_outcome.decision.action, scau::coupling::core::SystemMassGateAction::continue_run);
    EXPECT_EQ(decision.handling_state, scau::coupling::core::SystemMassRuntimeAbortHandlingState::continue_run);
    EXPECT_FALSE(decision.should_abort);
    EXPECT_EQ(
        decision.gate_outcome.decision.diagnostic.status,
        scau::coupling::core::SystemMassConservationStatus::conserved);
    EXPECT_DOUBLE_EQ(decision.gate_outcome.decision.diagnostic.residual, 0.0);
    EXPECT_DOUBLE_EQ(decision.gate_outcome.decision.diagnostic.baseline_total_mass, 41.0);
    EXPECT_DOUBLE_EQ(decision.gate_outcome.decision.diagnostic.current_total_mass, 41.0);
}

TEST(CouplingCoreState, RuntimeControlDecisionAgainstReferenceAbortsWhenMassDrifted) {
    constexpr double kHWet = 1.0e-4;
    scau::coupling::core::CouplingState state{{{
        .volume = 10.0,
        .mass_deficit_account = {.volume = 1.0},
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
    }}};

    const auto baseline = state.compute_system_mass(kHWet);
    state.enqueue_event({.exchange_cell_index = 0U, .volume_delta = 0.0, .unmet_volume = 4.0});
    state.replay_pending();

    const auto decision = state.decide_system_mass_runtime_control_against_reference(baseline, kHWet);

    EXPECT_EQ(decision.gate_outcome.status, scau::coupling::core::SystemMassRuntimeGateStatus::abort);
    EXPECT_EQ(decision.gate_outcome.decision.action, scau::coupling::core::SystemMassGateAction::abort_run);
    EXPECT_EQ(decision.handling_state, scau::coupling::core::SystemMassRuntimeAbortHandlingState::abort);
    EXPECT_TRUE(decision.should_abort);
    EXPECT_EQ(
        decision.gate_outcome.decision.diagnostic.status,
        scau::coupling::core::SystemMassConservationStatus::drifted);
    EXPECT_DOUBLE_EQ(decision.gate_outcome.decision.diagnostic.residual, 4.0);
    EXPECT_DOUBLE_EQ(decision.gate_outcome.decision.diagnostic.baseline_total_mass, 41.0);
    EXPECT_DOUBLE_EQ(decision.gate_outcome.decision.diagnostic.current_total_mass, 45.0);
}

TEST(CouplingCoreState, RuntimeControlAbortPredicateAgainstReferenceContinuesWhenMassIsConserved) {
    constexpr double kHWet = 1.0e-4;
    scau::coupling::core::CouplingState state{{{
        .volume = 10.0,
        .mass_deficit_account = {.volume = 1.0},
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
    }}};

    const auto baseline = state.compute_system_mass(kHWet);

    const bool should_abort = state.should_abort_system_mass_runtime_control_against_reference(baseline, kHWet);

    EXPECT_FALSE(should_abort);
}

TEST(CouplingCoreState, RuntimeControlAbortPredicateAgainstReferenceAbortsWhenMassDrifted) {
    constexpr double kHWet = 1.0e-4;
    scau::coupling::core::CouplingState state{{{
        .volume = 10.0,
        .mass_deficit_account = {.volume = 1.0},
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
    }}};

    const auto baseline = state.compute_system_mass(kHWet);
    state.enqueue_event({.exchange_cell_index = 0U, .volume_delta = 0.0, .unmet_volume = 4.0});
    state.replay_pending();

    const bool should_abort = state.should_abort_system_mass_runtime_control_against_reference(baseline, kHWet);

    EXPECT_TRUE(should_abort);
}

TEST(CouplingCoreState, RuntimeControlDecisionAgainstSnapshotContinuesWhenMassIsConserved) {
    constexpr double kHWet = 1.0e-4;
    scau::coupling::core::CouplingState state{{{
        .volume = 10.0,
        .mass_deficit_account = {.volume = 1.0},
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
    }}};

    const auto baseline = state.snapshot();

    const auto decision = state.decide_system_mass_runtime_control_against_snapshot(baseline, kHWet);

    EXPECT_EQ(decision.gate_outcome.status, scau::coupling::core::SystemMassRuntimeGateStatus::running);
    EXPECT_EQ(decision.gate_outcome.decision.action, scau::coupling::core::SystemMassGateAction::continue_run);
    EXPECT_EQ(decision.handling_state, scau::coupling::core::SystemMassRuntimeAbortHandlingState::continue_run);
    EXPECT_FALSE(decision.should_abort);
    EXPECT_EQ(
        decision.gate_outcome.decision.diagnostic.status,
        scau::coupling::core::SystemMassConservationStatus::conserved);
    EXPECT_DOUBLE_EQ(decision.gate_outcome.decision.diagnostic.residual, 0.0);
    EXPECT_DOUBLE_EQ(decision.gate_outcome.decision.diagnostic.baseline_total_mass, 41.0);
    EXPECT_DOUBLE_EQ(decision.gate_outcome.decision.diagnostic.current_total_mass, 41.0);
}

TEST(CouplingCoreState, RuntimeControlDecisionAgainstSnapshotAbortsWhenMassDrifted) {
    constexpr double kHWet = 1.0e-4;
    scau::coupling::core::CouplingState state{{{
        .volume = 10.0,
        .mass_deficit_account = {.volume = 1.0},
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
    }}};

    const auto baseline = state.snapshot();
    state.enqueue_event({.exchange_cell_index = 0U, .volume_delta = 0.0, .unmet_volume = 4.0});
    state.replay_pending();

    const auto decision = state.decide_system_mass_runtime_control_against_snapshot(baseline, kHWet);

    EXPECT_EQ(decision.gate_outcome.status, scau::coupling::core::SystemMassRuntimeGateStatus::abort);
    EXPECT_EQ(decision.gate_outcome.decision.action, scau::coupling::core::SystemMassGateAction::abort_run);
    EXPECT_EQ(decision.handling_state, scau::coupling::core::SystemMassRuntimeAbortHandlingState::abort);
    EXPECT_TRUE(decision.should_abort);
    EXPECT_EQ(
        decision.gate_outcome.decision.diagnostic.status,
        scau::coupling::core::SystemMassConservationStatus::drifted);
    EXPECT_DOUBLE_EQ(decision.gate_outcome.decision.diagnostic.residual, 4.0);
    EXPECT_DOUBLE_EQ(decision.gate_outcome.decision.diagnostic.baseline_total_mass, 41.0);
    EXPECT_DOUBLE_EQ(decision.gate_outcome.decision.diagnostic.current_total_mass, 45.0);
}

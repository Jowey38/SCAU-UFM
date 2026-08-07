#include <cmath>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "coupling/driver/checkpoint_payloads.hpp"

// Deterministic hashing + PreparedModuleCheckpoint builder coverage for the
// M269 epoch commit protocol.

namespace {

using scau::coupling::core::CouplingState;
using scau::coupling::core::ExchangeCellState;
using scau::coupling::driver::CheckpointModule;
using scau::coupling::driver::CheckpointRequirements;
using scau::coupling::driver::coordinate_checkpoint_commit;
using scau::coupling::driver::hash_coupling_snapshot;
using scau::coupling::driver::hash_surface_state;
using scau::coupling::driver::prepare_coupling_checkpoint;
using scau::coupling::driver::prepare_dflowfm_checkpoint_record;
using scau::coupling::driver::prepare_sim_driver_checkpoint;
using scau::coupling::driver::prepare_surface2d_checkpoint;
using scau::coupling::driver::prepare_swmm_checkpoint;

scau::surface2d::SurfaceState make_surface_state() {
    scau::surface2d::SurfaceState state{};
    state.cells.resize(2);
    state.cells[0].conserved.h = 1.25;
    state.cells[0].conserved.hu = 0.5;
    state.cells[0].eta = 11.25;
    state.cells[1].conserved.h = 0.75;
    state.cells[1].eta = 9.75;
    return state;
}

CouplingState make_coupling_state() {
    ExchangeCellState cell{};
    cell.volume = 40.0;
    cell.phi_t = 0.4;
    cell.h = 2.0;
    cell.area = 50.0;
    return CouplingState{{cell}};
}

}  // namespace

TEST(CheckpointPayloads, HashesAreDeterministicAndUlpSensitive) {
    const auto state = make_surface_state();
    const std::string first = hash_surface_state(state);
    const std::string second = hash_surface_state(state);
    EXPECT_EQ(first, second);
    EXPECT_EQ(first.rfind("fnv1a64:", 0), 0U);

    auto perturbed = state;
    perturbed.cells[0].conserved.h =
        std::nextafter(perturbed.cells[0].conserved.h, 2.0);
    EXPECT_NE(hash_surface_state(perturbed), first);

    const auto coupling = make_coupling_state();
    const std::string snapshot_hash = hash_coupling_snapshot(coupling.snapshot());
    EXPECT_EQ(snapshot_hash, hash_coupling_snapshot(coupling.snapshot()));

    auto aged = make_coupling_state();
    // Add a deficit, then age it once; age_steps is part of the checkpoint hash.
    scau::coupling::core::CouplingEvent unmet{};
    unmet.exchange_cell_index = 0U;
    unmet.unmet_volume = 1.0;
    aged.enqueue_event(unmet);
    aged.replay_pending();
    static_cast<void>(aged.apply_deficit_writeoff());
    EXPECT_NE(hash_coupling_snapshot(aged.snapshot()), snapshot_hash);

    // Pending events are part of the snapshot identity.
    auto with_event = make_coupling_state();
    scau::coupling::core::CouplingEvent event{};
    event.exchange_cell_index = 0U;
    event.volume_delta = 1.0;
    with_event.enqueue_event(event);
    EXPECT_NE(hash_coupling_snapshot(with_event.snapshot()), snapshot_hash);
}

TEST(CheckpointPayloads, BuildersProduceCompleteMetadata) {
    const auto state = make_surface_state();
    const auto coupling = make_coupling_state();

    const auto surface_record = prepare_surface2d_checkpoint(state, 3U, 180.0);
    EXPECT_EQ(surface_record.module, CheckpointModule::surface2d);
    EXPECT_EQ(surface_record.epoch, 3U);
    EXPECT_DOUBLE_EQ(surface_record.logical_time, 180.0);
    EXPECT_EQ(surface_record.schema_version, "surface2d-state-v1");
    EXPECT_EQ(surface_record.payload_reference, "memory://surface2d/3");
    EXPECT_FALSE(surface_record.content_hash.empty());

    const auto coupling_record = prepare_coupling_checkpoint(coupling.snapshot(), 3U, 180.0);
    EXPECT_EQ(coupling_record.module, CheckpointModule::coupling);
    EXPECT_EQ(coupling_record.schema_version, "coupling-snapshot-v1");

    const auto driver_record = prepare_sim_driver_checkpoint(3U, 3U, 180.0);
    EXPECT_EQ(driver_record.module, CheckpointModule::sim_driver);
    EXPECT_EQ(driver_record.schema_version, "sim-driver-progress-v1");

    const auto swmm_record = prepare_swmm_checkpoint(180.5, 3U, 180.0);
    EXPECT_EQ(swmm_record.module, CheckpointModule::swmm);
    EXPECT_EQ(swmm_record.schema_version, "swmm-noreload-v1");

    const auto dflowfm_record = prepare_dflowfm_checkpoint_record(180.0, nullptr, 3U, 180.0);
    EXPECT_EQ(dflowfm_record.module, CheckpointModule::dflowfm);
    EXPECT_EQ(dflowfm_record.schema_version, "dflowfm-elapsed-v1");
    EXPECT_EQ(dflowfm_record.payload_reference, "memory://dflowfm/3");
}

TEST(CheckpointPayloads, BuiltRecordsSatisfyTheCoordinator) {
    const auto state = make_surface_state();
    const auto coupling = make_coupling_state();

    std::vector<scau::coupling::driver::PreparedModuleCheckpoint> prepared{
        prepare_surface2d_checkpoint(state, 5U, 300.0),
        prepare_coupling_checkpoint(coupling.snapshot(), 5U, 300.0),
        prepare_sim_driver_checkpoint(5U, 5U, 300.0),
        prepare_swmm_checkpoint(300.0, 5U, 300.0),
        prepare_dflowfm_checkpoint_record(300.0, nullptr, 5U, 300.0),
    };
    CheckpointRequirements requirements{};
    requirements.require_swmm = true;
    requirements.require_dflowfm = true;

    const auto record = coordinate_checkpoint_commit(prepared, requirements);
    EXPECT_EQ(record.status, scau::coupling::driver::CheckpointCommitStatus::committed);
    EXPECT_EQ(record.modules.size(), 5U);

    // Missing a required engine record aborts the commit.
    prepared.pop_back();
    const auto aborted = coordinate_checkpoint_commit(prepared, requirements);
    EXPECT_EQ(aborted.status, scau::coupling::driver::CheckpointCommitStatus::aborted);
    EXPECT_FALSE(aborted.reason.empty());
}

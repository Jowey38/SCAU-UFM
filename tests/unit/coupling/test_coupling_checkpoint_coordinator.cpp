#include <gtest/gtest.h>

#include "coupling/driver/checkpoint_coordinator.hpp"

namespace {

scau::coupling::driver::PreparedModuleCheckpoint prepared(
    scau::coupling::driver::CheckpointModule module,
    std::uint64_t epoch = 7U,
    double logical_time = 600.0) {
    return {
        .module = module,
        .epoch = epoch,
        .logical_time = logical_time,
        .schema_version = "1.0",
        .content_hash = "sha256-fixture",
        .payload_reference = "payload-ref",
    };
}

std::vector<scau::coupling::driver::PreparedModuleCheckpoint> base_modules() {
    using scau::coupling::driver::CheckpointModule;
    return {
        prepared(CheckpointModule::surface2d),
        prepared(CheckpointModule::coupling),
        prepared(CheckpointModule::sim_driver),
    };
}

}  // namespace

TEST(CouplingCheckpointCoordinator, CommitsCompleteSameEpochSet) {
    using namespace scau::coupling::driver;
    auto modules = base_modules();
    modules.push_back(prepared(CheckpointModule::swmm));
    modules.push_back(prepared(CheckpointModule::dflowfm));

    const auto record = coordinate_checkpoint_commit(
        modules, {.require_swmm = true, .require_dflowfm = true});
    EXPECT_EQ(record.status, CheckpointCommitStatus::committed);
    EXPECT_EQ(record.epoch, 7U);
    EXPECT_DOUBLE_EQ(record.logical_time, 600.0);
    EXPECT_TRUE(record.reason.empty());
}

TEST(CouplingCheckpointCoordinator, AbortsMissingRequiredEngine) {
    using namespace scau::coupling::driver;
    const auto record = coordinate_checkpoint_commit(
        base_modules(), {.require_swmm = true});
    EXPECT_EQ(record.status, CheckpointCommitStatus::aborted);
    EXPECT_NE(record.reason.find("SWMM"), std::string::npos);
}

TEST(CouplingCheckpointCoordinator, AbortsEpochTimeAndDuplicateDrift) {
    using namespace scau::coupling::driver;
    auto epoch = base_modules();
    epoch[1].epoch = 8U;
    EXPECT_EQ(
        coordinate_checkpoint_commit(epoch, {}).status,
        CheckpointCommitStatus::aborted);

    auto time = base_modules();
    time[2].logical_time = 660.0;
    EXPECT_EQ(
        coordinate_checkpoint_commit(time, {}).status,
        CheckpointCommitStatus::aborted);

    auto duplicate = base_modules();
    duplicate.push_back(prepared(CheckpointModule::surface2d));
    EXPECT_EQ(
        coordinate_checkpoint_commit(duplicate, {}).status,
        CheckpointCommitStatus::aborted);
}

TEST(CouplingCheckpointCoordinator, AbortsIncompleteMetadata) {
    auto modules = base_modules();
    modules[0].content_hash.clear();
    const auto record = scau::coupling::driver::coordinate_checkpoint_commit(modules, {});
    EXPECT_EQ(
        record.status,
        scau::coupling::driver::CheckpointCommitStatus::aborted);
}

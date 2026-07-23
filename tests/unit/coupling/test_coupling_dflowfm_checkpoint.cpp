#include <filesystem>
#include <fstream>
#include <string>

#include <gtest/gtest.h>

#include "coupling/driver/dflowfm_checkpoint.hpp"

namespace {

class DFlowFMCheckpointTest : public ::testing::Test {
protected:
    void SetUp() override {
        root_ = std::filesystem::temp_directory_path() / "scau_dflowfm_checkpoint_test";
        std::filesystem::remove_all(root_);
        std::filesystem::create_directories(root_);
        base_mdu_ = root_ / "base.mdu";
        state_file_ = root_ / "state_rst.nc";
        std::ofstream(base_mdu_)
            << "[model]\nProgram = D-Flow FM\n\n[time]\nTStart = 0.0\nTStop = 6000\n\n"
               "[restart]\nRestartFile =\nRestartDateTime =\n\n[output]\nRstInterval = 600\n";
        std::ofstream(state_file_) << "fixture";
    }

    void TearDown() override { std::filesystem::remove_all(root_); }

    scau::coupling::driver::DFlowFMCheckpoint checkpoint() const {
        return {
            .logical_time = 600.0,
            .source = scau::coupling::driver::DFlowFMCheckpointSource::restart_nc,
            .state_file = state_file_,
            .base_mdu = base_mdu_,
            .model_fingerprint = "model-sha256",
        };
    }

    std::filesystem::path root_;
    std::filesystem::path base_mdu_;
    std::filesystem::path state_file_;
};

std::string read_all(const std::filesystem::path& path) {
    std::ifstream input(path);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

}  // namespace

TEST_F(DFlowFMCheckpointTest, ValidatesCheckpointAndRejectsInvalidMetadata) {
    EXPECT_NO_THROW(scau::coupling::driver::validate_dflowfm_checkpoint(checkpoint()));

    auto invalid = checkpoint();
    invalid.logical_time = -1.0;
    EXPECT_THROW(scau::coupling::driver::validate_dflowfm_checkpoint(invalid), std::invalid_argument);
    invalid = checkpoint();
    invalid.model_fingerprint.clear();
    EXPECT_THROW(scau::coupling::driver::validate_dflowfm_checkpoint(invalid), std::invalid_argument);
    invalid = checkpoint();
    invalid.source = scau::coupling::driver::DFlowFMCheckpointSource::map_nc;
    EXPECT_THROW(scau::coupling::driver::validate_dflowfm_checkpoint(invalid), std::invalid_argument);
}

TEST_F(DFlowFMCheckpointTest, ChoosesFailClosedRollbackActionByCommitBarrierState) {
    using scau::coupling::driver::DFlowFMRollbackAction;
    using scau::coupling::driver::DFlowFMRollbackRequest;
    using scau::coupling::driver::decide_dflowfm_rollback_action;

    EXPECT_EQ(decide_dflowfm_rollback_action({.engine_advanced = false}),
              DFlowFMRollbackAction::memory_only);
    EXPECT_EQ(decide_dflowfm_rollback_action({.engine_advanced = true}),
              DFlowFMRollbackAction::abort_no_checkpoint);
    const auto valid = checkpoint();
    EXPECT_EQ(
        decide_dflowfm_rollback_action({.engine_advanced = true,
                                        .deterministic_replay_available = false,
                                        .checkpoint = &valid}),
        DFlowFMRollbackAction::abort_replay_unavailable);
    EXPECT_EQ(
        decide_dflowfm_rollback_action({.engine_advanced = true,
                                        .deterministic_replay_available = true,
                                        .checkpoint = &valid}),
        DFlowFMRollbackAction::checkpoint_reload);
}

TEST_F(DFlowFMCheckpointTest, WritesDeterministicRestartMduAndPreservesOtherSections) {
    const auto output = root_ / "restart.mdu";
    scau::coupling::driver::write_dflowfm_restart_mdu(checkpoint(), output);
    const std::string text = read_all(output);

    EXPECT_NE(text.find("Program = D-Flow FM"), std::string::npos);
    EXPECT_NE(text.find("TStart = 600"), std::string::npos);
    EXPECT_NE(text.find("RestartFile = " + state_file_.generic_string()), std::string::npos);
    EXPECT_NE(text.find("RstInterval = 600"), std::string::npos);
}

TEST_F(DFlowFMCheckpointTest, MapCheckpointWritesRestartDateTime) {
    auto map = checkpoint();
    map.source = scau::coupling::driver::DFlowFMCheckpointSource::map_nc;
    map.restart_datetime = "20260723001000";
    const auto output = root_ / "map_restart.mdu";
    scau::coupling::driver::write_dflowfm_restart_mdu(map, output);

    EXPECT_NE(read_all(output).find("RestartDateTime = 20260723001000"), std::string::npos);
}

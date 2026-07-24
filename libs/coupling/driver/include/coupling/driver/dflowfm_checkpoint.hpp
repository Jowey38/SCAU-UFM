#pragma once

#include <filesystem>
#include <string>

#include "coupling/river/dflowfm_engine.hpp"

namespace scau::coupling::driver {

enum class DFlowFMCheckpointSource {
    restart_nc,
    map_nc,
};

struct DFlowFMCheckpoint {
    double logical_time{0.0};
    DFlowFMCheckpointSource source{DFlowFMCheckpointSource::restart_nc};
    std::filesystem::path state_file{};
    std::string restart_datetime{};
    std::filesystem::path base_mdu{};
    std::string model_fingerprint{};
};

enum class DFlowFMRollbackAction {
    memory_only,
    checkpoint_reload,
    abort_no_checkpoint,
    abort_replay_unavailable,
};

struct DFlowFMRollbackRequest {
    bool engine_advanced{false};
    bool deterministic_replay_available{false};
    const DFlowFMCheckpoint* checkpoint{nullptr};
};

void validate_dflowfm_checkpoint(const DFlowFMCheckpoint& checkpoint);

[[nodiscard]] DFlowFMRollbackAction decide_dflowfm_rollback_action(
    const DFlowFMRollbackRequest& request);

// Produces a temporary MDU whose [restart] section points at the checkpoint and
// whose TStart matches checkpoint.logical_time. Unrelated content is preserved.
void write_dflowfm_restart_mdu(
    const DFlowFMCheckpoint& checkpoint,
    const std::filesystem::path& output_mdu);

// Executes the native file-checkpoint recovery boundary on the concrete BMI
// engine. Rollback remains orchestration-owned and is not added to the abstract
// IDFlowFMEngine interface.
void reload_dflowfm_from_checkpoint(
    river::DFlowFMEngine& engine,
    const DFlowFMCheckpoint& checkpoint,
    const std::filesystem::path& generated_mdu);

}  // namespace scau::coupling::driver

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace scau::coupling::driver {

enum class CheckpointModule {
    surface2d,
    coupling,
    swmm,
    dflowfm,
    sim_driver,
};

struct PreparedModuleCheckpoint {
    CheckpointModule module{CheckpointModule::surface2d};
    std::uint64_t epoch{0U};
    double logical_time{0.0};
    std::string schema_version;
    std::string content_hash;
    std::string payload_reference;
};

struct CheckpointRequirements {
    bool require_swmm{false};
    bool require_dflowfm{false};
};

enum class CheckpointCommitStatus {
    prepared,
    committed,
    aborted,
};

struct CheckpointCommitRecord {
    std::uint64_t epoch{0U};
    double logical_time{0.0};
    CheckpointCommitStatus status{CheckpointCommitStatus::aborted};
    std::vector<PreparedModuleCheckpoint> modules;
    std::string reason;
};

// Validates same-epoch, same-time, unique-module prepared records and commits
// only when every required module is present. This is the project-level atomic
// visibility boundary; module-specific serialization remains adapter-owned.
[[nodiscard]] CheckpointCommitRecord coordinate_checkpoint_commit(
    const std::vector<PreparedModuleCheckpoint>& prepared,
    const CheckpointRequirements& requirements);

}  // namespace scau::coupling::driver

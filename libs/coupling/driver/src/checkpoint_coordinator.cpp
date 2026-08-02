#include "coupling/driver/checkpoint_coordinator.hpp"

#include <cmath>
#include <set>
#include <string>

namespace scau::coupling::driver {

namespace {

bool has_module(
    const std::set<CheckpointModule>& modules,
    CheckpointModule module) {
    return modules.find(module) != modules.end();
}

CheckpointCommitRecord abort_record(
    const std::vector<PreparedModuleCheckpoint>& prepared,
    const std::string& reason) {
    return CheckpointCommitRecord{
        .epoch = prepared.empty() ? 0U : prepared.front().epoch,
        .logical_time = prepared.empty() ? 0.0 : prepared.front().logical_time,
        .status = CheckpointCommitStatus::aborted,
        .modules = prepared,
        .reason = reason,
    };
}

}  // namespace

CheckpointCommitRecord coordinate_checkpoint_commit(
    const std::vector<PreparedModuleCheckpoint>& prepared,
    const CheckpointRequirements& requirements) {
    if (prepared.empty()) {
        return abort_record(prepared, "no module checkpoints prepared");
    }

    const std::uint64_t epoch = prepared.front().epoch;
    const double logical_time = prepared.front().logical_time;
    if (epoch == 0U || !std::isfinite(logical_time) || logical_time < 0.0) {
        return abort_record(prepared, "invalid checkpoint epoch or logical time");
    }

    std::set<CheckpointModule> modules;
    for (const auto& module : prepared) {
        if (module.epoch != epoch) {
            return abort_record(prepared, "module checkpoint epoch mismatch");
        }
        if (!std::isfinite(module.logical_time) || module.logical_time != logical_time) {
            return abort_record(prepared, "module checkpoint logical time mismatch");
        }
        if (module.schema_version.empty() || module.content_hash.empty()
            || module.payload_reference.empty()) {
            return abort_record(prepared, "module checkpoint metadata incomplete");
        }
        if (!modules.insert(module.module).second) {
            return abort_record(prepared, "duplicate module checkpoint");
        }
    }

    const bool base_complete = has_module(modules, CheckpointModule::surface2d)
        && has_module(modules, CheckpointModule::coupling)
        && has_module(modules, CheckpointModule::sim_driver);
    if (!base_complete) {
        return abort_record(prepared, "required base module checkpoint missing");
    }
    if (requirements.require_swmm && !has_module(modules, CheckpointModule::swmm)) {
        return abort_record(prepared, "required SWMM checkpoint missing");
    }
    if (requirements.require_dflowfm && !has_module(modules, CheckpointModule::dflowfm)) {
        return abort_record(prepared, "required D-Flow FM checkpoint missing");
    }

    return CheckpointCommitRecord{
        .epoch = epoch,
        .logical_time = logical_time,
        .status = CheckpointCommitStatus::committed,
        .modules = prepared,
        .reason = {},
    };
}

}  // namespace scau::coupling::driver

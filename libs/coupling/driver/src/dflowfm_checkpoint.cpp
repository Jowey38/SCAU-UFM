#include "coupling/driver/dflowfm_checkpoint.hpp"

#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace scau::coupling::driver {
namespace {

std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1U);
}

std::string key_before_equals(const std::string& line) {
    const auto equals = line.find('=');
    if (equals == std::string::npos) {
        return {};
    }
    return trim(line.substr(0U, equals));
}

bool is_section(const std::string& line, const std::string& section) {
    return trim(line) == "[" + section + "]";
}

}  // namespace

void validate_dflowfm_checkpoint(const DFlowFMCheckpoint& checkpoint) {
    if (!std::isfinite(checkpoint.logical_time) || checkpoint.logical_time < 0.0) {
        throw std::invalid_argument("D-Flow FM checkpoint logical_time must be finite and non-negative");
    }
    if (checkpoint.state_file.empty() || !std::filesystem::is_regular_file(checkpoint.state_file)) {
        throw std::invalid_argument("D-Flow FM checkpoint state_file must exist");
    }
    if (checkpoint.base_mdu.empty() || !std::filesystem::is_regular_file(checkpoint.base_mdu)) {
        throw std::invalid_argument("D-Flow FM checkpoint base_mdu must exist");
    }
    if (checkpoint.model_fingerprint.empty()) {
        throw std::invalid_argument("D-Flow FM checkpoint model_fingerprint must not be empty");
    }
    if (checkpoint.source == DFlowFMCheckpointSource::map_nc && checkpoint.restart_datetime.empty()) {
        throw std::invalid_argument("D-Flow FM map checkpoint requires restart_datetime");
    }
}

DFlowFMRollbackAction decide_dflowfm_rollback_action(const DFlowFMRollbackRequest& request) {
    if (!request.engine_advanced) {
        return DFlowFMRollbackAction::memory_only;
    }
    if (request.checkpoint == nullptr) {
        return DFlowFMRollbackAction::abort_no_checkpoint;
    }
    validate_dflowfm_checkpoint(*request.checkpoint);
    if (!request.deterministic_replay_available) {
        return DFlowFMRollbackAction::abort_replay_unavailable;
    }
    return DFlowFMRollbackAction::checkpoint_reload;
}

void write_dflowfm_restart_mdu(
    const DFlowFMCheckpoint& checkpoint,
    const std::filesystem::path& output_mdu) {
    validate_dflowfm_checkpoint(checkpoint);
    if (output_mdu.empty()) {
        throw std::invalid_argument("D-Flow FM restart output MDU path must not be empty");
    }

    std::ifstream input(checkpoint.base_mdu);
    if (!input) {
        throw std::runtime_error("failed to open D-Flow FM base MDU");
    }
    std::ostringstream source;
    source << input.rdbuf();
    std::istringstream lines(source.str());
    std::ostringstream result;

    bool in_restart = false;
    bool saw_restart = false;
    bool wrote_restart_file = false;
    bool wrote_restart_datetime = false;
    bool wrote_tstart = false;
    std::string line;
    while (std::getline(lines, line)) {
        if (!trim(line).empty() && trim(line).front() == '[') {
            if (in_restart) {
                if (!wrote_restart_file) {
                    result << "RestartFile = " << checkpoint.state_file.generic_string() << '\n';
                }
                if (checkpoint.source == DFlowFMCheckpointSource::map_nc && !wrote_restart_datetime) {
                    result << "RestartDateTime = " << checkpoint.restart_datetime << '\n';
                }
            }
            in_restart = is_section(line, "restart");
            saw_restart = saw_restart || in_restart;
        }

        const std::string key = key_before_equals(line);
        if (in_restart && key == "RestartFile") {
            result << "RestartFile = " << checkpoint.state_file.generic_string() << '\n';
            wrote_restart_file = true;
            continue;
        }
        if (in_restart && key == "RestartDateTime") {
            if (checkpoint.source == DFlowFMCheckpointSource::map_nc) {
                result << "RestartDateTime = " << checkpoint.restart_datetime << '\n';
            } else {
                result << "RestartDateTime =\n";
            }
            wrote_restart_datetime = true;
            continue;
        }
        if (key == "TStart") {
            result << "TStart = " << checkpoint.logical_time << '\n';
            wrote_tstart = true;
            continue;
        }
        result << line << '\n';
    }

    if (in_restart) {
        if (!wrote_restart_file) {
            result << "RestartFile = " << checkpoint.state_file.generic_string() << '\n';
        }
        if (checkpoint.source == DFlowFMCheckpointSource::map_nc && !wrote_restart_datetime) {
            result << "RestartDateTime = " << checkpoint.restart_datetime << '\n';
        }
    }
    if (!saw_restart) {
        result << "\n[restart]\nRestartFile = " << checkpoint.state_file.generic_string() << '\n';
        if (checkpoint.source == DFlowFMCheckpointSource::map_nc) {
            result << "RestartDateTime = " << checkpoint.restart_datetime << '\n';
        }
    }
    if (!wrote_tstart) {
        result << "\n[time]\nTStart = " << checkpoint.logical_time << '\n';
    }

    std::ofstream output(output_mdu, std::ios::trunc);
    if (!output) {
        throw std::runtime_error("failed to create D-Flow FM restart MDU");
    }
    output << result.str();
    if (!output) {
        throw std::runtime_error("failed to write D-Flow FM restart MDU");
    }
}

void reload_dflowfm_from_checkpoint(
    river::DFlowFMEngine& engine,
    const DFlowFMCheckpoint& checkpoint,
    const std::filesystem::path& generated_mdu) {
    validate_dflowfm_checkpoint(checkpoint);
    engine.finalize();
    write_dflowfm_restart_mdu(checkpoint, generated_mdu);
    engine.initialize(generated_mdu.string());
    constexpr double kTimeTolerance = 1.0e-9;
    if (std::abs(engine.current_time() - checkpoint.logical_time) > kTimeTolerance) {
        engine.finalize();
        throw std::runtime_error("D-Flow FM checkpoint reload time does not match logical checkpoint time");
    }
}

}  // namespace scau::coupling::driver

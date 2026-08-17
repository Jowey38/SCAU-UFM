#include "runtime_config_io.hpp"

#include <cstddef>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace scau::apps::sim_driver {

namespace {

[[noreturn]] void fail(const std::string& message) {
    throw std::invalid_argument("runtime config: " + message);
}

std::string trim(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = text.find_last_not_of(" \t\r");
    return text.substr(first, last - first + 1U);
}

double parse_double(const std::string& value, const std::string& key) {
    std::size_t consumed = 0U;
    double parsed = 0.0;
    try {
        parsed = std::stod(value, &consumed);
    } catch (const std::exception&) {
        fail("key '" + key + "' has a malformed numeric value '" + value + "'");
    }
    if (consumed != value.size()) {
        fail("key '" + key + "' has trailing garbage in value '" + value + "'");
    }
    return parsed;
}

std::size_t parse_index(const std::string& value, const std::string& key) {
    if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos) {
        fail("key '" + key + "' requires a non-negative integer, got '" + value + "'");
    }
    return static_cast<std::size_t>(std::stoull(value));
}

int parse_int(const std::string& value, const std::string& key) {
    std::string digits = value;
    if (!digits.empty() && digits.front() == '-') {
        digits.erase(0, 1U);
    }
    if (digits.empty() || digits.find_first_not_of("0123456789") != std::string::npos) {
        fail("key '" + key + "' requires an integer, got '" + value + "'");
    }
    return std::stoi(value);
}

bool parse_bool(const std::string& value, const std::string& key) {
    if (value == "true") {
        return true;
    }
    if (value == "false") {
        return false;
    }
    fail("key '" + key + "' requires 'true' or 'false', got '" + value + "'");
}

// Splits "a=1,b=2" into ordered subkey/value pairs; duplicate subkeys throw.
std::unordered_map<std::string, std::string> parse_structured_value(
    const std::string& value,
    const std::string& key,
    const std::unordered_set<std::string>& allowed) {
    std::unordered_map<std::string, std::string> fields;
    std::stringstream stream(value);
    std::string item;
    while (std::getline(stream, item, ',')) {
        const std::string entry = trim(item);
        if (entry.empty()) {
            fail("key '" + key + "' has an empty item");
        }
        const auto equals = entry.find('=');
        if (equals == std::string::npos) {
            fail("key '" + key + "' item '" + entry + "' is not subkey=value");
        }
        const std::string subkey = trim(entry.substr(0, equals));
        const std::string subvalue = trim(entry.substr(equals + 1U));
        if (!allowed.contains(subkey)) {
            fail("key '" + key + "' has unknown subkey '" + subkey + "'");
        }
        if (subvalue.empty()) {
            fail("key '" + key + "' subkey '" + subkey + "' has an empty value");
        }
        if (!fields.emplace(subkey, subvalue).second) {
            fail("key '" + key + "' repeats subkey '" + subkey + "'");
        }
    }
    return fields;
}

const std::string& require_subkey(
    const std::unordered_map<std::string, std::string>& fields,
    const std::string& subkey,
    const std::string& key) {
    const auto found = fields.find(subkey);
    if (found == fields.end()) {
        fail("key '" + key + "' is missing required subkey '" + subkey + "'");
    }
    return found->second;
}

}  // namespace

RuntimeConfig parse_runtime_config_text(const std::string& text) {
    RuntimeConfig config{};
    std::unordered_set<std::string> seen_scalar_keys;
    bool version_seen = false;

    std::stringstream stream(text);
    std::string raw_line;
    while (std::getline(stream, raw_line)) {
        std::string line = raw_line;
        const auto comment = line.find('#');
        if (comment != std::string::npos) {
            line.erase(comment);
        }
        line = trim(line);
        if (line.empty()) {
            continue;
        }

        const auto equals = line.find('=');
        if (equals == std::string::npos) {
            fail("line '" + line + "' is not key = value");
        }
        const std::string key = trim(line.substr(0, equals));
        const std::string value = trim(line.substr(equals + 1U));
        if (key.empty()) {
            fail("line '" + line + "' has an empty key");
        }
        if (value.empty()) {
            fail("key '" + key + "' has an empty value");
        }

        if (!version_seen) {
            if (key != "version") {
                fail("the first logical line must be 'version = 2'");
            }
            config.version = parse_int(value, key);
            version_seen = true;
            continue;
        }

        const bool is_link_key = key == "surface_drainage_link" ||
                                 key == "surface_river_link" ||
                                 key == "drainage_river_link";
        if (!is_link_key && !seen_scalar_keys.insert(key).second) {
            fail("duplicate key '" + key + "'");
        }

        if (key == "version") {
            fail("duplicate key 'version'");
        } else if (key == "start_time") {
            config.start_time = parse_double(value, key);
        } else if (key == "end_time") {
            config.end_time = parse_double(value, key);
        } else if (key == "dt_couple") {
            config.dt_couple = parse_double(value, key);
        } else if (key == "dt_surface") {
            config.dt_surface = parse_double(value, key);
        } else if (key == "dt_swmm") {
            config.dt_swmm = parse_double(value, key);
        } else if (key == "dt_dflowfm") {
            config.dt_dflowfm = parse_double(value, key);
        } else if (key == "enable_swmm") {
            config.enable_swmm = parse_bool(value, key);
        } else if (key == "enable_dflowfm") {
            config.enable_dflowfm = parse_bool(value, key);
        } else if (key == "stcf_case_path") {
            config.stcf_case_path = value;
        } else if (key == "swmm_inp_path") {
            config.swmm_inp_path = value;
        } else if (key == "dflowfm_mdu_path") {
            config.dflowfm_mdu_path = value;
        } else if (key == "initial_eta") {
            config.initial_eta = parse_double(value, key);
        } else if (key == "h_wet") {
            config.h_wet = parse_double(value, key);
        } else if (key == "cfl_safety") {
            config.cfl_safety = parse_double(value, key);
        } else if (key == "c_rollback") {
            config.c_rollback = parse_double(value, key);
        } else if (key == "enable_cvc_spatial_phi_t_correction") {
            config.enable_cvc_spatial_phi_t_correction = parse_bool(value, key);
        } else if (key == "enable_whole_system_mass_audit") {
            config.enable_whole_system_mass_audit = parse_bool(value, key);
        } else if (key == "n_writeoff_steps") {
            config.n_writeoff_steps = parse_index(value, key);
        } else if (key == "mass_audit_engine_residual_absolute") {
            config.mass_audit_engine_residual_absolute = parse_double(value, key);
        } else if (key == "mass_audit_engine_residual_relative") {
            config.mass_audit_engine_residual_relative = parse_double(value, key);
        } else if (key == "mass_audit_engine_internal_gap_absolute") {
            config.mass_audit_engine_internal_gap_absolute = parse_double(value, key);
        } else if (key == "engine_mode") {
            if (value == "mock") {
                config.engine_mode = EngineMode::mock;
            } else if (value == "real") {
                config.engine_mode = EngineMode::real;
            } else {
                fail("key 'engine_mode' requires 'mock' or 'real', got '" + value + "'");
            }
        } else if (key == "river_water_level_variable") {
            config.river_water_level_variable = value;
        } else if (key == "output_summary_path") {
            config.output_summary_path = value;
        } else if (key == "surface_drainage_link") {
            const auto fields = parse_structured_value(
                value, key, {"cell", "node", "crest", "width", "weight"});
            SurfaceDrainageLinkConfig link{};
            link.cell = parse_index(require_subkey(fields, "cell", key), key);
            link.node_name = require_subkey(fields, "node", key);
            link.crest_level = parse_double(require_subkey(fields, "crest", key), key);
            link.exchange_width = parse_double(require_subkey(fields, "width", key), key);
            if (const auto weight = fields.find("weight"); weight != fields.end()) {
                link.priority_weight = parse_double(weight->second, key);
            }
            config.surface_drainage.push_back(std::move(link));
        } else if (key == "surface_river_link") {
            const auto fields = parse_structured_value(
                value, key, {"cell", "location", "lateral_id", "crest", "width", "weight"});
            SurfaceRiverLinkConfig link{};
            link.cell = parse_index(require_subkey(fields, "cell", key), key);
            link.location_id = parse_int(require_subkey(fields, "location", key), key);
            link.native_lateral_id = require_subkey(fields, "lateral_id", key);
            link.crest_level = parse_double(require_subkey(fields, "crest", key), key);
            link.exchange_width = parse_double(require_subkey(fields, "width", key), key);
            if (const auto weight = fields.find("weight"); weight != fields.end()) {
                link.priority_weight = parse_double(weight->second, key);
            }
            config.surface_river.push_back(std::move(link));
        } else if (key == "drainage_river_link") {
            const auto fields = parse_structured_value(
                value, key, {"outfall", "location", "q_capacity", "drive_outfall_stage"});
            DrainageRiverLinkConfig link{};
            link.outfall_name = require_subkey(fields, "outfall", key);
            link.river_location_id = parse_int(require_subkey(fields, "location", key), key);
            link.q_capacity = parse_double(require_subkey(fields, "q_capacity", key), key);
            if (const auto drive = fields.find("drive_outfall_stage"); drive != fields.end()) {
                link.drive_outfall_stage = parse_bool(drive->second, key);
            }
            config.drainage_river.push_back(std::move(link));
        } else {
            fail("unknown key '" + key + "'");
        }
    }

    if (!version_seen) {
        fail("empty config; the first logical line must be 'version = 2'");
    }
    return config;
}

RuntimeConfig read_runtime_config_file(const std::filesystem::path& path) {
    std::ifstream stream(path);
    if (!stream) {
        fail("cannot open config file '" + path.string() + "'");
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return parse_runtime_config_text(buffer.str());
}

}  // namespace scau::apps::sim_driver

#pragma once

#include <filesystem>
#include <string>

#include "sim_driver.hpp"

namespace scau::apps::sim_driver {

// Strict fail-closed key=value runtime config format:
//   - '#' starts a comment; blank lines are ignored;
//   - the first logical line must be `version = 2`;
//   - unknown keys, duplicate scalar keys, malformed values, and trailing
//     garbage throw std::invalid_argument with the offending key;
//   - coupling links use repeated structured keys, e.g.
//       surface_drainage_link = cell=12,node=J1,crest=2.05,width=1.5,weight=1.0
//       surface_river_link    = cell=3,location=5,lateral_id=lat1,crest=2.0,width=2.0
//       drainage_river_link   = outfall=OUT1,location=5,q_capacity=0.5
// The parsed config is returned as-is; callers run validate_runtime_config
// (SimDriver::configure does) for cross-field rules.
[[nodiscard]] RuntimeConfig parse_runtime_config_text(const std::string& text);

[[nodiscard]] RuntimeConfig read_runtime_config_file(const std::filesystem::path& path);

}  // namespace scau::apps::sim_driver

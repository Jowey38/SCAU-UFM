#pragma once

#include <string>
#include <vector>

#include "stcf/topology.hpp"

namespace scau::stcf {

inline constexpr const char* kMixedMinimalProfile = "mixed-minimal";

[[nodiscard]] StcfCase make_mixed_minimal_case();

[[nodiscard]] std::vector<std::string> case_profile_names();

[[nodiscard]] StcfCase make_case_profile(const std::string& profile_name);

}  // namespace scau::stcf

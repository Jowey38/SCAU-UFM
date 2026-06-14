#pragma once

#include <cstdint>
#include <vector>

#include "core/types.hpp"

namespace scau::surface2d {

// Green-Ampt soil parameters (main Spec SoilParamsLUT::Entry).
// k_s: saturated hydraulic conductivity (m/s); psi_f: wetting front suction
// head (m); theta_s: saturated moisture content; theta_i: initial moisture
// content. Machine-facing names K_s/psi_f/theta_s/theta_i.
struct SoilParams {
    core::Real k_s{0.0};
    core::Real psi_f{0.0};
    core::Real theta_s{0.0};
    core::Real theta_i{0.0};
};

// Soil parameter lookup table indexed by soil_type.
struct SoilParamsLUT {
    std::vector<SoilParams> entries;

    // Fail-closed lookup: throws std::out_of_range if soil_type is not present.
    [[nodiscard]] const SoilParams& at(std::uint8_t soil_type) const;
};

// Fail-closed domain validation. psi_f must be strictly positive (psi_f == 0
// degenerates Green-Ampt). Requires theta_s > theta_i, theta_s <= 1,
// theta_i >= 0, k_s >= 0, all finite.
void validate_soil_params(const SoilParams& params);

// Validates every entry; throws if the table is empty.
void validate_soil_params_lut(const SoilParamsLUT& lut);

}  // namespace scau::surface2d

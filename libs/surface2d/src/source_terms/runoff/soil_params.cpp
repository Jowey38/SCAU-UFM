#include "surface2d/source_terms/runoff/soil_params.hpp"

#include <cmath>
#include <stdexcept>

namespace scau::surface2d {

const SoilParams& SoilParamsLUT::at(std::uint8_t soil_type) const {
    const std::size_t index = static_cast<std::size_t>(soil_type);
    if (index >= entries.size()) {
        throw std::out_of_range("soil_type index is out of range of the soil parameter table");
    }
    return entries[index];
}

void validate_soil_params(const SoilParams& params) {
    if (!std::isfinite(params.k_s) || params.k_s < 0.0) {
        throw std::invalid_argument("soil k_s must be finite and non-negative");
    }
    if (!std::isfinite(params.psi_f) || params.psi_f <= 0.0) {
        throw std::invalid_argument("soil psi_f must be finite and strictly positive");
    }
    if (!std::isfinite(params.theta_s) || !std::isfinite(params.theta_i)) {
        throw std::invalid_argument("soil moisture contents must be finite");
    }
    if (params.theta_i < 0.0) {
        throw std::invalid_argument("soil theta_i must be non-negative");
    }
    if (params.theta_s > 1.0) {
        throw std::invalid_argument("soil theta_s must be <= 1");
    }
    if (params.theta_s <= params.theta_i) {
        throw std::invalid_argument("soil theta_s must be greater than theta_i");
    }
}

void validate_soil_params_lut(const SoilParamsLUT& lut) {
    if (lut.entries.empty()) {
        throw std::invalid_argument("soil parameter table must not be empty");
    }
    for (const auto& entry : lut.entries) {
        validate_soil_params(entry);
    }
}

}  // namespace scau::surface2d

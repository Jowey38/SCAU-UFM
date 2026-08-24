#include "surface2d/dpm/cvc_augmented_flux.hpp"

#include <cmath>
#include <stdexcept>
#include <string>

namespace scau::surface2d {
namespace {

void validate_phi_t(core::Real value, const char* side) {
    if (!std::isfinite(value) || value <= 0.0) {
        throw std::invalid_argument(std::string("CVC ") + side + " phi_t must be finite and positive");
    }
}

}  // namespace

CvcSideFluxes cvc_side_fluxes(
    const EdgeFlux& baseline,
    core::Real phi_t_left,
    core::Real phi_t_right) {
    validate_phi_t(phi_t_left, "left");
    validate_phi_t(phi_t_right, "right");
    return cvc_side_fluxes_unchecked(baseline, phi_t_left, phi_t_right);
}

}  // namespace scau::surface2d

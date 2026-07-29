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

core::Real upwind_phi(core::Real flux, core::Real left, core::Real right) {
    if (flux > 0.0) {
        return left;
    }
    if (flux < 0.0) {
        return right;
    }
    return 0.5 * (left + right);
}

}  // namespace

CvcSideFluxes cvc_side_fluxes(
    const EdgeFlux& baseline,
    core::Real phi_t_left,
    core::Real phi_t_right) {
    validate_phi_t(phi_t_left, "left");
    validate_phi_t(phi_t_right, "right");
    if (phi_t_left == phi_t_right) {
        return CvcSideFluxes{
            .left_mass = baseline.mass,
            .right_mass = baseline.mass,
            .left_momentum_x = baseline.momentum_x,
            .right_momentum_x = baseline.momentum_x,
            .left_momentum_y = baseline.momentum_y,
            .right_momentum_y = baseline.momentum_y,
        };
    }

    const core::Real transport_phi = upwind_phi(
        baseline.mass, phi_t_left, phi_t_right);
    const core::Real mass_storage_flux = transport_phi * baseline.mass;
    const core::Real momentum_x_storage_flux = transport_phi * baseline.momentum_x;
    const core::Real momentum_y_storage_flux = transport_phi * baseline.momentum_y;

    const core::Real left_mass = mass_storage_flux / phi_t_left;
    const core::Real right_mass = mass_storage_flux / phi_t_right;
    const core::Real left_momentum_x = momentum_x_storage_flux / phi_t_left;
    const core::Real right_momentum_x = momentum_x_storage_flux / phi_t_right;
    const core::Real left_momentum_y = momentum_y_storage_flux / phi_t_left;
    const core::Real right_momentum_y = momentum_y_storage_flux / phi_t_right;

    return CvcSideFluxes{
        .left_mass = left_mass,
        .right_mass = right_mass,
        .left_momentum_x = left_momentum_x,
        .right_momentum_x = right_momentum_x,
        .left_momentum_y = left_momentum_y,
        .right_momentum_y = right_momentum_y,
        .storage_residual_before = baseline.mass * (phi_t_right - phi_t_left),
        .storage_residual_after =
            -phi_t_left * left_mass + phi_t_right * right_mass,
        .applied = baseline.mass != 0.0
            || baseline.momentum_x != 0.0
            || baseline.momentum_y != 0.0,
    };
}

}  // namespace scau::surface2d

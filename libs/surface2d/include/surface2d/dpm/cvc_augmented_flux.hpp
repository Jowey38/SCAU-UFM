#pragma once

#include "core/types.hpp"
#include "surface2d/portability.hpp"
#include "surface2d/riemann/hllc.hpp"

namespace scau::surface2d {

struct CvcSideFluxes {
    core::Real left_mass{0.0};
    core::Real right_mass{0.0};
    core::Real left_momentum_x{0.0};
    core::Real right_momentum_x{0.0};
    core::Real left_momentum_y{0.0};
    core::Real right_momentum_y{0.0};
    core::Real storage_residual_before{0.0};
    core::Real storage_residual_after{0.0};
    bool applied{false};
};

// Converts one baseline h/hu/hv HLLC flux into side-specific fluctuations
// whose phi_t-weighted mass and Cartesian momentum are conservative across a
// spatial phi_t discontinuity. The returned values replace the baseline flux
// on each side; they are not additive corrections.
// Checked host entry point (throws on non-finite/non-positive phi_t).
[[nodiscard]] CvcSideFluxes cvc_side_fluxes(
    const EdgeFlux& baseline,
    core::Real phi_t_left,
    core::Real phi_t_right);

// Unchecked SCAU_HD core shared with the deterministic CUDA backend
// (M284/G9). Identical arithmetic to the checked wrapper; the CUDA path
// pre-validates phi_t on the host for exactly the edges the CPU path would
// have validated (wb-paired internal edges with a phi_t jump).
[[nodiscard]] SCAU_HD inline CvcSideFluxes cvc_side_fluxes_unchecked(
    const EdgeFlux& baseline,
    core::Real phi_t_left,
    core::Real phi_t_right) {
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

    const core::Real transport_phi = baseline.mass > 0.0
        ? phi_t_left
        : (baseline.mass < 0.0 ? phi_t_right : 0.5 * (phi_t_left + phi_t_right));
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

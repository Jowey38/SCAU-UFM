#pragma once

#include "core/types.hpp"
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
[[nodiscard]] CvcSideFluxes cvc_side_fluxes(
    const EdgeFlux& baseline,
    core::Real phi_t_left,
    core::Real phi_t_right);

}  // namespace scau::surface2d

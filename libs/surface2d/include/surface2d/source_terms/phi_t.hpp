#pragma once

#include "core/types.hpp"
#include "surface2d/portability.hpp"

namespace scau::surface2d {

// Header-inline SCAU_HD so the deterministic CUDA backend (M284/G9) compiles
// the exact same pairing diagnostics for the device.
[[nodiscard]] SCAU_HD inline core::Real pressure_pairing_phi_t(
    core::Real phi_left,
    core::Real phi_right,
    core::Real h,
    core::Real gravity = 9.81) {
    return 0.5 * gravity * h * h * (phi_right - phi_left);
}

[[nodiscard]] SCAU_HD inline core::Real s_phi_t_centered(
    core::Real phi_left,
    core::Real phi_right,
    core::Real h,
    core::Real gravity = 9.81) {
    return -pressure_pairing_phi_t(phi_left, phi_right, h, gravity);
}

}  // namespace scau::surface2d

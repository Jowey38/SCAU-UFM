#pragma once

#include "core/types.hpp"

namespace scau::surface2d {

[[nodiscard]] core::Real pressure_pairing_phi_t(
    core::Real phi_left,
    core::Real phi_right,
    core::Real h,
    core::Real gravity = 9.81);

[[nodiscard]] core::Real s_phi_t_centered(
    core::Real phi_left,
    core::Real phi_right,
    core::Real h,
    core::Real gravity = 9.81);

}  // namespace scau::surface2d

#include "surface2d/source_terms/phi_t.hpp"

namespace scau::surface2d {

core::Real pressure_pairing_phi_t(core::Real phi_left, core::Real phi_right, core::Real h, core::Real gravity) {
    return 0.5 * gravity * h * h * (phi_right - phi_left);
}

core::Real s_phi_t_centered(core::Real phi_left, core::Real phi_right, core::Real h, core::Real gravity) {
    return -pressure_pairing_phi_t(phi_left, phi_right, h, gravity);
}

}  // namespace scau::surface2d

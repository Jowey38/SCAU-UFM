#pragma once

#include "core/types.hpp"

namespace scau::surface2d {

// Per-edge well-balanced pressure-source pairing for one internal edge
// (Main Spec 5.6, Audusse single-sided split). All quantities are scalar
// magnitudes along the edge normal n (which points from the left cell into
// the right cell). The caller applies:
//   dM_left  += left_normal  * n * len
//   dM_right += right_normal * (-n) * len
// so the pressure flux F_p (shared) is conservative (opposite signs) while
// each side's S_phi_t/S_topo source uses that cell's own phi_t and depth.
struct WellBalancedEdgePairing {
    core::Real left_normal{0.0};   // -F_p + S_phi_t^(L) + S_topo^(L)
    core::Real right_normal{0.0};  // -F_p + S_phi_t^(R) + S_topo^(R)
    core::Real pressure_flux{0.0}; // F_p,e (diagnostic)
    core::Real s_phi_t_left{0.0};  // diagnostic
    core::Real s_topo_left{0.0};   // diagnostic
};

// Inputs:
//   phi_t_left/right : cell-centre storage porosity of each side (phi_t,i)
//   h_left/right     : cell-centre water depth of each side (h_i)
//   h_left_star/right_star : hydrostatic-reconstructed depth at the edge (h_i*)
//   gravity          : g
// Edge templates (Main Spec 5.5.2 arithmetic mean):
//   h_bar = 0.5*(h_left_star + h_right_star);  phi_t_e = 0.5*(phi_t_left+phi_t_right)
//   F_p           = 0.5*g*phi_t_e*h_bar^2
//   S_phi_t^(i)   = 0.5*g*h_bar^2*(phi_t_e - phi_t_i)
//   S_topo^(i)    = 0.5*g*phi_t_i*(h_bar^2 - h_i^2)   (Audusse square-difference)
[[nodiscard]] WellBalancedEdgePairing well_balanced_edge_pairing(
    core::Real phi_t_left,
    core::Real phi_t_right,
    core::Real h_left,
    core::Real h_right,
    core::Real h_left_star,
    core::Real h_right_star,
    core::Real gravity = 9.81);

// Reflective-wall / open-boundary single-cell hydrostatic pressure magnitude:
// F_p = 0.5*g*phi_t_inside*h_inside^2 (phi_t-scaled; topo source is zero at a
// reflective boundary because the mirrored neighbour has the same bed).
[[nodiscard]] core::Real well_balanced_boundary_pressure(
    core::Real phi_t_inside,
    core::Real h_inside,
    core::Real gravity = 9.81);

}  // namespace scau::surface2d

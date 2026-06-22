#include "surface2d/source_terms/well_balanced.hpp"

namespace scau::surface2d {

WellBalancedEdgePairing well_balanced_edge_pairing(
    core::Real phi_t_left,
    core::Real phi_t_right,
    core::Real h_left,
    core::Real h_right,
    core::Real h_left_star,
    core::Real h_right_star,
    core::Real gravity) {
    const core::Real h_bar = 0.5 * (h_left_star + h_right_star);
    const core::Real h_bar_sq = h_bar * h_bar;
    const core::Real phi_t_e = 0.5 * (phi_t_left + phi_t_right);

    const core::Real pressure_flux = 0.5 * gravity * phi_t_e * h_bar_sq;

    const core::Real s_phi_t_left = 0.5 * gravity * h_bar_sq * (phi_t_e - phi_t_left);
    const core::Real s_phi_t_right = 0.5 * gravity * h_bar_sq * (phi_t_e - phi_t_right);

    const core::Real s_topo_left = 0.5 * gravity * phi_t_left * (h_bar_sq - h_left * h_left);
    const core::Real s_topo_right = 0.5 * gravity * phi_t_right * (h_bar_sq - h_right * h_right);

    return WellBalancedEdgePairing{
        .left_normal = -pressure_flux + s_phi_t_left + s_topo_left,
        .right_normal = -pressure_flux + s_phi_t_right + s_topo_right,
        .pressure_flux = pressure_flux,
        .s_phi_t_left = s_phi_t_left,
        .s_topo_left = s_topo_left,
    };
}

core::Real well_balanced_boundary_pressure(
    core::Real phi_t_inside,
    core::Real h_inside,
    core::Real gravity) {
    return 0.5 * gravity * phi_t_inside * h_inside * h_inside;
}

}  // namespace scau::surface2d

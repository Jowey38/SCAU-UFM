#include "surface2d/riemann/hllc.hpp"

#include "surface2d/reconstruction/hydrostatic.hpp"

namespace scau::surface2d {

core::Real normal_velocity(const CellState& state, Normal2 normal) {
    return state.u() * normal.x + state.v() * normal.y;
}

EdgeFlux hllc_normal_flux(
    const CellState& left,
    const CellState& right,
    const EdgeDpmFields& edge_fields,
    Normal2 normal,
    core::Real h_min) {
    if (edge_fields.omega_edge == 0.0 || edge_fields.phi_e_n < PhiEdgeMin) {
        return EdgeFlux{};
    }

    const auto pair = reconstruct_hydrostatic_pair(left, right);
    if (pair.left.conserved.h <= h_min || pair.right.conserved.h <= h_min) {
        return EdgeFlux{};
    }

    const core::Real left_un = normal_velocity(pair.left, normal);
    const core::Real right_un = normal_velocity(pair.right, normal);
    if (left_un == 0.0 && right_un == 0.0 && pair.left.eta == pair.right.eta) {
        return EdgeFlux{};
    }

    return EdgeFlux{
        .mass = 0.5 * edge_fields.phi_e_n * (pair.left.conserved.h * left_un + pair.right.conserved.h * right_un),
        .momentum_n = 0.0,
    };
}

}  // namespace scau::surface2d

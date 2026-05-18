#include "surface2d/riemann/hllc.hpp"

#include <algorithm>
#include <cmath>

#include "surface2d/reconstruction/hydrostatic.hpp"

namespace scau::surface2d {
namespace {

core::Real wave_celerity(const CellState& state, core::Real gravity) {
    return std::sqrt(gravity * state.conserved.h);
}

core::Real physical_normal_mass_flux(const CellState& state, Normal2 normal) {
    return state.conserved.h * normal_velocity(state, normal);
}

core::Real physical_normal_momentum_flux(const CellState& state, Normal2 normal, core::Real gravity) {
    const core::Real un = normal_velocity(state, normal);
    return state.conserved.h * un * un + 0.5 * gravity * state.conserved.h * state.conserved.h;
}

core::Real hllc_star_normal_mass_flux(
    const CellState& state,
    Normal2 normal,
    core::Real s_k,
    core::Real s_star) {
    const core::Real un = normal_velocity(state, normal);
    const core::Real denominator = s_k - s_star;
    if (denominator == 0.0) {
        return physical_normal_mass_flux(state, normal);
    }
    const core::Real h_star = state.conserved.h * (s_k - un) / denominator;
    return physical_normal_mass_flux(state, normal) + s_k * (h_star - state.conserved.h);
}

core::Real hllc_star_normal_momentum_flux(
    const CellState& state,
    Normal2 normal,
    core::Real s_k,
    core::Real s_star,
    core::Real gravity) {
    const core::Real un = normal_velocity(state, normal);
    const core::Real denominator = s_k - s_star;
    if (denominator == 0.0) {
        return physical_normal_momentum_flux(state, normal, gravity);
    }
    const core::Real h_star = state.conserved.h * (s_k - un) / denominator;
    const core::Real momentum_star = h_star * s_star;
    return physical_normal_momentum_flux(state, normal, gravity) + s_k * (momentum_star - state.conserved.h * un);
}

}  // namespace

core::Real normal_velocity(const CellState& state, Normal2 normal) {
    return state.u() * normal.x + state.v() * normal.y;
}

WaveSpeeds estimate_hllc_wave_speeds(
    const CellState& left,
    const CellState& right,
    Normal2 normal,
    core::Real gravity) {
    const core::Real left_un = normal_velocity(left, normal);
    const core::Real right_un = normal_velocity(right, normal);
    const core::Real left_c = wave_celerity(left, gravity);
    const core::Real right_c = wave_celerity(right, gravity);
    const core::Real s_l = std::min(left_un - left_c, right_un - right_c);
    const core::Real s_r = std::max(left_un + left_c, right_un + right_c);

    const core::Real numerator = right.conserved.h * right_un * (s_r - right_un) -
                                 left.conserved.h * left_un * (s_l - left_un) +
                                 physical_normal_momentum_flux(left, normal, gravity) -
                                 physical_normal_momentum_flux(right, normal, gravity);
    const core::Real denominator = right.conserved.h * (s_r - right_un) - left.conserved.h * (s_l - left_un);
    const core::Real s_star = denominator != 0.0 ? numerator / denominator : 0.0;

    return WaveSpeeds{.s_l = s_l, .s_star = s_star, .s_r = s_r};
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

    const auto speeds = estimate_hllc_wave_speeds(pair.left, pair.right, normal);

    core::Real mass = 0.0;
    core::Real momentum_n = 0.0;
    if (0.0 <= speeds.s_l) {
        mass = physical_normal_mass_flux(pair.left, normal);
        momentum_n = physical_normal_momentum_flux(pair.left, normal, 9.81);
    } else if (speeds.s_l <= 0.0 && 0.0 <= speeds.s_star) {
        mass = hllc_star_normal_mass_flux(pair.left, normal, speeds.s_l, speeds.s_star);
        momentum_n = hllc_star_normal_momentum_flux(pair.left, normal, speeds.s_l, speeds.s_star, 9.81);
    } else if (speeds.s_star <= 0.0 && 0.0 <= speeds.s_r) {
        mass = hllc_star_normal_mass_flux(pair.right, normal, speeds.s_r, speeds.s_star);
        momentum_n = hllc_star_normal_momentum_flux(pair.right, normal, speeds.s_r, speeds.s_star, 9.81);
    } else {
        mass = physical_normal_mass_flux(pair.right, normal);
        momentum_n = physical_normal_momentum_flux(pair.right, normal, 9.81);
    }

    return EdgeFlux{
        .mass = edge_fields.phi_e_n * mass,
        .momentum_n = edge_fields.phi_e_n * momentum_n,
    };
}

}  // namespace scau::surface2d

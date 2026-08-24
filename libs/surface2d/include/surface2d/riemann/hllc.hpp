#pragma once

#include "core/types.hpp"
#include "surface2d/dpm/edge_classification.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/portability.hpp"
#include "surface2d/reconstruction/hydrostatic.hpp"
#include "surface2d/state/state.hpp"

namespace scau::surface2d {

struct Normal2 {
    core::Real x{0.0};
    core::Real y{0.0};
};

struct EdgeFlux {
    core::Real mass{0.0};
    core::Real momentum_n{0.0};
    core::Real momentum_x{0.0};
    core::Real momentum_y{0.0};
};

struct WaveSpeeds {
    core::Real s_l{0.0};
    core::Real s_star{0.0};
    core::Real s_r{0.0};
};

// The whole HLLC kernel is header-inline SCAU_HD so the deterministic CUDA
// backend (M284/G9) compiles the exact same Riemann solver for the device.

[[nodiscard]] SCAU_HD inline core::Real normal_velocity(const CellState& state, Normal2 normal) {
    return state.u() * normal.x + state.v() * normal.y;
}

namespace detail {

[[nodiscard]] SCAU_HD inline core::Real wave_celerity(const CellState& state, core::Real gravity) {
    return hd::sqrt_(gravity * state.conserved.h);
}

[[nodiscard]] SCAU_HD inline core::Real physical_normal_mass_flux(const CellState& state, Normal2 normal) {
    return state.conserved.h * normal_velocity(state, normal);
}

[[nodiscard]] SCAU_HD inline core::Real physical_normal_momentum_flux(
    const CellState& state,
    Normal2 normal,
    core::Real gravity) {
    const core::Real un = normal_velocity(state, normal);
    return state.conserved.h * un * un + 0.5 * gravity * state.conserved.h * state.conserved.h;
}

[[nodiscard]] SCAU_HD inline core::Real advective_normal_momentum_flux(const CellState& state, Normal2 normal) {
    const core::Real un = normal_velocity(state, normal);
    return state.conserved.h * un * un;
}

[[nodiscard]] SCAU_HD inline Normal2 tangent_from_normal(Normal2 normal) {
    return Normal2{.x = -normal.y, .y = normal.x};
}

[[nodiscard]] SCAU_HD inline core::Real tangential_velocity(const CellState& state, Normal2 normal) {
    const auto tangent = tangent_from_normal(normal);
    return state.u() * tangent.x + state.v() * tangent.y;
}

[[nodiscard]] SCAU_HD inline core::Real hllc_star_normal_mass_flux(
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

[[nodiscard]] SCAU_HD inline core::Real hllc_star_normal_momentum_flux(
    const CellState& state,
    Normal2 normal,
    core::Real s_k,
    core::Real s_star) {
    const core::Real un = normal_velocity(state, normal);
    const core::Real denominator = s_k - s_star;
    if (denominator == 0.0) {
        return advective_normal_momentum_flux(state, normal);
    }
    const core::Real h_star = state.conserved.h * (s_k - un) / denominator;
    const core::Real momentum_star = h_star * s_star;
    return advective_normal_momentum_flux(state, normal) + s_k * (momentum_star - state.conserved.h * un);
}

}  // namespace detail

[[nodiscard]] SCAU_HD inline WaveSpeeds estimate_hllc_wave_speeds(
    const CellState& left,
    const CellState& right,
    Normal2 normal,
    core::Real gravity = 9.81) {
    const core::Real left_un = normal_velocity(left, normal);
    const core::Real right_un = normal_velocity(right, normal);
    const core::Real left_c = detail::wave_celerity(left, gravity);
    const core::Real right_c = detail::wave_celerity(right, gravity);
    const core::Real s_l = hd::min_(left_un - left_c, right_un - right_c);
    const core::Real s_r = hd::max_(left_un + left_c, right_un + right_c);

    const core::Real numerator = right.conserved.h * right_un * (s_r - right_un) -
                                 left.conserved.h * left_un * (s_l - left_un) +
                                 detail::physical_normal_momentum_flux(left, normal, gravity) -
                                 detail::physical_normal_momentum_flux(right, normal, gravity);
    const core::Real denominator = right.conserved.h * (s_r - right_un) - left.conserved.h * (s_l - left_un);
    const core::Real s_star = denominator != 0.0 ? numerator / denominator : 0.0;

    return WaveSpeeds{.s_l = s_l, .s_star = s_star, .s_r = s_r};
}

[[nodiscard]] SCAU_HD inline EdgeFlux hllc_normal_flux(
    const CellState& left,
    const CellState& right,
    const EdgeDpmFields& edge_fields,
    Normal2 normal,
    core::Real h_min = 1.0e-8) {
    if (classify_edge(edge_fields.omega_edge, edge_fields.phi_e_n).advective_flux_zeroed) {
        return EdgeFlux{};
    }

    const auto pair = reconstruct_hydrostatic_pair(left, right);
    if (pair.left.conserved.h <= h_min || pair.right.conserved.h <= h_min) {
        return EdgeFlux{};
    }

    const auto speeds = estimate_hllc_wave_speeds(pair.left, pair.right, normal);

    core::Real mass = 0.0;
    core::Real momentum_n = 0.0;
    core::Real tangential = 0.0;
    if (0.0 <= speeds.s_l) {
        mass = detail::physical_normal_mass_flux(pair.left, normal);
        momentum_n = detail::advective_normal_momentum_flux(pair.left, normal);
        tangential = detail::tangential_velocity(pair.left, normal);
    } else if (speeds.s_l <= 0.0 && 0.0 <= speeds.s_star) {
        mass = detail::hllc_star_normal_mass_flux(pair.left, normal, speeds.s_l, speeds.s_star);
        momentum_n = detail::hllc_star_normal_momentum_flux(pair.left, normal, speeds.s_l, speeds.s_star);
        tangential = detail::tangential_velocity(pair.left, normal);
    } else if (speeds.s_star <= 0.0 && 0.0 <= speeds.s_r) {
        mass = detail::hllc_star_normal_mass_flux(pair.right, normal, speeds.s_r, speeds.s_star);
        momentum_n = detail::hllc_star_normal_momentum_flux(pair.right, normal, speeds.s_r, speeds.s_star);
        tangential = detail::tangential_velocity(pair.right, normal);
    } else {
        mass = detail::physical_normal_mass_flux(pair.right, normal);
        momentum_n = detail::advective_normal_momentum_flux(pair.right, normal);
        tangential = detail::tangential_velocity(pair.right, normal);
    }

    const auto tangent = detail::tangent_from_normal(normal);
    const core::Real scaled_mass = edge_fields.phi_e_n * mass;
    const core::Real scaled_momentum_n = edge_fields.phi_e_n * momentum_n;
    const core::Real scaled_momentum_t = scaled_mass * tangential;

    return EdgeFlux{
        .mass = scaled_mass,
        .momentum_n = scaled_momentum_n,
        .momentum_x = scaled_momentum_n * normal.x + scaled_momentum_t * tangent.x,
        .momentum_y = scaled_momentum_n * normal.y + scaled_momentum_t * tangent.y,
    };
}

}  // namespace scau::surface2d

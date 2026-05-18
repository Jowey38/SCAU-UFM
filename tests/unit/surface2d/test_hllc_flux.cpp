#include <gtest/gtest.h>

#include "surface2d/dpm/fields.hpp"
#include "surface2d/riemann/hllc.hpp"
#include "surface2d/state/state.hpp"

TEST(HllcFlux, NormalVelocityProjectsMomentum) {
    const scau::surface2d::CellState state{.conserved = {.h = 2.0, .hu = 6.0, .hv = 8.0}, .eta = 2.0};

    EXPECT_EQ(scau::surface2d::normal_velocity(state, scau::surface2d::Normal2{.x = 1.0, .y = 0.0}), 3.0);
    EXPECT_EQ(scau::surface2d::normal_velocity(state, scau::surface2d::Normal2{.x = 0.0, .y = 1.0}), 4.0);
}

TEST(HllcFlux, ZeroVelocityHydrostaticStateHasZeroMassFluxAndPressureMomentum) {
    const scau::surface2d::CellState left{.conserved = {.h = 1.0, .hu = 0.0, .hv = 0.0}, .eta = 1.0};
    const scau::surface2d::CellState right{.conserved = {.h = 1.0, .hu = 0.0, .hv = 0.0}, .eta = 1.0};
    const scau::surface2d::EdgeDpmFields edge_fields{.phi_e_n = 1.0, .omega_edge = 1.0};

    const auto flux = scau::surface2d::hllc_normal_flux(
        left,
        right,
        edge_fields,
        scau::surface2d::Normal2{.x = 1.0, .y = 0.0});

    EXPECT_EQ(flux.mass, 0.0);
    EXPECT_DOUBLE_EQ(flux.momentum_n, 4.905);
}

TEST(HllcFlux, WaveSpeedsBracketLeftAndRightNormalVelocities) {
    const scau::surface2d::CellState left{.conserved = {.h = 4.0, .hu = 8.0, .hv = 0.0}, .eta = 4.0};
    const scau::surface2d::CellState right{.conserved = {.h = 1.0, .hu = -1.0, .hv = 0.0}, .eta = 1.0};

    const auto normal = scau::surface2d::Normal2{.x = 1.0, .y = 0.0};
    const auto speeds = scau::surface2d::estimate_hllc_wave_speeds(left, right, normal);

    EXPECT_LT(speeds.s_l, scau::surface2d::normal_velocity(left, normal));
    EXPECT_GT(speeds.s_r, scau::surface2d::normal_velocity(right, normal));
    EXPECT_LE(speeds.s_l, speeds.s_star);
    EXPECT_LE(speeds.s_star, speeds.s_r);
}

TEST(HllcFlux, NormalMomentumFluxDependsOnVelocityDirection) {
    const scau::surface2d::CellState still_left{.conserved = {.h = 1.0, .hu = 0.0, .hv = 0.0}, .eta = 1.0};
    const scau::surface2d::CellState still_right{.conserved = {.h = 1.0, .hu = 0.0, .hv = 0.0}, .eta = 1.0};
    const scau::surface2d::CellState moving_left{.conserved = {.h = 1.0, .hu = 2.0, .hv = 0.0}, .eta = 1.0};
    const scau::surface2d::CellState moving_right{.conserved = {.h = 1.0, .hu = 2.0, .hv = 0.0}, .eta = 1.0};
    const scau::surface2d::EdgeDpmFields edge_fields{.phi_e_n = 1.0, .omega_edge = 1.0};
    const auto normal = scau::surface2d::Normal2{.x = 1.0, .y = 0.0};

    const auto still_flux = scau::surface2d::hllc_normal_flux(still_left, still_right, edge_fields, normal);
    const auto moving_flux = scau::surface2d::hllc_normal_flux(moving_left, moving_right, edge_fields, normal);

    EXPECT_GT(moving_flux.momentum_n, still_flux.momentum_n);
    EXPECT_GT(moving_flux.mass, still_flux.mass);
}

TEST(HllcFlux, HardAndSoftBlocksHaveZeroMassFlux) {
    const scau::surface2d::CellState left{.conserved = {.h = 1.0, .hu = 1.0, .hv = 0.0}, .eta = 1.0};
    const scau::surface2d::CellState right{.conserved = {.h = 1.0, .hu = 1.0, .hv = 0.0}, .eta = 1.0};
    const auto normal = scau::surface2d::Normal2{.x = 1.0, .y = 0.0};

    const auto hard_block = scau::surface2d::hllc_normal_flux(
        left,
        right,
        scau::surface2d::EdgeDpmFields{.phi_e_n = 1.0, .omega_edge = 0.0},
        normal);
    const auto soft_block = scau::surface2d::hllc_normal_flux(
        left,
        right,
        scau::surface2d::EdgeDpmFields{.phi_e_n = 0.5e-12, .omega_edge = 1.0},
        normal);

    EXPECT_EQ(hard_block.mass, 0.0);
    EXPECT_EQ(soft_block.mass, 0.0);
}

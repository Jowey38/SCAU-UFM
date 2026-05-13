#include <gtest/gtest.h>

#include "surface2d/dpm/fields.hpp"
#include "surface2d/riemann/hllc.hpp"
#include "surface2d/state/state.hpp"

TEST(HllcFlux, NormalVelocityProjectsMomentum) {
    const scau::surface2d::CellState state{.conserved = {.h = 2.0, .hu = 6.0, .hv = 8.0}, .eta = 2.0};

    EXPECT_EQ(scau::surface2d::normal_velocity(state, scau::surface2d::Normal2{.x = 1.0, .y = 0.0}), 3.0);
    EXPECT_EQ(scau::surface2d::normal_velocity(state, scau::surface2d::Normal2{.x = 0.0, .y = 1.0}), 4.0);
}

TEST(HllcFlux, ZeroVelocityHydrostaticStateHasZeroMassFlux) {
    const scau::surface2d::CellState left{.conserved = {.h = 1.0, .hu = 0.0, .hv = 0.0}, .eta = 1.0};
    const scau::surface2d::CellState right{.conserved = {.h = 1.0, .hu = 0.0, .hv = 0.0}, .eta = 1.0};
    const scau::surface2d::EdgeDpmFields edge_fields{.phi_e_n = 1.0, .omega_edge = 1.0};

    const auto flux = scau::surface2d::hllc_normal_flux(
        left,
        right,
        edge_fields,
        scau::surface2d::Normal2{.x = 1.0, .y = 0.0});

    EXPECT_EQ(flux.mass, 0.0);
    EXPECT_EQ(flux.momentum_n, 0.0);
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

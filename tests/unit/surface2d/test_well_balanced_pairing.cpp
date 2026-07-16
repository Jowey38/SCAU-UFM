#include <gtest/gtest.h>

#include "surface2d/source_terms/well_balanced.hpp"

namespace {
constexpr double kG = 9.81;
}

TEST(WellBalancedPairing, UniformFlatReducesToPurePressure) {
    const auto p = scau::surface2d::well_balanced_edge_pairing(1.0, 1.0, 2.0, 2.0, 2.0, 2.0, kG);
    EXPECT_NEAR(p.pressure_flux, 0.5 * kG * 4.0, 1e-12);
    EXPECT_NEAR(p.s_phi_t_left, 0.0, 1e-12);
    EXPECT_NEAR(p.s_topo_left, 0.0, 1e-12);
    EXPECT_NEAR(p.left_normal, -0.5 * kG * 4.0, 1e-12);
    EXPECT_NEAR(p.right_normal, -0.5 * kG * 4.0, 1e-12);
}

TEST(WellBalancedPairing, PhiTJumpFlatReducesToLocalPorosityPressure) {
    const double h = 1.0;
    const auto p = scau::surface2d::well_balanced_edge_pairing(1.0, 1.25, h, h, h, h, kG);
    EXPECT_NEAR(p.left_normal, -0.5 * kG * 1.0 * h * h, 1e-12);
    EXPECT_NEAR(p.right_normal, -0.5 * kG * 1.25 * h * h, 1e-12);
}

TEST(WellBalancedPairing, SlopingBedReducesToLocalCellDepthPressure) {
    const double phi = 1.0;
    const double h_left = 2.0;
    const double h_right = 1.0;
    const double h_left_star = 0.8;
    const double h_right_star = 0.8;
    const double h_bar = 0.5 * (h_left_star + h_right_star);
    const auto p = scau::surface2d::well_balanced_edge_pairing(
        phi, phi, h_left, h_right, h_left_star, h_right_star, kG);
    EXPECT_NEAR(p.left_normal, -0.5 * kG * phi * h_left * h_left, 1e-12);
    EXPECT_NEAR(p.right_normal, -0.5 * kG * phi * h_right * h_right, 1e-12);
    EXPECT_NEAR(p.pressure_flux, 0.5 * kG * phi * h_bar * h_bar, 1e-12);
}

TEST(WellBalancedPairing, CombinedJumpAndSlopeReducesToLocalConstant) {
    const auto p = scau::surface2d::well_balanced_edge_pairing(
        0.8, 1.0, 1.5, 0.9, 0.7, 0.7, kG);
    EXPECT_NEAR(p.left_normal, -0.5 * kG * 0.8 * 1.5 * 1.5, 1e-12);
    EXPECT_NEAR(p.right_normal, -0.5 * kG * 1.0 * 0.9 * 0.9, 1e-12);
}

TEST(WellBalancedPairing, BoundaryPressureIsPhiTScaled) {
    EXPECT_NEAR(scau::surface2d::well_balanced_boundary_pressure(1.0, 2.0, kG), 0.5 * kG * 4.0, 1e-12);
    EXPECT_NEAR(scau::surface2d::well_balanced_boundary_pressure(0.5, 2.0, kG), 0.5 * kG * 0.5 * 4.0, 1e-12);
}

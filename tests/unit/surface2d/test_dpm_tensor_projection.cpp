#include <cmath>
#include <stdexcept>

#include <gtest/gtest.h>

#include "core/types.hpp"
#include "surface2d/dpm/tensor_projection.hpp"

namespace {

constexpr scau::core::Vector2 kNormalX{.x = 1.0, .y = 0.0};
constexpr scau::core::Vector2 kNormalY{.x = 0.0, .y = 1.0};

scau::surface2d::Tensor2Symmetric isotropic(double lambda) {
    return scau::surface2d::Tensor2Symmetric{.xx = lambda, .xy = 0.0, .yy = lambda};
}

}  // namespace

TEST(DpmTensorProjection, EdgeAverageIsArithmeticMean) {
    const auto avg = scau::surface2d::edge_average_tensor(
        scau::surface2d::Tensor2Symmetric{.xx = 2.0, .xy = 0.5, .yy = 4.0},
        scau::surface2d::Tensor2Symmetric{.xx = 4.0, .xy = -0.5, .yy = 6.0});
    EXPECT_NEAR(avg.xx, 3.0, 1.0e-15);
    EXPECT_NEAR(avg.xy, 0.0, 1.0e-15);
    EXPECT_NEAR(avg.yy, 5.0, 1.0e-15);
}

TEST(DpmTensorProjection, NormalProjectionPicksTensorComponent) {
    const scau::surface2d::Tensor2Symmetric phi_c{.xx = 2.0, .xy = 0.3, .yy = 5.0};
    EXPECT_NEAR(scau::surface2d::project_normal(phi_c, kNormalX), 2.0, 1.0e-15);
    EXPECT_NEAR(scau::surface2d::project_normal(phi_c, kNormalY), 5.0, 1.0e-15);
}

TEST(DpmTensorProjection, TangentialProjectionRotatesNormalByNinetyDegrees) {
    const scau::surface2d::Tensor2Symmetric phi_c{.xx = 2.0, .xy = 0.3, .yy = 5.0};
    // t for kNormalX is (0, 1) => yy component.
    EXPECT_NEAR(scau::surface2d::project_tangential(phi_c, kNormalX), 5.0, 1.0e-15);
    EXPECT_NEAR(scau::surface2d::project_tangential(phi_c, kNormalY), 2.0, 1.0e-15);
}

TEST(DpmTensorProjection, NonUnitNormalIsNormalizedInternally) {
    const scau::surface2d::Tensor2Symmetric phi_c{.xx = 3.0, .xy = 0.0, .yy = 7.0};
    const scau::core::Vector2 scaled{.x = 5.0, .y = 0.0};
    EXPECT_NEAR(scau::surface2d::project_normal(phi_c, scaled), 3.0, 1.0e-15);
}

TEST(DpmTensorProjection, IsotropicTensorHasZeroAnisotropy) {
    const scau::core::Vector2 diagonal{.x = 1.0, .y = 1.0};
    EXPECT_NEAR(scau::surface2d::edge_normal_anisotropy_metric(isotropic(2.5), diagonal), 0.0, 1.0e-15);
    EXPECT_NEAR(scau::surface2d::edge_normal_anisotropy_metric(isotropic(2.5), kNormalX), 0.0, 1.0e-15);
}

TEST(DpmTensorProjection, AnisotropyMetricMatchesClosedForm) {
    // Phi_c = diag(1, 9): eigenvalues 1 and 9, n along x.
    // metric = |1 - 5| / 9 = 4/9.
    const scau::surface2d::Tensor2Symmetric phi_c{.xx = 1.0, .xy = 0.0, .yy = 9.0};
    EXPECT_NEAR(
        scau::surface2d::edge_normal_anisotropy_metric(phi_c, kNormalX),
        4.0 / 9.0,
        1.0e-15);
}

TEST(DpmTensorProjection, IsotropicEdgeHasNoWeakGuarantee) {
    const auto projection = scau::surface2d::project_edge_conveyance(
        isotropic(2.0),
        isotropic(4.0),
        kNormalX,
        1.0);
    // Edge tensor = isotropic(3); phi_e_n = 1.0 * 3 = 3.
    EXPECT_NEAR(projection.phi_e_n, 3.0, 1.0e-15);
    EXPECT_NEAR(projection.phi_e_t, 3.0, 1.0e-15);
    EXPECT_NEAR(projection.anisotropy_metric, 0.0, 1.0e-15);
    EXPECT_FALSE(projection.weak_dpm_guarantee);
}

TEST(DpmTensorProjection, StronglyAnisotropicEdgeFlagsWeakGuarantee) {
    const scau::surface2d::Tensor2Symmetric phi_c{.xx = 1.0, .xy = 0.0, .yy = 9.0};
    const auto projection = scau::surface2d::project_edge_conveyance(phi_c, phi_c, kNormalX, 1.0);
    // metric = 4/9 ~= 0.444 > 0.3.
    EXPECT_GT(projection.anisotropy_metric, scau::surface2d::DpmAnisotropyWarnThreshold);
    EXPECT_TRUE(projection.weak_dpm_guarantee);
    EXPECT_NEAR(projection.phi_e_n, 1.0, 1.0e-15);
}

TEST(DpmTensorProjection, OmegaScalesNormalConveyance) {
    const auto projection = scau::surface2d::project_edge_conveyance(
        isotropic(2.0),
        isotropic(2.0),
        kNormalX,
        0.25);
    EXPECT_NEAR(projection.phi_e_n, 0.5, 1.0e-15);
    // Tangential projection is not omega-scaled.
    EXPECT_NEAR(projection.phi_e_t, 2.0, 1.0e-15);
}

TEST(DpmTensorProjection, ConservativeRuleTakesMinimumNormalForm) {
    const scau::surface2d::Tensor2Symmetric soft{.xx = 1.0, .xy = 0.0, .yy = 5.0};
    const scau::surface2d::Tensor2Symmetric stiff{.xx = 8.0, .xy = 0.0, .yy = 5.0};
    const double phi_e_n = scau::surface2d::project_edge_conveyance_conservative(soft, stiff, kNormalX, 0.5);
    // min(1, 8) = 1, scaled by omega 0.5.
    EXPECT_NEAR(phi_e_n, 0.5, 1.0e-15);
}

TEST(DpmTensorProjection, InvalidTensorFailsClosed) {
    const scau::surface2d::Tensor2Symmetric indefinite{.xx = 1.0, .xy = 5.0, .yy = 1.0};
    EXPECT_THROW(
        static_cast<void>(scau::surface2d::project_normal(indefinite, kNormalX)),
        std::invalid_argument);
    const scau::surface2d::Tensor2Symmetric zero{.xx = 0.0, .xy = 0.0, .yy = 0.0};
    EXPECT_THROW(
        static_cast<void>(scau::surface2d::tensor_max_eigenvalue(zero)),
        std::invalid_argument);
}

TEST(DpmTensorProjection, InvalidNormalAndOmegaFailClosed) {
    const auto phi_c = isotropic(2.0);
    const scau::core::Vector2 zero_normal{.x = 0.0, .y = 0.0};
    EXPECT_THROW(
        static_cast<void>(scau::surface2d::project_normal(phi_c, zero_normal)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::surface2d::project_edge_conveyance(phi_c, phi_c, kNormalX, 1.5)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::surface2d::project_edge_conveyance(phi_c, phi_c, kNormalX, -0.1)),
        std::invalid_argument);
}

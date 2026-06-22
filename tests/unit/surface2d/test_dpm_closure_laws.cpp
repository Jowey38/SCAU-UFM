#include <limits>
#include <stdexcept>

#include <gtest/gtest.h>

#include "surface2d/dpm/closure_laws.hpp"
#include "surface2d/dpm/tensor_projection.hpp"

namespace {

scau::surface2d::Tensor2Symmetric tensor(double xx, double xy, double yy) {
    return scau::surface2d::Tensor2Symmetric{.xx = xx, .xy = xy, .yy = yy};
}

}  // namespace

TEST(DpmClosureLaws, AcceptsConsistentParameters) {
    EXPECT_NO_THROW(
        scau::surface2d::validate_dpm_cell_consistency(0.8, tensor(0.5, 0.1, 0.6)));
    EXPECT_TRUE(scau::surface2d::is_dpm_cell_consistent(0.8, tensor(0.5, 0.1, 0.6)));
}

TEST(DpmClosureLaws, AcceptsDefaultIdentityWithUnitPhiT) {
    EXPECT_NO_THROW(scau::surface2d::validate_dpm_cell_consistency(1.0, tensor(1.0, 0.0, 1.0)));
}

TEST(DpmClosureLaws, RejectsPhiTBelowMaxDiagonal) {
    EXPECT_THROW(
        scau::surface2d::validate_dpm_cell_consistency(0.4, tensor(0.5, 0.0, 0.6)),
        std::invalid_argument);
    EXPECT_FALSE(scau::surface2d::is_dpm_cell_consistent(0.4, tensor(0.5, 0.0, 0.6)));
}

TEST(DpmClosureLaws, RejectsPhiTAboveOne) {
    EXPECT_THROW(
        scau::surface2d::validate_dpm_cell_consistency(1.2, tensor(0.5, 0.0, 0.5)),
        std::invalid_argument);
}

TEST(DpmClosureLaws, RejectsNonPositiveDiagonal) {
    EXPECT_THROW(
        scau::surface2d::validate_dpm_cell_consistency(0.5, tensor(-0.1, 0.0, 0.3)),
        std::invalid_argument);
}

TEST(DpmClosureLaws, RejectsNonPositiveDefinite) {
    // det = 0.5*0.5 - 0.5*0.5 = 0.
    EXPECT_THROW(
        scau::surface2d::validate_dpm_cell_consistency(0.9, tensor(0.5, 0.5, 0.5)),
        std::invalid_argument);
}

TEST(DpmClosureLaws, RejectsMaxEigenvalueAboveOne) {
    // diag-ish with lambda_max = 1.25.
    EXPECT_THROW(
        scau::surface2d::validate_dpm_cell_consistency(1.0, tensor(0.95, 0.3, 0.95)),
        std::invalid_argument);
}

TEST(DpmClosureLaws, RejectsTooSmallMinEigenvalue) {
    // lambda_min ~ 1e-7 < DpmMinEigenvalue.
    EXPECT_THROW(
        scau::surface2d::validate_dpm_cell_consistency(0.6, tensor(1.0e-7, 0.0, 0.5)),
        std::invalid_argument);
}

TEST(DpmClosureLaws, RejectsLargeConditionNumber) {
    // lambda_min = 1e-5 (>= 1e-6), lambda_max = 0.5 -> condition 5e4 > 1e4.
    EXPECT_THROW(
        scau::surface2d::validate_dpm_cell_consistency(0.6, tensor(1.0e-5, 0.0, 0.5)),
        std::invalid_argument);
}

TEST(DpmClosureLaws, RejectsNonFiniteInputs) {
    const double inf = std::numeric_limits<double>::infinity();
    EXPECT_THROW(
        scau::surface2d::validate_dpm_cell_consistency(inf, tensor(0.5, 0.0, 0.5)),
        std::invalid_argument);
    EXPECT_FALSE(scau::surface2d::is_dpm_cell_consistent(0.8, tensor(0.5, inf, 0.6)));
}

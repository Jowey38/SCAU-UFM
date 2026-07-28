#include <gtest/gtest.h>

#include <limits>

#include "core/error.hpp"
#include "stcf/schema.hpp"
#include "stcf/validate.hpp"

namespace {

using scau::core::ScauError;
using scau::stcf::make_uniform_dataset;
using scau::stcf::SoilParamsEntry;
using scau::stcf::validate_stcf_dataset;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

TEST(StcfValidate, RejectsWrongSchemaVersion) {
    auto dataset = make_uniform_dataset(2, 2);
    dataset.schema_version = 4;
    EXPECT_THROW(validate_stcf_dataset(dataset, 2, 2), ScauError);
}

TEST(StcfValidate, RejectsEmptyShapes) {
    const auto dataset = make_uniform_dataset(2, 2);
    EXPECT_THROW(validate_stcf_dataset(dataset, 0, 2), ScauError);
    EXPECT_THROW(validate_stcf_dataset(dataset, 2, 0), ScauError);
}

TEST(StcfValidate, RejectsCrossVectorSizeMismatch) {
    auto dataset = make_uniform_dataset(3, 3);
    dataset.cells.manning_n.pop_back();
    EXPECT_THROW(validate_stcf_dataset(dataset, 3, 3), ScauError);

    auto edge_mismatch = make_uniform_dataset(3, 3);
    edge_mismatch.edges.phi_et.push_back(1.0);
    EXPECT_THROW(validate_stcf_dataset(edge_mismatch, 3, 3), ScauError);
}

TEST(StcfValidate, RejectsPhiTOutsideUnitInterval) {
    auto zero = make_uniform_dataset(2, 2);
    zero.cells.phi_t[0] = 0.0;
    EXPECT_THROW(validate_stcf_dataset(zero, 2, 2), ScauError);

    auto above_one = make_uniform_dataset(2, 2);
    above_one.cells.phi_t[1] = 1.25;
    EXPECT_THROW(validate_stcf_dataset(above_one, 2, 2), ScauError);

    auto not_finite = make_uniform_dataset(2, 2);
    not_finite.cells.phi_t[0] = kNaN;
    EXPECT_THROW(validate_stcf_dataset(not_finite, 2, 2), ScauError);
}

TEST(StcfValidate, RejectsNonPositiveDiagonal) {
    auto dataset = make_uniform_dataset(2, 2);
    dataset.cells.phi_xx[0] = 0.0;
    EXPECT_THROW(validate_stcf_dataset(dataset, 2, 2), ScauError);
}

TEST(StcfValidate, RejectsNonPositiveDeterminant) {
    auto dataset = make_uniform_dataset(2, 2);
    dataset.cells.phi_xx[0] = 0.5;
    dataset.cells.phi_yy[0] = 0.5;
    dataset.cells.phi_xy[0] = 0.5;
    EXPECT_THROW(validate_stcf_dataset(dataset, 2, 2), ScauError);
}

TEST(StcfValidate, RejectsLambdaMaxAboveOne) {
    auto dataset = make_uniform_dataset(2, 2);
    dataset.cells.phi_xx[0] = 0.9;
    dataset.cells.phi_yy[0] = 0.9;
    dataset.cells.phi_xy[0] = 0.15;
    EXPECT_THROW(validate_stcf_dataset(dataset, 2, 2), ScauError);
}

TEST(StcfValidate, RejectsLambdaMinBelowEpsilonPhi) {
    auto dataset = make_uniform_dataset(2, 2);
    dataset.cells.phi_xx[0] = 1.0e-7;
    dataset.cells.phi_yy[0] = 0.5;
    dataset.cells.phi_xy[0] = 0.0;
    EXPECT_THROW(validate_stcf_dataset(dataset, 2, 2), ScauError);
}

TEST(StcfValidate, RejectsConditionNumberAboveConfiguredLimit) {
    auto dataset = make_uniform_dataset(2, 2);
    dataset.cells.phi_xx[0] = 1.0e-5;
    dataset.cells.phi_yy[0] = 0.5;
    dataset.cells.phi_xy[0] = 0.0;
    EXPECT_THROW(validate_stcf_dataset(dataset, 2, 2), ScauError);
}

TEST(StcfValidate, RejectsPhiTBelowMaxDiagonal) {
    auto dataset = make_uniform_dataset(2, 2);
    dataset.cells.phi_t[0] = 0.5;
    dataset.cells.phi_xx[0] = 0.9;
    dataset.cells.phi_yy[0] = 0.4;
    EXPECT_THROW(validate_stcf_dataset(dataset, 2, 2), ScauError);
}

TEST(StcfValidate, RejectsNegativeManningAndNonFiniteBed) {
    auto negative_manning = make_uniform_dataset(2, 2);
    negative_manning.cells.manning_n[0] = -0.01;
    EXPECT_THROW(validate_stcf_dataset(negative_manning, 2, 2), ScauError);

    auto bad_bed = make_uniform_dataset(2, 2);
    bad_bed.cells.z_b[1] = kNaN;
    EXPECT_THROW(validate_stcf_dataset(bad_bed, 2, 2), ScauError);
}

TEST(StcfValidate, RejectsEdgeFieldsOutsideUnitInterval) {
    auto omega = make_uniform_dataset(2, 2);
    omega.edges.omega_edge[0] = 1.5;
    EXPECT_THROW(validate_stcf_dataset(omega, 2, 2), ScauError);

    auto phi_e_n = make_uniform_dataset(2, 2);
    phi_e_n.edges.phi_e_n[1] = -0.1;
    EXPECT_THROW(validate_stcf_dataset(phi_e_n, 2, 2), ScauError);

    auto phi_et = make_uniform_dataset(2, 2);
    phi_et.edges.phi_et[0] = 1.1;
    EXPECT_THROW(validate_stcf_dataset(phi_et, 2, 2), ScauError);
}

TEST(StcfValidate, AcceptsHardAndSoftBlockEdgeValues) {
    auto dataset = make_uniform_dataset(2, 3);
    dataset.edges.omega_edge[0] = 0.0;
    dataset.edges.phi_e_n[0] = 0.0;
    dataset.edges.phi_et[0] = 0.0;
    dataset.edges.omega_edge[1] = 5.0e-5;
    dataset.edges.phi_e_n[1] = 5.0e-3;
    EXPECT_NO_THROW(validate_stcf_dataset(dataset, 2, 3));
}

TEST(StcfValidate, RejectsSoilTableViolations) {
    auto empty_lut = make_uniform_dataset(2, 2);
    empty_lut.soil_params.clear();
    EXPECT_THROW(validate_stcf_dataset(empty_lut, 2, 2), ScauError);

    auto oversized_lut = make_uniform_dataset(2, 2);
    const SoilParamsEntry oversized_entry = oversized_lut.soil_params[0];
    oversized_lut.soil_params.assign(17, oversized_entry);
    EXPECT_THROW(validate_stcf_dataset(oversized_lut, 2, 2), ScauError);

    auto out_of_range = make_uniform_dataset(2, 2);
    out_of_range.cells.soil_type[1] = 3;
    EXPECT_THROW(validate_stcf_dataset(out_of_range, 2, 2), ScauError);

    auto zero_psi_f = make_uniform_dataset(2, 2);
    zero_psi_f.soil_params[0].psi_f = 0.0;
    EXPECT_THROW(validate_stcf_dataset(zero_psi_f, 2, 2), ScauError);

    auto negative_k_s = make_uniform_dataset(2, 2);
    negative_k_s.soil_params[0].K_s = -1.0e-6;
    EXPECT_THROW(validate_stcf_dataset(negative_k_s, 2, 2), ScauError);

    auto theta_order = make_uniform_dataset(2, 2);
    theta_order.soil_params[0].theta_i = 0.5;
    theta_order.soil_params[0].theta_s = 0.4;
    EXPECT_THROW(validate_stcf_dataset(theta_order, 2, 2), ScauError);

    auto theta_above_one = make_uniform_dataset(2, 2);
    theta_above_one.soil_params[0].theta_s = 1.2;
    EXPECT_THROW(validate_stcf_dataset(theta_above_one, 2, 2), ScauError);
}

TEST(StcfValidate, ReportsStageAndIndexInMessage) {
    auto dataset = make_uniform_dataset(3, 2);
    dataset.cells.phi_t[2] = -1.0;
    try {
        validate_stcf_dataset(dataset, 3, 2);
        FAIL() << "expected ScauError";
    } catch (const ScauError& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("phi_t"), std::string::npos);
        EXPECT_NE(message.find("index 2"), std::string::npos);
    }
}

}  // namespace

#include <stdexcept>

#include <gtest/gtest.h>

#include "surface2d/source_terms/runoff/soil_params.hpp"

namespace {

using scau::surface2d::SoilParams;
using scau::surface2d::SoilParamsLUT;

SoilParamsLUT make_valid_lut() {
    SoilParamsLUT lut;
    lut.entries.push_back(SoilParams{.k_s = 1.0e-5, .psi_f = 0.1, .theta_s = 0.4, .theta_i = 0.1});
    lut.entries.push_back(SoilParams{.k_s = 3.0e-6, .psi_f = 0.2, .theta_s = 0.45, .theta_i = 0.2});
    return lut;
}

}  // namespace

TEST(SoilParamsLUT, AtReturnsEntry) {
    const auto lut = make_valid_lut();
    EXPECT_EQ(lut.at(0).k_s, 1.0e-5);
    EXPECT_EQ(lut.at(1).psi_f, 0.2);
}

TEST(SoilParamsLUT, AtOutOfRangeFailsClosed) {
    const auto lut = make_valid_lut();
    EXPECT_THROW(static_cast<void>(lut.at(2)), std::out_of_range);
}

TEST(SoilParamsLUT, ValidLutPasses) {
    const auto lut = make_valid_lut();
    EXPECT_NO_THROW(scau::surface2d::validate_soil_params_lut(lut));
}

TEST(SoilParams, ZeroSuctionFailsClosed) {
    EXPECT_THROW(
        scau::surface2d::validate_soil_params(SoilParams{.k_s = 1.0e-5, .psi_f = 0.0, .theta_s = 0.4, .theta_i = 0.1}),
        std::invalid_argument);
}

TEST(SoilParams, MoistureOrderingFailsClosed) {
    EXPECT_THROW(
        scau::surface2d::validate_soil_params(SoilParams{.k_s = 1.0e-5, .psi_f = 0.1, .theta_s = 0.1, .theta_i = 0.4}),
        std::invalid_argument);
}

TEST(SoilParams, SaturationAboveOneFailsClosed) {
    EXPECT_THROW(
        scau::surface2d::validate_soil_params(SoilParams{.k_s = 1.0e-5, .psi_f = 0.1, .theta_s = 1.2, .theta_i = 0.1}),
        std::invalid_argument);
}

TEST(SoilParamsLUT, EmptyLutFailsClosed) {
    EXPECT_THROW(scau::surface2d::validate_soil_params_lut(SoilParamsLUT{}), std::invalid_argument);
}

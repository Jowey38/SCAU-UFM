#include <stdexcept>

#include <gtest/gtest.h>

#include "coupling/core/state.hpp"

TEST(CouplingFlowLimit, ComputesCanonicalVolumeAndFlowLimit) {
    const scau::coupling::core::ExchangeCellState cell{
        .volume = 40.0,
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
    };

    const auto limit = scau::coupling::core::compute_flow_limit(cell, 4.0);

    EXPECT_DOUBLE_EQ(limit.v_limit, 36.0);
    EXPECT_DOUBLE_EQ(limit.q_limit, 9.0);
}

TEST(CouplingFlowLimit, ZeroStorageInputsProduceZeroLimits) {
    const scau::coupling::core::ExchangeCellState zero_phi_t{
        .volume = 0.0,
        .phi_t = 0.0,
        .h = 2.0,
        .area = 50.0,
    };
    const scau::coupling::core::ExchangeCellState zero_h{
        .volume = 0.0,
        .phi_t = 0.4,
        .h = 0.0,
        .area = 50.0,
    };
    const scau::coupling::core::ExchangeCellState zero_area{
        .volume = 0.0,
        .phi_t = 0.4,
        .h = 2.0,
        .area = 0.0,
    };

    EXPECT_DOUBLE_EQ(scau::coupling::core::compute_flow_limit(zero_phi_t, 4.0).v_limit, 0.0);
    EXPECT_DOUBLE_EQ(scau::coupling::core::compute_flow_limit(zero_phi_t, 4.0).q_limit, 0.0);
    EXPECT_DOUBLE_EQ(scau::coupling::core::compute_flow_limit(zero_h, 4.0).v_limit, 0.0);
    EXPECT_DOUBLE_EQ(scau::coupling::core::compute_flow_limit(zero_h, 4.0).q_limit, 0.0);
    EXPECT_DOUBLE_EQ(scau::coupling::core::compute_flow_limit(zero_area, 4.0).v_limit, 0.0);
    EXPECT_DOUBLE_EQ(scau::coupling::core::compute_flow_limit(zero_area, 4.0).q_limit, 0.0);
}

TEST(CouplingFlowLimit, RejectsNonPositiveDtSub) {
    const scau::coupling::core::ExchangeCellState cell{
        .volume = 40.0,
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
    };

    EXPECT_THROW(static_cast<void>(scau::coupling::core::compute_flow_limit(cell, 0.0)), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(scau::coupling::core::compute_flow_limit(cell, -1.0)), std::invalid_argument);
}

TEST(CouplingFlowLimit, RejectsNegativeStorageInputs) {
    const scau::coupling::core::ExchangeCellState negative_phi_t{
        .volume = 0.0,
        .phi_t = -0.1,
        .h = 2.0,
        .area = 50.0,
    };
    const scau::coupling::core::ExchangeCellState negative_h{
        .volume = 0.0,
        .phi_t = 0.4,
        .h = -2.0,
        .area = 50.0,
    };
    const scau::coupling::core::ExchangeCellState negative_area{
        .volume = 0.0,
        .phi_t = 0.4,
        .h = 2.0,
        .area = -50.0,
    };

    EXPECT_THROW(static_cast<void>(scau::coupling::core::compute_flow_limit(negative_phi_t, 4.0)), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(scau::coupling::core::compute_flow_limit(negative_h, 4.0)), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(scau::coupling::core::compute_flow_limit(negative_area, 4.0)), std::invalid_argument);
}

#include <stdexcept>

#include <gtest/gtest.h>

#include "coupling/core/state.hpp"

namespace {

scau::coupling::core::ExchangeCellState make_cell() {
    return scau::coupling::core::ExchangeCellState{
        .volume = 40.0,
        .mass_deficit_account = {},
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
    };
}

scau::coupling::core::ExchangeDecision make_decision(double v_granted, double v_repay = 0.0) {
    return scau::coupling::core::ExchangeDecision{
        .q_granted = 0.0,
        .v_granted = v_granted,
        .q_repay = 0.0,
        .v_repay = v_repay,
        .v_unmet = 0.0,
    };
}

}  // namespace

TEST(CouplingDrainSplit, KeepsSingleMicroStepWellBelowThreshold) {
    const auto cell = make_cell();
    const auto decision = make_decision(4.0);

    const auto split = scau::coupling::core::split_drain(cell, decision, 4.0);

    EXPECT_EQ(split.micro_steps, 1);
    EXPECT_DOUBLE_EQ(split.dt_micro, 4.0);
    EXPECT_DOUBLE_EQ(split.v_per_micro_step, 4.0);
}

TEST(CouplingDrainSplit, KeepsSingleMicroStepAtExactThreshold) {
    const auto cell = make_cell();
    const auto decision = make_decision(8.0);

    const auto split = scau::coupling::core::split_drain(cell, decision, 4.0);

    EXPECT_EQ(split.micro_steps, 1);
    EXPECT_DOUBLE_EQ(split.dt_micro, 4.0);
    EXPECT_DOUBLE_EQ(split.v_per_micro_step, 8.0);
}

TEST(CouplingDrainSplit, SplitsIntoTwoMicroStepsSlightlyAboveThreshold) {
    const auto cell = make_cell();
    const auto decision = make_decision(10.0);

    const auto split = scau::coupling::core::split_drain(cell, decision, 4.0);

    EXPECT_EQ(split.micro_steps, 2);
    EXPECT_DOUBLE_EQ(split.dt_micro, 2.0);
    EXPECT_DOUBLE_EQ(split.v_per_micro_step, 5.0);
}

TEST(CouplingDrainSplit, CapsMicroStepsAtFive) {
    const auto cell = make_cell();
    const auto decision = make_decision(60.0);

    const auto split = scau::coupling::core::split_drain(cell, decision, 4.0);

    EXPECT_EQ(split.micro_steps, 5);
    EXPECT_DOUBLE_EQ(split.dt_micro, 0.8);
    EXPECT_DOUBLE_EQ(split.v_per_micro_step, 12.0);
}

TEST(CouplingDrainSplit, ReportsTrivialMicroStepForZeroGrantedVolume) {
    const auto cell = make_cell();
    const auto decision = make_decision(0.0);

    const auto split = scau::coupling::core::split_drain(cell, decision, 4.0);

    EXPECT_EQ(split.micro_steps, 1);
    EXPECT_DOUBLE_EQ(split.dt_micro, 4.0);
    EXPECT_DOUBLE_EQ(split.v_per_micro_step, 0.0);
}

TEST(CouplingDrainSplit, IncludesRepaymentVolumeInAppliedVolumeThreshold) {
    const auto cell = make_cell();
    const auto decision = make_decision(4.0, 8.0);

    const auto split = scau::coupling::core::split_drain(cell, decision, 4.0);

    EXPECT_EQ(split.micro_steps, 2);
    EXPECT_DOUBLE_EQ(split.dt_micro, 2.0);
    EXPECT_DOUBLE_EQ(split.v_per_micro_step, 6.0);
}

TEST(CouplingDrainSplit, SplitsRepaymentOnlyAppliedVolumeAboveThreshold) {
    const auto cell = make_cell();
    const auto decision = make_decision(0.0, 10.0);

    const auto split = scau::coupling::core::split_drain(cell, decision, 4.0);

    EXPECT_EQ(split.micro_steps, 2);
    EXPECT_DOUBLE_EQ(split.dt_micro, 2.0);
    EXPECT_DOUBLE_EQ(split.v_per_micro_step, 5.0);
}

TEST(CouplingDrainSplit, RejectsInvalidInputs) {
    const auto cell = make_cell();
    const auto valid_decision = make_decision(4.0);

    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::split_drain(cell, valid_decision, 0.0)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::split_drain(cell, valid_decision, -1.0)),
        std::invalid_argument);

    auto negative_phi_t = cell;
    negative_phi_t.phi_t = -0.1;
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::split_drain(negative_phi_t, valid_decision, 4.0)),
        std::invalid_argument);

    auto negative_h = cell;
    negative_h.h = -1.0;
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::split_drain(negative_h, valid_decision, 4.0)),
        std::invalid_argument);

    auto negative_area = cell;
    negative_area.area = -1.0;
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::split_drain(negative_area, valid_decision, 4.0)),
        std::invalid_argument);

    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::split_drain(cell, make_decision(-0.1), 4.0)),
        std::invalid_argument);

    auto zero_storage = cell;
    zero_storage.phi_t = 0.0;
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::split_drain(zero_storage, make_decision(1.0), 4.0)),
        std::invalid_argument);
}

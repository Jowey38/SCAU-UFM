#include <stdexcept>

#include <gtest/gtest.h>

#include "coupling/core/state.hpp"

namespace {

scau::coupling::core::ExchangeCellState make_cell(double phi_t = 0.4, double h = 2.0, double area = 50.0) {
    return scau::coupling::core::ExchangeCellState{
        .volume = 0.0,
        .mass_deficit_account = {},
        .phi_t = phi_t,
        .h = h,
        .area = area,
    };
}

scau::coupling::core::ExchangeDecision make_decision(double q_granted, double v_granted, double v_unmet = 0.0) {
    return scau::coupling::core::ExchangeDecision{
        .q_granted = q_granted,
        .v_granted = v_granted,
        .q_repay = 1.5,
        .v_repay = 6.0,
        .v_unmet = v_unmet,
    };
}

}  // namespace

TEST(CouplingNonnegativeStorageBackoff, LeavesDecisionBelowAvailableStorageUnchanged) {
    const auto cell = make_cell();
    const auto decision = make_decision(5.0, 20.0, 2.0);

    const auto enforced = scau::coupling::core::enforce_nonnegative_storage(cell, decision, 4.0);

    EXPECT_DOUBLE_EQ(enforced.q_granted, 5.0);
    EXPECT_DOUBLE_EQ(enforced.v_granted, 20.0);
    EXPECT_DOUBLE_EQ(enforced.q_repay, 1.5);
    EXPECT_DOUBLE_EQ(enforced.v_repay, 6.0);
    EXPECT_DOUBLE_EQ(enforced.v_unmet, 2.0);
}

TEST(CouplingNonnegativeStorageBackoff, LeavesDecisionAtAvailableStorageUnchanged) {
    const auto cell = make_cell();
    const auto decision = make_decision(10.0, 40.0, 3.0);

    const auto enforced = scau::coupling::core::enforce_nonnegative_storage(cell, decision, 4.0);

    EXPECT_DOUBLE_EQ(enforced.q_granted, 10.0);
    EXPECT_DOUBLE_EQ(enforced.v_granted, 40.0);
    EXPECT_DOUBLE_EQ(enforced.q_repay, 1.5);
    EXPECT_DOUBLE_EQ(enforced.v_repay, 6.0);
    EXPECT_DOUBLE_EQ(enforced.v_unmet, 3.0);
}

TEST(CouplingNonnegativeStorageBackoff, ClampsGrantAboveAvailableStorageAndAccruesAdditionalUnmet) {
    const auto cell = make_cell();
    const auto decision = make_decision(12.5, 50.0, 4.0);

    const auto enforced = scau::coupling::core::enforce_nonnegative_storage(cell, decision, 4.0);

    EXPECT_DOUBLE_EQ(enforced.q_granted, 10.0);
    EXPECT_DOUBLE_EQ(enforced.v_granted, 40.0);
    EXPECT_DOUBLE_EQ(enforced.q_repay, 1.5);
    EXPECT_DOUBLE_EQ(enforced.v_repay, 6.0);
    EXPECT_DOUBLE_EQ(enforced.v_unmet, 14.0);
}

TEST(CouplingNonnegativeStorageBackoff, ZeroAvailableStorageMovesAllGrantedVolumeToUnmet) {
    const auto cell = make_cell(0.0, 2.0, 50.0);
    const auto decision = make_decision(2.0, 8.0, 1.0);

    const auto enforced = scau::coupling::core::enforce_nonnegative_storage(cell, decision, 4.0);

    EXPECT_DOUBLE_EQ(enforced.q_granted, 0.0);
    EXPECT_DOUBLE_EQ(enforced.v_granted, 0.0);
    EXPECT_DOUBLE_EQ(enforced.q_repay, 1.5);
    EXPECT_DOUBLE_EQ(enforced.v_repay, 6.0);
    EXPECT_DOUBLE_EQ(enforced.v_unmet, 9.0);
}

TEST(CouplingNonnegativeStorageBackoff, RejectsInvalidInputs) {
    const auto cell = make_cell();
    const auto decision = make_decision(5.0, 20.0, 0.0);

    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::enforce_nonnegative_storage(cell, decision, 0.0)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::enforce_nonnegative_storage(cell, decision, -1.0)),
        std::invalid_argument);

    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::enforce_nonnegative_storage(make_cell(-0.1, 2.0, 50.0), decision, 4.0)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::enforce_nonnegative_storage(make_cell(0.4, -1.0, 50.0), decision, 4.0)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::enforce_nonnegative_storage(make_cell(0.4, 2.0, -1.0), decision, 4.0)),
        std::invalid_argument);

    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::enforce_nonnegative_storage(
            cell, make_decision(-0.1, 20.0, 0.0), 4.0)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::enforce_nonnegative_storage(
            cell, make_decision(5.0, -0.1, 0.0), 4.0)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::enforce_nonnegative_storage(
            cell,
            scau::coupling::core::ExchangeDecision{
                .q_granted = 5.0,
                .v_granted = 20.0,
                .q_repay = -0.1,
                .v_repay = 6.0,
                .v_unmet = 0.0,
            },
            4.0)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::enforce_nonnegative_storage(
            cell,
            scau::coupling::core::ExchangeDecision{
                .q_granted = 5.0,
                .v_granted = 20.0,
                .q_repay = 1.5,
                .v_repay = -0.1,
                .v_unmet = 0.0,
            },
            4.0)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(scau::coupling::core::enforce_nonnegative_storage(
            cell, make_decision(5.0, 20.0, -0.1), 4.0)),
        std::invalid_argument);
}

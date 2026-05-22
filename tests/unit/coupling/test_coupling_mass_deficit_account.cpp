#include <stdexcept>

#include <gtest/gtest.h>

#include "coupling/core/state.hpp"

TEST(CouplingMassDeficitAccount, RollsZeroDeficitForwardWithUnmetVolume) {
    const scau::coupling::core::MassDeficitAccount account{};

    const auto updated = scau::coupling::core::roll_deficit(account, 3.5);

    EXPECT_DOUBLE_EQ(updated.volume, 3.5);
}

TEST(CouplingMassDeficitAccount, AccumulatesMultipleUnmetVolumes) {
    const scau::coupling::core::MassDeficitAccount account{.volume = 2.0};

    const auto updated = scau::coupling::core::roll_deficit(account, 1.25);

    EXPECT_DOUBLE_EQ(updated.volume, 3.25);
}

TEST(CouplingMassDeficitAccount, RepaysPartOfDeficit) {
    const scau::coupling::core::MassDeficitAccount account{.volume = 5.0};

    const auto updated = scau::coupling::core::apply_repayment(account, 1.5);

    EXPECT_DOUBLE_EQ(updated.volume, 3.5);
}

TEST(CouplingMassDeficitAccount, ClampsRepaymentAtZero) {
    const scau::coupling::core::MassDeficitAccount account{.volume = 2.0};

    const auto updated = scau::coupling::core::apply_repayment(account, 3.0);

    EXPECT_DOUBLE_EQ(updated.volume, 0.0);
}

TEST(CouplingMassDeficitAccount, RejectsNegativeInputs) {
    const scau::coupling::core::MassDeficitAccount account{.volume = 1.0};

    EXPECT_THROW(static_cast<void>(scau::coupling::core::roll_deficit(account, -0.1)), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(scau::coupling::core::apply_repayment(account, -0.1)), std::invalid_argument);
}

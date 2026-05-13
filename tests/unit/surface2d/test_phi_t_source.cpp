#include <gtest/gtest.h>

#include "surface2d/source_terms/phi_t.hpp"

TEST(PhiTSource, PressurePairingAndSourceCancelExactly) {
    const auto pressure_pairing = scau::surface2d::pressure_pairing_phi_t(1.0, 1.25, 1.0);
    const auto source = scau::surface2d::s_phi_t_centered(1.0, 1.25, 1.0);

    EXPECT_DOUBLE_EQ(pressure_pairing, 1.22625);
    EXPECT_DOUBLE_EQ(source, -1.22625);
    EXPECT_DOUBLE_EQ(pressure_pairing + source, 0.0);
}

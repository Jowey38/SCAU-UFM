#include <gtest/gtest.h>
#include <core/types.hpp>

#include <cstdint>
#include <limits>
#include <type_traits>

namespace {

TEST(CoreTypes, IndexIsAtLeast32BitSigned) {
    static_assert(std::is_signed_v<scau::core::Index>,
                  "Index must be signed to allow -1 sentinel");
    EXPECT_GE(sizeof(scau::core::Index), sizeof(std::int32_t));
}

TEST(CoreTypes, RealIsDoublePrecisionByDefault) {
    static_assert(std::is_same_v<scau::core::Real, double>,
                  "Real must be double on the CPU reference path");
    EXPECT_EQ(std::numeric_limits<scau::core::Real>::digits, 53);
}

TEST(CoreTypes, Vector2DefaultsToZero) {
    scau::core::Vector2 v{};
    EXPECT_DOUBLE_EQ(v.x, 0.0);
    EXPECT_DOUBLE_EQ(v.y, 0.0);
}

TEST(CoreTypes, Vector2BraceInit) {
    scau::core::Vector2 v{1.5, -2.25};
    EXPECT_DOUBLE_EQ(v.x, 1.5);
    EXPECT_DOUBLE_EQ(v.y, -2.25);
}

}  // namespace

#include <gtest/gtest.h>
#include <core/error.hpp>

#include <string>

namespace {

TEST(CoreError, ScauErrorCarriesMessage) {
    try {
        throw scau::core::ScauError("expected message");
    } catch (const scau::core::ScauError& e) {
        EXPECT_STREQ(e.what(), "expected message");
    } catch (...) {
        FAIL() << "Wrong exception type";
    }
}

TEST(CoreError, ScauErrorIsStdException) {
    scau::core::ScauError err("x");
    const std::exception* base = &err;
    EXPECT_NE(base, nullptr);
}

TEST(CoreError, AssertPassesWhenTrue) {
    EXPECT_NO_THROW(SCAU_ASSERT(1 + 1 == 2, "math is broken"));
}

TEST(CoreError, AssertThrowsWhenFalse) {
    EXPECT_THROW(SCAU_ASSERT(false, "intentional failure"),
                 scau::core::ScauError);
}

TEST(CoreError, AssertMessageContainsHint) {
    try {
        SCAU_ASSERT(false, "trigger reason");
        FAIL() << "should have thrown";
    } catch (const scau::core::ScauError& e) {
        std::string msg(e.what());
        EXPECT_NE(msg.find("trigger reason"), std::string::npos);
    }
}

}  // namespace

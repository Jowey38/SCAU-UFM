#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

namespace scau::core {

class ScauError : public std::runtime_error {
public:
    explicit ScauError(const std::string& msg) : std::runtime_error(msg) {}
    explicit ScauError(const char* msg) : std::runtime_error(msg) {}
};

[[noreturn]] void throw_assert_failure(std::string_view condition,
                                       std::string_view message,
                                       const char* file,
                                       int line);

}  // namespace scau::core

#define SCAU_ASSERT(cond, msg)                                                \
    do {                                                                       \
        const bool scau_assert_condition = static_cast<bool>(cond);             \
        if (!scau_assert_condition) {                                           \
            ::scau::core::throw_assert_failure(#cond, (msg), __FILE__,         \
                                                __LINE__);                     \
        }                                                                      \
    } while (false)

#pragma once

#include <cstdint>

namespace scau::core {

using Index = std::int32_t;
using Real = double;

struct Vector2 {
    Real x{0.0};
    Real y{0.0};
};

}  // namespace scau::core

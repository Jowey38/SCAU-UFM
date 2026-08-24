#pragma once

// Host/device portability seam for the shared Surface2D numerics (M284/G9).
// Under nvcc the annotated functions compile for both host and device from
// the SAME source, which is the M280 requirement for a CPU-matchable
// deterministic CUDA backend. In plain C++ translation units the macro is
// empty and everything below is ordinary inline code.

#if defined(__CUDACC__)
#define SCAU_HD __host__ __device__
#else
#define SCAU_HD
#endif

#include <algorithm>
#include <cmath>

#include "core/types.hpp"

namespace scau::surface2d::hd {

// Math shims: on the host these are exactly the std calls the reference CPU
// implementation has always used; on the device they map to the CUDA math
// intrinsics (double sqrt/fabs are IEEE correctly rounded, so those results
// are bitwise identical to the host; pow may differ by ulps and is covered
// by the locked G9 tolerance).

SCAU_HD inline core::Real sqrt_(core::Real x) {
#if defined(__CUDA_ARCH__)
    return ::sqrt(x);
#else
    return std::sqrt(x);
#endif
}

SCAU_HD inline core::Real abs_(core::Real x) {
#if defined(__CUDA_ARCH__)
    return ::fabs(x);
#else
    return std::abs(x);
#endif
}

SCAU_HD inline core::Real pow_(core::Real base, core::Real exponent) {
#if defined(__CUDA_ARCH__)
    return ::pow(base, exponent);
#else
    return std::pow(base, exponent);
#endif
}

SCAU_HD inline bool isfinite_(core::Real x) {
#if defined(__CUDA_ARCH__)
    return ::isfinite(x) != 0;
#else
    return std::isfinite(x);
#endif
}

// Exact std::min/std::max semantics (return the FIRST argument on ties),
// expressed portably so host and device share one definition.
SCAU_HD inline core::Real min_(core::Real a, core::Real b) {
    return b < a ? b : a;
}

SCAU_HD inline core::Real max_(core::Real a, core::Real b) {
    return a < b ? b : a;
}

}  // namespace scau::surface2d::hd

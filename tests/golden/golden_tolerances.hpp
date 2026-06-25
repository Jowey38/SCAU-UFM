#pragma once

namespace scau::golden {

// Main-spec §11 tolerance constants. Change here propagates to all golden tests.
inline constexpr double u_hydro_tol = 1.0e-12;           // m/s
inline constexpr double eta_tol = 1.0e-12;               // m
inline constexpr double conservation_near_tol = 1.0e-9; // recomposed-float anti-flaky

}  // namespace scau::golden

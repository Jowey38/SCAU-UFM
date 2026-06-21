#pragma once

#include "core/types.hpp"
#include "surface2d/dpm/tensor_projection.hpp"

namespace scau::surface2d {

// DPM parameter closure constraints (main-spec validate_dpm_consistency,
// v5.3.2 conditioning guards). These are PreProc/STCF-time physical constraints
// on the static DPM fields, NOT per-step runtime checks: they bound phi_t <= 1
// and lambda_max(Phi_c) <= 1, which intentionally reject the unphysical
// fixtures (phi_t = 1.25, diag(1, 9)) that the solver unit suite uses to
// exercise math. Do not wire this into advance_one_step_cpu.
inline constexpr core::Real DpmMinEigenvalue = 1.0e-6;
inline constexpr core::Real DpmMaxConditionNumber = 1.0e4;

// Fail-closed validation, in main-spec order:
//   1. phi_t finite; Phi_c components finite
//   2. phi_t >= max(Phi_c.xx, Phi_c.yy)
//   3. phi_t <= 1
//   4. Phi_c.xx > 0 and Phi_c.yy > 0
//   5. det(Phi_c) > 0 (positive definite)
//   6. lambda_min(Phi_c) >= DpmMinEigenvalue
//   7. lambda_max(Phi_c) <= 1
//   8. lambda_max / lambda_min <= DpmMaxConditionNumber
// Throws std::invalid_argument on the first violated rule.
void validate_dpm_cell_consistency(core::Real phi_t, const Tensor2Symmetric& phi_c);

// Non-throwing variant for audit/classification paths.
[[nodiscard]] bool is_dpm_cell_consistent(core::Real phi_t, const Tensor2Symmetric& phi_c);

}  // namespace scau::surface2d

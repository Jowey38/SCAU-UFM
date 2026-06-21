#pragma once

#include "core/types.hpp"

namespace scau::surface2d {

// Symmetric 2x2 conveyance tensor Phi_c = [[xx, xy], [xy, yy]].
// Represents the directional in-cell conveyance capacity (transport sovereignty),
// distinct from the storage porosity scalar phi_t (storage sovereignty).
struct Tensor2Symmetric {
    core::Real xx{1.0};
    core::Real xy{0.0};
    core::Real yy{1.0};
};

// Main-spec 5.5.2 anisotropy WARN threshold: edges whose edge-normal anisotropy
// metric exceeds this are marked "DPM consistency weak guarantee" (audit, not block).
inline constexpr core::Real DpmAnisotropyWarnThreshold = 0.3;

// Fail-closed validation: finite components, positive semi-definite, and a
// positive maximum eigenvalue (a conveyance tensor cannot be zero/negative).
void validate_conveyance_tensor(const Tensor2Symmetric& phi_c);

// Edge tensor value uses the locked arithmetic-mean operator (main-spec 5.5.2):
// Phi_c_e = 0.5 * (Phi_c_i + Phi_c_j). Harmonic/geometric means are forbidden.
[[nodiscard]] Tensor2Symmetric edge_average_tensor(
    const Tensor2Symmetric& left,
    const Tensor2Symmetric& right);

// Eigenvalues of a symmetric 2x2 tensor (always real).
[[nodiscard]] core::Real tensor_max_eigenvalue(const Tensor2Symmetric& phi_c);
[[nodiscard]] core::Real tensor_min_eigenvalue(const Tensor2Symmetric& phi_c);

// n^T Phi_c n, projecting onto the (internally normalized) edge normal direction.
[[nodiscard]] core::Real project_normal(const Tensor2Symmetric& phi_c, core::Vector2 normal);

// t^T Phi_c t with t = (-n_y, n_x), the edge tangential projection.
[[nodiscard]] core::Real project_tangential(const Tensor2Symmetric& phi_c, core::Vector2 normal);

// |n^T Phi_c n - 0.5 trace(Phi_c)| / lambda_max(Phi_c) (main-spec 5.5.2).
[[nodiscard]] core::Real edge_normal_anisotropy_metric(
    const Tensor2Symmetric& phi_c,
    core::Vector2 normal);

struct EdgeConveyanceProjection {
    core::Real phi_e_n{0.0};
    core::Real phi_e_t{0.0};
    core::Real anisotropy_metric{0.0};
    bool weak_dpm_guarantee{false};
};

// Primary rule (main-spec 5.3): phi_e_n = omega * n^T Phi_c_e n, with Phi_c_e the
// arithmetic-mean edge tensor. weak_dpm_guarantee is set when the edge-normal
// anisotropy metric of the edge tensor exceeds DpmAnisotropyWarnThreshold.
[[nodiscard]] EdgeConveyanceProjection project_edge_conveyance(
    const Tensor2Symmetric& phi_c_left,
    const Tensor2Symmetric& phi_c_right,
    core::Vector2 normal,
    core::Real omega_edge);

// Degenerate conservative rule (main-spec 5.3): used when geometric phi_e_n is
// unavailable. phi_e_n = omega * min(n^T Phi_c_i n, n^T Phi_c_j n).
[[nodiscard]] core::Real project_edge_conveyance_conservative(
    const Tensor2Symmetric& phi_c_left,
    const Tensor2Symmetric& phi_c_right,
    core::Vector2 normal,
    core::Real omega_edge);

}  // namespace scau::surface2d

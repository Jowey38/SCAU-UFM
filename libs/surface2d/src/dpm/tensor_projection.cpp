#include "surface2d/dpm/tensor_projection.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace scau::surface2d {
namespace {

void validate_tensor(const Tensor2Symmetric& phi_c) {
    validate_conveyance_tensor(phi_c);
}

core::Vector2 unit_normal(core::Vector2 normal) {
    if (!std::isfinite(normal.x) || !std::isfinite(normal.y)) {
        throw std::invalid_argument("edge normal components must be finite");
    }
    const core::Real length = std::sqrt(normal.x * normal.x + normal.y * normal.y);
    if (length <= 0.0) {
        throw std::invalid_argument("edge normal must have positive length");
    }
    return core::Vector2{.x = normal.x / length, .y = normal.y / length};
}

core::Real quadratic_form(const Tensor2Symmetric& phi_c, core::Vector2 direction) {
    return phi_c.xx * direction.x * direction.x +
           2.0 * phi_c.xy * direction.x * direction.y +
           phi_c.yy * direction.y * direction.y;
}

void validate_omega(core::Real omega_edge) {
    if (!std::isfinite(omega_edge) || omega_edge < 0.0 || omega_edge > 1.0) {
        throw std::invalid_argument("omega_edge must be finite and in [0, 1]");
    }
}

}  // namespace

void validate_conveyance_tensor(const Tensor2Symmetric& phi_c) {
    if (!std::isfinite(phi_c.xx) || !std::isfinite(phi_c.xy) || !std::isfinite(phi_c.yy)) {
        throw std::invalid_argument("Phi_c tensor components must be finite");
    }
    // Symmetric 2x2 eigenvalues: tr/2 +/- sqrt(((xx-yy)/2)^2 + xy^2).
    const core::Real half_trace = 0.5 * (phi_c.xx + phi_c.yy);
    const core::Real half_diff = 0.5 * (phi_c.xx - phi_c.yy);
    const core::Real radius = std::sqrt(half_diff * half_diff + phi_c.xy * phi_c.xy);
    const core::Real lambda_min = half_trace - radius;
    const core::Real lambda_max = half_trace + radius;
    if (lambda_max <= 0.0) {
        throw std::invalid_argument("Phi_c tensor must have a positive maximum eigenvalue");
    }
    if (lambda_min < 0.0) {
        throw std::invalid_argument("Phi_c tensor must be positive semi-definite (conveyance >= 0)");
    }
}

Tensor2Symmetric edge_average_tensor(const Tensor2Symmetric& left, const Tensor2Symmetric& right) {
    validate_tensor(left);
    validate_tensor(right);
    return Tensor2Symmetric{
        .xx = 0.5 * (left.xx + right.xx),
        .xy = 0.5 * (left.xy + right.xy),
        .yy = 0.5 * (left.yy + right.yy),
    };
}

core::Real tensor_max_eigenvalue(const Tensor2Symmetric& phi_c) {
    validate_tensor(phi_c);
    const core::Real half_trace = 0.5 * (phi_c.xx + phi_c.yy);
    const core::Real half_diff = 0.5 * (phi_c.xx - phi_c.yy);
    return half_trace + std::sqrt(half_diff * half_diff + phi_c.xy * phi_c.xy);
}

core::Real tensor_min_eigenvalue(const Tensor2Symmetric& phi_c) {
    validate_tensor(phi_c);
    const core::Real half_trace = 0.5 * (phi_c.xx + phi_c.yy);
    const core::Real half_diff = 0.5 * (phi_c.xx - phi_c.yy);
    return half_trace - std::sqrt(half_diff * half_diff + phi_c.xy * phi_c.xy);
}

core::Real project_normal(const Tensor2Symmetric& phi_c, core::Vector2 normal) {
    validate_tensor(phi_c);
    return quadratic_form(phi_c, unit_normal(normal));
}

core::Real project_tangential(const Tensor2Symmetric& phi_c, core::Vector2 normal) {
    validate_tensor(phi_c);
    const core::Vector2 n = unit_normal(normal);
    const core::Vector2 tangent{.x = -n.y, .y = n.x};
    return quadratic_form(phi_c, tangent);
}

core::Real edge_normal_anisotropy_metric(const Tensor2Symmetric& phi_c, core::Vector2 normal) {
    validate_tensor(phi_c);
    const core::Vector2 n = unit_normal(normal);
    const core::Real normal_form = quadratic_form(phi_c, n);
    const core::Real half_trace = 0.5 * (phi_c.xx + phi_c.yy);
    const core::Real lambda_max = tensor_max_eigenvalue(phi_c);
    return std::abs(normal_form - half_trace) / lambda_max;
}

EdgeConveyanceProjection project_edge_conveyance(
    const Tensor2Symmetric& phi_c_left,
    const Tensor2Symmetric& phi_c_right,
    core::Vector2 normal,
    core::Real omega_edge) {
    validate_omega(omega_edge);
    const Tensor2Symmetric phi_c_edge = edge_average_tensor(phi_c_left, phi_c_right);
    const core::Vector2 n = unit_normal(normal);
    const core::Real metric = edge_normal_anisotropy_metric(phi_c_edge, n);
    return EdgeConveyanceProjection{
        .phi_e_n = omega_edge * quadratic_form(phi_c_edge, n),
        .phi_e_t = project_tangential(phi_c_edge, n),
        .anisotropy_metric = metric,
        .weak_dpm_guarantee = metric > DpmAnisotropyWarnThreshold,
    };
}

core::Real project_edge_conveyance_conservative(
    const Tensor2Symmetric& phi_c_left,
    const Tensor2Symmetric& phi_c_right,
    core::Vector2 normal,
    core::Real omega_edge) {
    validate_omega(omega_edge);
    const core::Vector2 n = unit_normal(normal);
    const core::Real left_form = project_normal(phi_c_left, n);
    const core::Real right_form = project_normal(phi_c_right, n);
    return omega_edge * std::min(left_form, right_form);
}

}  // namespace scau::surface2d

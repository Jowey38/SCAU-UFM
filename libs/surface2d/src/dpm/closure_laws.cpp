#include "surface2d/dpm/closure_laws.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace scau::surface2d {

void validate_dpm_cell_consistency(core::Real phi_t, const Tensor2Symmetric& phi_c) {
    if (!std::isfinite(phi_t)) {
        throw std::invalid_argument("phi_t must be finite");
    }
    if (!std::isfinite(phi_c.xx) || !std::isfinite(phi_c.xy) || !std::isfinite(phi_c.yy)) {
        throw std::invalid_argument("Phi_c tensor components must be finite");
    }
    if (phi_t < std::max(phi_c.xx, phi_c.yy)) {
        throw std::invalid_argument("phi_t must be >= max(Phi_c.xx, Phi_c.yy)");
    }
    if (phi_t > 1.0) {
        throw std::invalid_argument("phi_t must be <= 1");
    }
    if (phi_c.xx <= 0.0 || phi_c.yy <= 0.0) {
        throw std::invalid_argument("Phi_c diagonal entries must be positive");
    }
    const core::Real det = phi_c.xx * phi_c.yy - phi_c.xy * phi_c.xy;
    if (det <= 0.0) {
        throw std::invalid_argument("Phi_c must be positive definite");
    }
    const core::Real trace = phi_c.xx + phi_c.yy;
    const core::Real disc = std::sqrt(std::max(0.0, trace * trace - 4.0 * det));
    const core::Real lambda_min = 0.5 * (trace - disc);
    const core::Real lambda_max = 0.5 * (trace + disc);
    if (lambda_min < DpmMinEigenvalue) {
        throw std::invalid_argument("Phi_c minimum eigenvalue is too small");
    }
    if (lambda_max > 1.0) {
        throw std::invalid_argument("Phi_c eigenvalues must be <= 1");
    }
    if (lambda_max / lambda_min > DpmMaxConditionNumber) {
        throw std::invalid_argument("Phi_c condition number is too large");
    }
}

bool is_dpm_cell_consistent(core::Real phi_t, const Tensor2Symmetric& phi_c) {
    try {
        validate_dpm_cell_consistency(phi_t, phi_c);
    } catch (const std::invalid_argument&) {
        return false;
    }
    return true;
}

}  // namespace scau::surface2d

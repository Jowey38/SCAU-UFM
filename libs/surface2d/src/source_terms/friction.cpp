#include "surface2d/source_terms/friction.hpp"

#include <cmath>
#include <stdexcept>

namespace scau::surface2d {

ConservedState apply_manning_friction(
    const ConservedState& conserved,
    core::Real manning_n,
    core::Real dt,
    core::Real h_min,
    core::Real gravity) {
    if (!std::isfinite(manning_n) || manning_n < 0.0) {
        throw std::invalid_argument("manning_n must be finite and non-negative");
    }
    if (!std::isfinite(dt) || dt <= 0.0) {
        throw std::invalid_argument("friction dt must be finite and positive");
    }
    if (!std::isfinite(h_min) || h_min < 0.0) {
        throw std::invalid_argument("friction h_min must be finite and non-negative");
    }
    if (!std::isfinite(gravity) || gravity <= 0.0) {
        throw std::invalid_argument("friction gravity must be finite and positive");
    }
    if (!std::isfinite(conserved.h) || conserved.h < 0.0) {
        throw std::invalid_argument("friction cell depth must be finite and non-negative");
    }
    if (!std::isfinite(conserved.hu) || !std::isfinite(conserved.hv)) {
        throw std::invalid_argument("friction cell momentum must be finite");
    }

    ConservedState updated = conserved;
    if (conserved.h <= h_min) {
        updated.hu = 0.0;
        updated.hv = 0.0;
        return updated;
    }
    if (manning_n == 0.0) {
        return updated;
    }

    const core::Real u = conserved.hu / conserved.h;
    const core::Real v = conserved.hv / conserved.h;
    const core::Real speed = std::sqrt(u * u + v * v);
    if (speed == 0.0) {
        return updated;
    }

    const core::Real h_pow = std::pow(conserved.h, 4.0 / 3.0);
    const core::Real denom = 1.0 + dt * gravity * manning_n * manning_n * speed / h_pow;
    updated.hu /= denom;
    updated.hv /= denom;
    return updated;
}

}  // namespace scau::surface2d

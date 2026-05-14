#pragma once

#include "core/types.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/state/state.hpp"

namespace scau::surface2d {

inline constexpr core::Real PhiEdgeMin = 1.0e-12;

struct Normal2 {
    core::Real x{0.0};
    core::Real y{0.0};
};

struct EdgeFlux {
    core::Real mass{0.0};
    core::Real momentum_n{0.0};
};

[[nodiscard]] core::Real normal_velocity(const CellState& state, Normal2 normal);

[[nodiscard]] EdgeFlux hllc_normal_flux(
    const CellState& left,
    const CellState& right,
    const EdgeDpmFields& edge_fields,
    Normal2 normal,
    core::Real h_min = 1.0e-8);

}  // namespace scau::surface2d

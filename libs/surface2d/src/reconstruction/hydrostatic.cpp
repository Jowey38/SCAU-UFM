#include "surface2d/reconstruction/hydrostatic.hpp"

#include <algorithm>

namespace scau::surface2d {

HydrostaticPair reconstruct_hydrostatic_pair(const CellState& left, const CellState& right) {
    const core::Real eta = std::min(left.eta, right.eta);
    const core::Real left_depth = std::max(0.0, left.conserved.h - (left.eta - eta));
    const core::Real right_depth = std::max(0.0, right.conserved.h - (right.eta - eta));

    return HydrostaticPair{
        .left = CellState{.conserved = {.h = left_depth, .hu = left_depth * left.u(), .hv = left_depth * left.v()}, .eta = eta},
        .right = CellState{.conserved = {.h = right_depth, .hu = right_depth * right.u(), .hv = right_depth * right.v()}, .eta = eta},
    };
}

}  // namespace scau::surface2d

#include "surface2d/reconstruction/hydrostatic.hpp"

#include <algorithm>

namespace scau::surface2d {

HydrostaticPair reconstruct_hydrostatic_pair(const CellState& left, const CellState& right) {
    const core::Real z_b_left = left.eta - left.conserved.h;
    const core::Real z_b_right = right.eta - right.conserved.h;
    const core::Real z_b_star = std::max(z_b_left, z_b_right);
    const core::Real left_depth = std::max(0.0, left.eta - z_b_star);
    const core::Real right_depth = std::max(0.0, right.eta - z_b_star);
    return HydrostaticPair{
        .left = CellState{
            .conserved = {.h = left_depth, .hu = left_depth * left.u(), .hv = left_depth * left.v()},
            .eta = z_b_star + left_depth},
        .right = CellState{
            .conserved = {.h = right_depth, .hu = right_depth * right.u(), .hv = right_depth * right.v()},
            .eta = z_b_star + right_depth},
    };
}

}  // namespace scau::surface2d

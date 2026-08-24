#pragma once

#include "surface2d/portability.hpp"
#include "surface2d/state/state.hpp"

namespace scau::surface2d {

struct HydrostaticPair {
    CellState left;
    CellState right;
};

// Main-spec 5.4 Audusse hydrostatic reconstruction. Header-inline SCAU_HD so
// the deterministic CUDA backend (M284/G9) compiles the exact same kernel
// for the device.
[[nodiscard]] SCAU_HD inline HydrostaticPair reconstruct_hydrostatic_pair(
    const CellState& left,
    const CellState& right) {
    const core::Real z_b_left = left.eta - left.conserved.h;
    const core::Real z_b_right = right.eta - right.conserved.h;
    const core::Real z_b_star = hd::max_(z_b_left, z_b_right);
    const core::Real left_depth = hd::max_(0.0, left.eta - z_b_star);
    const core::Real right_depth = hd::max_(0.0, right.eta - z_b_star);
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

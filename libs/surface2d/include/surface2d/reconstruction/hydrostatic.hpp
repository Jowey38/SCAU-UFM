#pragma once

#include "surface2d/state/state.hpp"

namespace scau::surface2d {

struct HydrostaticPair {
    CellState left;
    CellState right;
};

[[nodiscard]] HydrostaticPair reconstruct_hydrostatic_pair(const CellState& left, const CellState& right);

}  // namespace scau::surface2d

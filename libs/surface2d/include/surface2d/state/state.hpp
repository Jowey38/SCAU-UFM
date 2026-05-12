#pragma once

#include <stdexcept>
#include <vector>

#include "core/types.hpp"
#include "mesh/mesh.hpp"

namespace scau::surface2d {

struct ConservedState {
    core::Real h{0.0};
    core::Real hu{0.0};
    core::Real hv{0.0};
};

struct CellState {
    ConservedState conserved;
};

struct SurfaceState {
    std::vector<CellState> cells;

    [[nodiscard]] static SurfaceState for_mesh(const mesh::Mesh& mesh) {
        SurfaceState state;
        state.cells.resize(mesh.cells.size());
        return state;
    }
};

inline void validate_state_matches_mesh(const SurfaceState& state, const mesh::Mesh& mesh) {
    if (state.cells.size() != mesh.cells.size()) {
        throw std::invalid_argument("surface state cell count must match mesh cell count");
    }
}

}  // namespace scau::surface2d

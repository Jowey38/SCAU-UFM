#pragma once

#include "core/types.hpp"
#include "surface2d/portability.hpp"
#include "surface2d/state/state.hpp"

namespace scau::surface2d {

// Boundary ghost-state builders shared by the CPU reference step and the
// deterministic CUDA backend (M284/G9). Header-inline SCAU_HD so both
// backends compile the exact same ghost construction.

[[nodiscard]] SCAU_HD inline CellState open_boundary_outside_state(const CellState& inside) {
    return CellState{
        .conserved = {.h = inside.conserved.h, .hu = inside.conserved.hu, .hv = inside.conserved.hv},
        .eta = inside.eta,
    };
}

// WaterLevel boundary ghost state: prescribed stage over the inside cell's
// bed (z_b = eta - h), inside velocity carried at the ghost depth. Equal
// stages produce the same reconstructed pair as a lake at rest (zero flux).
[[nodiscard]] SCAU_HD inline CellState water_level_outside_state(const CellState& inside, core::Real eta_bc) {
    const core::Real z_b = inside.eta - inside.conserved.h;
    const core::Real h_out = eta_bc > z_b ? eta_bc - z_b : 0.0;
    return CellState{
        .conserved = {.h = h_out, .hu = h_out * inside.u(), .hv = h_out * inside.v()},
        .eta = z_b + h_out,
    };
}

}  // namespace scau::surface2d
